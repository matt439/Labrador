"""Render the inspectable, one-instance CloudFormation run stack."""

from __future__ import annotations

import textwrap
from datetime import datetime
from typing import Any

from .common import deadline, object_keys, run_prefix, validate_config


def template(document: dict[str, Any], *, require_authorization: bool = False,
             now: datetime | None = None, check_time: bool = True) -> dict[str, Any]:
    config = validate_config(document, now=now, check_time=check_time)
    if require_authorization and not config["authorization"]["allow_launch"]:
        raise ValueError("launch authorization is false")
    tags = [
        {"Key": "Project", "Value": "Labrador"},
        {"Key": "Purpose", "Value": "cloud-performance"},
        {"Key": "RunId", "Value": config["run_id"]},
    ]
    prefix = run_prefix(config)
    keys = object_keys(config)
    bucket_arn = f"arn:${{AWS::Partition}}:s3:::{config['artifact_bucket']}"
    input_arn = f"{bucket_arn}/{prefix}/input/*"
    output_arns = [
        f"{bucket_arn}/{prefix}/output/*",
        f"{bucket_arn}/{prefix}/ssm/*",
    ]
    trust = {
        "Version": "2012-10-17",
        "Statement": [{
            "Effect": "Allow",
            "Principal": {"Service": "ec2.amazonaws.com"},
            "Action": "sts:AssumeRole",
        }],
    }
    resources: dict[str, Any] = {
        "RunnerSecurityGroup": {
            "Type": "AWS::EC2::SecurityGroup",
            "Properties": {
                "GroupDescription": f"No-ingress Labrador performance run {config['run_id']}",
                "VpcId": config["vpc_id"],
                "SecurityGroupEgress": [{
                    "IpProtocol": "tcp",
                    "FromPort": 443,
                    "ToPort": 443,
                    "CidrIp": "0.0.0.0/0",
                    "Description": "SSM, S3 and AWS service HTTPS",
                }],
                "Tags": tags,
            },
        },
        "RunnerRole": {
            "Type": "AWS::IAM::Role",
            "Properties": {
                "AssumeRolePolicyDocument": trust,
                "ManagedPolicyArns": [{"Fn::Sub":
                    "arn:${AWS::Partition}:iam::aws:policy/AmazonSSMManagedInstanceCore"}],
                "Policies": [{
                    "PolicyName": "RunEvidence",
                    "PolicyDocument": {
                        "Version": "2012-10-17",
                        "Statement": [
                            {
                                "Effect": "Allow",
                                "Action": "s3:GetBucketLocation",
                                "Resource": {"Fn::Sub": bucket_arn},
                            },
                            {
                                "Effect": "Allow",
                                "Action": "s3:ListBucket",
                                "Resource": {"Fn::Sub": bucket_arn},
                                "Condition": {"StringLike": {
                                    "s3:prefix": [f"{prefix}/*"],
                                }},
                            },
                            {
                                "Effect": "Allow",
                                "Action": ["s3:GetObject", "s3:GetObjectVersion"],
                                "Resource": {"Fn::Sub": input_arn},
                            },
                            {
                                "Effect": "Allow",
                                "Action": ["s3:PutObject", "s3:AbortMultipartUpload"],
                                "Resource": [{"Fn::Sub": arn} for arn in output_arns],
                            },
                        ],
                    },
                }],
                "Tags": tags,
            },
        },
        "RunnerProfile": {
            "Type": "AWS::IAM::InstanceProfile",
            "Properties": {"Roles": [{"Ref": "RunnerRole"}]},
        },
    }
    bootstrap = textwrap.dedent("""\
        <powershell>
        $ErrorActionPreference = 'Stop'
        Set-Service -Name AmazonSSMAgent -StartupType Automatic
        Start-Service -Name AmazonSSMAgent
        </powershell>
        <powershellArguments>-ExecutionPolicy Bypass -NoProfile -NonInteractive</powershellArguments>
        <persist>false</persist>
        """)
    resources["Runner"] = {
        "Type": "AWS::EC2::Instance",
        "Properties": {
            "ImageId": config["ami_id"],
            "InstanceType": config["instance_type"],
            "AvailabilityZone": config["availability_zone"],
            "CpuOptions": {
                "CoreCount": config["cpu_options"]["core_count"],
                "ThreadsPerCore": config["cpu_options"]["threads_per_core"],
            },
            "IamInstanceProfile": {"Ref": "RunnerProfile"},
            "InstanceInitiatedShutdownBehavior": "terminate",
            "BlockDeviceMappings": [{
                "DeviceName": config["root_device_name"],
                "Ebs": {
                    "DeleteOnTermination": True,
                    "Encrypted": True,
                    "VolumeSize": config["root_volume_gib"],
                    "VolumeType": "gp3",
                },
            }],
            "MetadataOptions": {
                "HttpEndpoint": "enabled",
                "HttpTokens": "required",
                "HttpPutResponseHopLimit": 1,
                "InstanceMetadataTags": "disabled",
            },
            "NetworkInterfaces": [{
                "AssociatePublicIpAddress": config["associate_public_ip"],
                "DeleteOnTermination": True,
                "DeviceIndex": "0",
                "GroupSet": [{"Ref": "RunnerSecurityGroup"}],
                "SubnetId": config["subnet_id"],
            }],
            "Monitoring": False,
            "Tags": tags,
            "UserData": {"Fn::Base64": bootstrap},
        },
    }
    watchdog_trust = {
        "Version": "2012-10-17",
        "Statement": [{
            "Effect": "Allow",
            "Principal": {"Service": "lambda.amazonaws.com"},
            "Action": "sts:AssumeRole",
        }],
    }
    resources["WatchdogRole"] = {
        "Type": "AWS::IAM::Role",
        "Properties": {
            "AssumeRolePolicyDocument": watchdog_trust,
            "ManagedPolicyArns": [{"Fn::Sub":
                "arn:${AWS::Partition}:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"}],
            "Policies": [{
                "PolicyName": "TerminateExpiredRunner",
                "PolicyDocument": {
                    "Version": "2012-10-17",
                    "Statement": [{
                        "Effect": "Allow",
                        "Action": "ec2:TerminateInstances",
                        "Resource": {"Fn::Sub":
                            "arn:${AWS::Partition}:ec2:${AWS::Region}:${AWS::AccountId}:instance/${Runner}"},
                        "Condition": {"StringEquals": {
                            "ec2:ResourceTag/Project": "Labrador",
                            "ec2:ResourceTag/RunId": config["run_id"],
                        }},
                    }],
                },
            }],
            "Tags": tags,
        },
    }
    watchdog_code = textwrap.dedent("""\
        import boto3
        import os
        import time

        def handler(event, context):
            if time.time() < float(os.environ["DEADLINE"]):
                return {"expired": False}
            instance_id = os.environ["INSTANCE_ID"]
            boto3.client("ec2").terminate_instances(InstanceIds=[instance_id])
            return {"expired": True, "instance_id": instance_id}
        """)
    resources["Watchdog"] = {
        "Type": "AWS::Lambda::Function",
        "Properties": {
            "Runtime": "python3.12",
            "Handler": "index.handler",
            "Timeout": 15,
            "Role": {"Fn::GetAtt": ["WatchdogRole", "Arn"]},
            "Code": {"ZipFile": watchdog_code},
            "Environment": {"Variables": {
                "DEADLINE": str(deadline(config).timestamp()),
                "INSTANCE_ID": {"Ref": "Runner"},
            }},
            "Tags": tags,
        },
    }
    resources["WatchdogLogGroup"] = {
        "Type": "AWS::Logs::LogGroup",
        "Properties": {
            "LogGroupName": {"Fn::Sub": "/aws/lambda/${Watchdog}"},
            "RetentionInDays": 14,
        },
    }
    resources["WatchdogTick"] = {
        "Type": "AWS::Events::Rule",
        "DependsOn": "WatchdogLogGroup",
        "Properties": {
            "ScheduleExpression": "rate(1 minute)",
            "State": "ENABLED",
            "Targets": [{"Arn": {"Fn::GetAtt": ["Watchdog", "Arn"]},
                         "Id": "deadline-watchdog"}],
        },
    }
    resources["WatchdogPermission"] = {
        "Type": "AWS::Lambda::Permission",
        "Properties": {
            "Action": "lambda:InvokeFunction",
            "FunctionName": {"Ref": "Watchdog"},
            "Principal": "events.amazonaws.com",
            "SourceArn": {"Fn::GetAtt": ["WatchdogTick", "Arn"]},
        },
    }
    return {
        "AWSTemplateFormatVersion": "2010-09-09",
        "Description": f"Labrador one-shot performance reference run {config['run_id']}",
        "Resources": resources,
        "Outputs": {
            "RunId": {"Value": config["run_id"]},
            "RunnerInstanceId": {"Value": {"Ref": "Runner"}},
            "ArtifactBucket": {"Value": config["artifact_bucket"]},
            "RunPrefix": {"Value": prefix},
            "BundleKey": {"Value": keys["bundle"]},
        },
    }
