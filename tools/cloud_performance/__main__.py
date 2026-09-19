"""Package, review and operate one-shot Labrador EC2 performance runs."""

from __future__ import annotations

import argparse
import hashlib
import json
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

from . import analysis, infrastructure, release
from .common import (
    RUN_STARTUP_SLACK_SECONDS,
    canonical,
    deadline,
    inside,
    load_config,
    object_keys,
    run_prefix,
    safe_relative,
    worker_source_info,
)

ROOT = Path(__file__).resolve().parents[2]
WORKER = Path(__file__).with_name("worker.ps1")


def _write_new(path: str | Path, data: bytes) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("xb") as stream:
        stream.write(data)


def _stack_name(run_id: str) -> str:
    return "labrador-performance-" + run_id


def _outputs(stack: dict[str, Any]) -> dict[str, str]:
    return {item["OutputKey"]: item["OutputValue"] for item in stack.get("Outputs", [])}


def _describe_stack(cloudformation: Any, name: str) -> dict[str, Any]:
    return cloudformation.describe_stacks(StackName=name)["Stacks"][0]


def _stack_instance(cloudformation: Any, stack: dict[str, Any]) -> str:
    outputs = _outputs(stack)
    if outputs.get("RunnerInstanceId"):
        return outputs["RunnerInstanceId"]
    pages = cloudformation.get_paginator("list_stack_resources").paginate(StackName=stack["StackId"])
    matches = [resource["PhysicalResourceId"] for page in pages
               for resource in page["StackResourceSummaries"]
               if resource["LogicalResourceId"] == "Runner" and resource.get("PhysicalResourceId")]
    if len(matches) != 1:
        raise RuntimeError("stack does not identify exactly one Runner instance")
    return matches[0]


def _validated_runner(cloudformation: Any, stack_name: str) -> tuple[dict[str, Any], str]:
    prefix = "labrador-performance-"
    if not stack_name.startswith(prefix) or len(stack_name) == len(prefix):
        raise ValueError("refusing a stack outside the Labrador performance namespace")
    run_id = stack_name.removeprefix(prefix)
    stack = _describe_stack(cloudformation, stack_name)
    if stack.get("StackName") != stack_name:
        raise ValueError("resolved stack name differs from the requested exact stack")
    tags = {item["Key"]: item["Value"] for item in stack.get("Tags", [])}
    outputs = _outputs(stack)
    if (tags.get("Project") != "Labrador" or tags.get("RunId") != run_id
            or outputs.get("RunId") != run_id):
        raise ValueError("refusing a stack whose Labrador run identity does not match its name")
    pages = cloudformation.get_paginator("list_stack_resources").paginate(
        StackName=stack["StackId"])
    matches = [resource for page in pages for resource in page["StackResourceSummaries"]
               if resource["LogicalResourceId"] == "Runner"]
    if (len(matches) != 1 or matches[0].get("ResourceType") != "AWS::EC2::Instance"
            or not matches[0].get("PhysicalResourceId")):
        raise ValueError("stack does not contain exactly one EC2 logical Runner")
    instance_id = matches[0]["PhysicalResourceId"]
    if outputs.get("RunnerInstanceId") != instance_id:
        raise ValueError("stack Runner output differs from its logical resource")
    return stack, instance_id


def _missing_stack(exc: Exception) -> bool:
    response = getattr(exc, "response", {})
    return (response.get("Error", {}).get("Code") == "ValidationError"
            and "does not exist" in response.get("Error", {}).get("Message", ""))


def _missing_instance(exc: Exception) -> bool:
    response = getattr(exc, "response", {})
    return response.get("Error", {}).get("Code") == "InvalidInstanceID.NotFound"


def _verify_bucket(s3: Any, config: dict[str, Any]) -> None:
    name = config["artifact_bucket"]
    s3.head_bucket(Bucket=name)
    location = s3.get_bucket_location(Bucket=name).get("LocationConstraint") or "us-east-1"
    if location == "EU":
        location = "eu-west-1"
    if location != config["region"]:
        raise ValueError("artifact bucket must be in the declared run region")
    encryption = s3.get_bucket_encryption(Bucket=name)
    algorithms = {
        item["ApplyServerSideEncryptionByDefault"]["SSEAlgorithm"]
        for item in encryption["ServerSideEncryptionConfiguration"]["Rules"]
    }
    if "AES256" not in algorithms:
        raise ValueError("artifact bucket must have default SSE-S3 encryption")
    if s3.get_bucket_versioning(Bucket=name).get("Status") != "Enabled":
        raise ValueError("artifact bucket versioning must be enabled")
    block = s3.get_public_access_block(Bucket=name)["PublicAccessBlockConfiguration"]
    required = ("BlockPublicAcls", "IgnorePublicAcls", "BlockPublicPolicy", "RestrictPublicBuckets")
    if not all(block.get(field) is True for field in required):
        raise ValueError("artifact bucket must block all public access")


def _verify_ami(ec2: Any, config: dict[str, Any]) -> None:
    images = ec2.describe_images(ImageIds=[config["ami_id"]], Owners=["self"])["Images"]
    if len(images) != 1:
        raise ValueError("AMI is not owned by this account")
    image = images[0]
    if (image.get("State") != "available" or image.get("Architecture") != "x86_64"
            or image.get("Platform") != "windows"
            or image.get("RootDeviceName") != config["root_device_name"]):
        raise ValueError("AMI must be an available x86_64 Windows image with the declared root device")
    tags = {item["Key"]: item["Value"] for item in image.get("Tags", [])}
    unexpected = {key: value for key, value in config["expected_ami_tags"].items()
                  if tags.get(key) != value}
    if unexpected:
        raise ValueError(f"AMI tag identity differs: {sorted(unexpected)}")
    roots = [item.get("Ebs", {}) for item in image.get("BlockDeviceMappings", [])
             if item.get("DeviceName") == config["root_device_name"]]
    if len(roots) != 1 or roots[0].get("Encrypted") is not True:
        raise ValueError("AMI root snapshot must be encrypted")
    offerings = ec2.describe_instance_type_offerings(
        LocationType="availability-zone",
        Filters=[
            {"Name": "instance-type", "Values": [config["instance_type"]]},
            {"Name": "location", "Values": [config["availability_zone"]]},
        ],
    )["InstanceTypeOfferings"]
    if not offerings:
        raise ValueError("declared instance type is not offered in the availability zone")
    details = ec2.describe_instance_types(InstanceTypes=[config["instance_type"]])["InstanceTypes"]
    if (len(details) != 1
            or "x86_64" not in details[0]["ProcessorInfo"]["SupportedArchitectures"]):
        raise ValueError("declared instance type does not support the required architecture")
    vcpu = details[0]["VCpuInfo"]
    valid_cores = vcpu.get("ValidCores") or [vcpu["DefaultCores"]]
    valid_threads = vcpu.get("ValidThreadsPerCore") or [vcpu["DefaultThreadsPerCore"]]
    if (config["cpu_options"]["core_count"] not in valid_cores
            or config["cpu_options"]["threads_per_core"] not in valid_threads):
        raise ValueError("declared CPU options are not valid for the instance type")


def _verify_network(ec2: Any, config: dict[str, Any]) -> None:
    subnets = ec2.describe_subnets(SubnetIds=[config["subnet_id"]])["Subnets"]
    if len(subnets) != 1:
        raise ValueError("declared subnet was not resolved exactly once")
    subnet = subnets[0]
    if (subnet.get("State") != "available" or subnet.get("VpcId") != config["vpc_id"]
            or subnet.get("AvailabilityZone") != config["availability_zone"]):
        raise ValueError("subnet VPC, availability zone or state differs from the declaration")
    vpcs = ec2.describe_vpcs(VpcIds=[config["vpc_id"]])["Vpcs"]
    if len(vpcs) != 1 or vpcs[0].get("State") != "available":
        raise ValueError("declared VPC is unavailable")


def _wait_for_ssm(ssm: Any, instance_id: str, deadline_utc: datetime) -> None:
    while datetime.now(timezone.utc) < deadline_utc:
        records = ssm.describe_instance_information(
            Filters=[{"Key": "InstanceIds", "Values": [instance_id]}]
        )["InstanceInformationList"]
        if len(records) == 1 and records[0].get("PingStatus") == "Online":
            return
        time.sleep(10)
    raise TimeoutError("runner did not become an online SSM managed node before the deadline")


def _powershell_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def worker_dispatch(config: dict[str, Any], instance_id: str, *, config_sha: str,
                    worker_sha: str, template_sha: str,
                    now: datetime | None = None) -> dict[str, Any]:
    """The exact send_command call that starts the worker on the runner.

    Kept whole in one place so the offline suite can assert on what the agent
    will parse. The first launch idled its runner for an hour because the
    worker's location was written as an s3:// URI and nothing before the
    agent looked at it; the second did the same because the corrected form
    existed as a function nobody called.
    """
    keys = object_keys(config)
    arguments = {
        "Bucket": config["artifact_bucket"],
        "ConfigKey": keys["config"],
        "ConfigSHA256": config_sha,
        "BundleKey": keys["bundle"],
        "BundleSHA256": config["bundle"]["sha256"],
        "WorkerSHA256": worker_sha,
        "TemplateKey": keys["template"],
        "TemplateSHA256": template_sha,
        "OutputPrefix": keys["output"],
        "Region": config["region"],
    }
    command_line = (
        "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass "
        "-File .\\worker.ps1 "
        + " ".join(f"-{name} {_powershell_quote(value)}"
                   for name, value in arguments.items())
    )
    current = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
    seconds = max(30, int((deadline(config) - current).total_seconds()))
    return {
        "InstanceIds": [instance_id],
        "DocumentName": "AWS-RunRemoteScript",
        "Comment": f"Labrador performance {config['run_id']}",
        "TimeoutSeconds": min(600, seconds),
        "Parameters": {
            "sourceType": ["S3"],
            "sourceInfo": [json.dumps(worker_source_info(config))],
            "commandLine": [command_line],
            "executionTimeout": [str(seconds)],
        },
        "OutputS3BucketName": config["artifact_bucket"],
        "OutputS3KeyPrefix": keys["ssm"],
    }


def _launch(config_path: str, execute: bool) -> dict[str, Any]:
    config = load_config(config_path)
    bundle_path = inside(ROOT, config["bundle"]["path"], field="bundle.path")
    manifest = release.verify(bundle_path, config["bundle"]["sha256"])
    release.verify_benchmark_payload(manifest, config["workload"]["backends"])
    release.verify_workspace_snapshot(manifest, ROOT)
    actual_release_sha = hashlib.sha256(canonical(manifest)).hexdigest()
    if actual_release_sha != config["bundle"]["release_sha256"]:
        raise ValueError("release manifest hash differs from declaration")
    config_bytes = canonical(config)
    config_sha = hashlib.sha256(config_bytes).hexdigest()
    worker_bytes = WORKER.read_bytes()
    worker_sha = hashlib.sha256(worker_bytes).hexdigest()
    bundled_worker = manifest.get("files", {}).get(
        "source/tools/cloud_performance/worker.ps1", {}).get("sha256")
    if bundled_worker != worker_sha:
        raise ValueError("worker differs from the source snapshot in the release bundle")
    keys = object_keys(config)
    stack_name = _stack_name(config["run_id"])
    rendered = infrastructure.template(config, require_authorization=execute)
    template_bytes = canonical(rendered)
    template_sha = hashlib.sha256(template_bytes).hexdigest()
    plan = {
        "execute": execute,
        "stack_name": stack_name,
        "region": config["region"],
        "uploads": {
            keys["bundle"]: config["bundle"]["sha256"],
            keys["config"]: config_sha,
            keys["worker"]: worker_sha,
            keys["template"]: template_sha,
        },
        "release_sha256": config["bundle"]["release_sha256"],
        "template_sha256": template_sha,
        "instance": {
            "ami_id": config["ami_id"],
            "instance_type": config["instance_type"],
            "cpu_options": config["cpu_options"],
            "deadline_utc": config["authorization"]["deadline_utc"],
        },
        "template": rendered,
    }
    if not execute:
        return plan

    import boto3

    session = boto3.session.Session(region_name=config["region"])
    ec2 = session.client("ec2")
    s3 = session.client("s3")
    cloudformation = session.client("cloudformation")
    ssm = session.client("ssm")
    _verify_bucket(s3, config)
    _verify_ami(ec2, config)
    _verify_network(ec2, config)
    existing_objects = s3.list_objects_v2(
        Bucket=config["artifact_bucket"], Prefix=run_prefix(config) + "/", MaxKeys=1)
    if existing_objects.get("KeyCount", 0) != 0:
        raise ValueError("artifact run prefix already exists; run_id reuse is refused")
    try:
        _describe_stack(cloudformation, stack_name)
    except Exception as exc:
        if not _missing_stack(exc):
            raise
    else:
        raise ValueError("a stack with this run_id already exists; it will not be updated")
    extra = {"ServerSideEncryption": "AES256"}
    s3.upload_file(str(bundle_path), config["artifact_bucket"], keys["bundle"], ExtraArgs=extra)
    s3.put_object(Bucket=config["artifact_bucket"], Key=keys["config"], Body=config_bytes,
                  ServerSideEncryption="AES256", ContentType="application/json")
    s3.put_object(Bucket=config["artifact_bucket"], Key=keys["worker"], Body=worker_bytes,
                  ServerSideEncryption="AES256", ContentType="text/plain")
    s3.put_object(Bucket=config["artifact_bucket"], Key=keys["template"], Body=template_bytes,
                  ServerSideEncryption="AES256", ContentType="application/json")
    stack_id = cloudformation.create_stack(
        StackName=stack_name,
        TemplateBody=template_bytes.decode("utf-8"),
        Capabilities=["CAPABILITY_IAM"],
        OnFailure="DELETE",
        Tags=[{"Key": "Project", "Value": "Labrador"},
              {"Key": "RunId", "Value": config["run_id"]}],
    )["StackId"]
    cloudformation.get_waiter("stack_create_complete").wait(StackName=stack_id)
    stack = _describe_stack(cloudformation, stack_id)
    instance_id = _stack_instance(cloudformation, stack)
    try:
        startup_deadline = min(
            deadline(config),
            datetime.now(timezone.utc) + timedelta(seconds=RUN_STARTUP_SLACK_SECONDS),
        )
        _wait_for_ssm(ssm, instance_id, startup_deadline)
        command_id = ssm.send_command(**worker_dispatch(
            config, instance_id, config_sha=config_sha, worker_sha=worker_sha,
            template_sha=template_sha))["Command"]["CommandId"]
        launch_record = canonical({
            "schema_version": 1,
            "run_id": config["run_id"],
            "stack_id": stack_id,
            "instance_id": instance_id,
            "command_id": command_id,
            "bundle_sha256": config["bundle"]["sha256"],
            "release_sha256": config["bundle"]["release_sha256"],
            "config_sha256": config_sha,
            "worker_sha256": worker_sha,
            "template_sha256": template_sha,
        })
        s3.put_object(Bucket=config["artifact_bucket"], Key=keys["launch"],
                      Body=launch_record, ServerSideEncryption="AES256",
                      ContentType="application/json")
    except Exception:
        ec2.terminate_instances(InstanceIds=[instance_id])
        raise
    return plan | {"stack_id": stack_id, "instance_id": instance_id, "command_id": command_id}


def _status(stack_name: str, region: str) -> dict[str, Any]:
    import boto3

    session = boto3.session.Session(region_name=region)
    cloudformation = session.client("cloudformation")
    stack, instance_id = _validated_runner(cloudformation, stack_name)
    outputs = _outputs(stack)
    reservations = session.client("ec2").describe_instances(InstanceIds=[instance_id])["Reservations"]
    instances = [instance for reservation in reservations for instance in reservation["Instances"]]
    s3 = session.client("s3")
    prefix = outputs["RunPrefix"] + "/output/"
    pages = s3.get_paginator("list_objects_v2").paginate(
        Bucket=outputs["ArtifactBucket"], Prefix=prefix)
    objects = [{"key": item["Key"], "bytes": item["Size"],
                "last_modified": item["LastModified"].isoformat()}
               for page in pages for item in page.get("Contents", [])]
    command: Any = "not_dispatched"
    try:
        launch = json.loads(s3.get_object(
            Bucket=outputs["ArtifactBucket"],
            Key=outputs["RunPrefix"] + "/control/launch.json",
        )["Body"].read())
        command = session.client("ssm").get_command_invocation(
            CommandId=launch["command_id"], InstanceId=launch["instance_id"])
    except Exception as exc:
        response = getattr(exc, "response", {})
        code = response.get("Error", {}).get("Code")
        if code not in ("NoSuchKey", "InvocationDoesNotExist"):
            raise
    return {
        "stack_id": stack["StackId"],
        "stack_status": stack["StackStatus"],
        "outputs": outputs,
        "instance": instances[0] if len(instances) == 1 else {"unexpected_count": len(instances)},
        "command": command,
        "objects": objects,
    }


def _collect(stack_name: str, region: str, output: str | Path) -> dict[str, Any]:
    import boto3

    destination = Path(output).resolve()
    if destination.exists():
        raise FileExistsError(f"refusing to overwrite {destination}")
    session = boto3.session.Session(region_name=region)
    cloudformation = session.client("cloudformation")
    stack, _ = _validated_runner(cloudformation, stack_name)
    values = _outputs(stack)
    prefix = values["RunPrefix"] + "/"
    s3 = session.client("s3")
    pages = s3.get_paginator("list_objects_v2").paginate(
        Bucket=values["ArtifactBucket"], Prefix=prefix)
    objects = [item for page in pages for item in page.get("Contents", [])
               if any(part in item["Key"] for part in ("/control/", "/output/", "/ssm/"))]
    destination.mkdir(parents=True)
    for item in objects:
        relative = item["Key"].removeprefix(prefix)
        path = safe_relative(relative, field="S3 result key")
        local = destination.joinpath(*path.parts)
        local.parent.mkdir(parents=True, exist_ok=True)
        with local.open("xb") as stream:
            s3.download_fileobj(values["ArtifactBucket"], item["Key"], stream)
    return {"stack_id": stack["StackId"], "downloaded": len(objects), "output": str(destination)}


def _stop(stack_name: str, region: str, execute: bool) -> dict[str, Any]:
    plan = {"execute": execute, "stack_name": stack_name, "region": region,
            "action": "terminate the exact stack Runner, then delete the exact stack"}
    if not execute:
        return plan
    import boto3

    session = boto3.session.Session(region_name=region)
    cloudformation = session.client("cloudformation")
    stack, instance_id = _validated_runner(cloudformation, stack_name)
    ec2 = session.client("ec2")
    instance = None
    try:
        reservations = ec2.describe_instances(InstanceIds=[instance_id])["Reservations"]
        instances = [item for reservation in reservations for item in reservation["Instances"]]
        if len(instances) != 1:
            raise ValueError("stack Runner did not resolve to exactly one EC2 instance")
        instance = instances[0]
    except Exception as exc:
        if not _missing_instance(exc):
            raise
    if instance is not None:
        tags = {item["Key"]: item["Value"] for item in instance.get("Tags", [])}
        run_id = _outputs(stack)["RunId"]
        if tags.get("Project") != "Labrador" or tags.get("RunId") != run_id:
            raise ValueError("EC2 Runner tags differ from the exact stack identity")
        state = instance["State"]["Name"]
        if state not in ("shutting-down", "terminated"):
            ec2.terminate_instances(InstanceIds=[instance_id])
        if state != "terminated":
            ec2.get_waiter("instance_terminated").wait(InstanceIds=[instance_id])
    cloudformation.delete_stack(StackName=stack["StackId"])
    return plan | {"stack_id": stack["StackId"], "instance_id": instance_id,
                   "instance_already_gone": instance is None}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    pack = commands.add_parser("bundle", help="package prebuilt executables and source bytes")
    pack.add_argument("--payload", required=True)
    pack.add_argument("--out", required=True)
    pack.add_argument("--root", default=str(ROOT))
    render = commands.add_parser("render", help="render CloudFormation without AWS calls")
    render.add_argument("--config", required=True)
    render.add_argument("--out", required=True)
    launch = commands.add_parser("launch", help="plan or launch one immutable run")
    launch.add_argument("--config", required=True)
    launch.add_argument("--execute", action="store_true")
    status = commands.add_parser("status", help="show exact-stack state and evidence keys")
    status.add_argument("--stack", required=True)
    status.add_argument("--region", default="ap-southeast-2")
    collect = commands.add_parser("collect", help="download exact-stack evidence")
    collect.add_argument("--stack", required=True)
    collect.add_argument("--region", default="ap-southeast-2")
    collect.add_argument("--out", required=True)
    stop = commands.add_parser("stop", help="plan or stop the exact stack")
    stop.add_argument("--stack", required=True)
    stop.add_argument("--region", default="ap-southeast-2")
    stop.add_argument("--execute", action="store_true")
    analyze = commands.add_parser("analyze", help="validate and summarize downloaded evidence")
    analyze.add_argument("--input", required=True)
    analyze.add_argument("--out", required=True)
    args = parser.parse_args()

    if args.command == "bundle":
        result = release.bundle(args.payload, args.out, root=args.root)
    elif args.command == "render":
        config = load_config(args.config)
        rendered = infrastructure.template(config)
        _write_new(args.out, json.dumps(rendered, indent=2).encode("utf-8") + b"\n")
        result = {"template": str(Path(args.out).resolve()), "stack_name": _stack_name(config["run_id"])}
    elif args.command == "launch":
        result = _launch(args.config, args.execute)
    elif args.command == "status":
        result = _status(args.stack, args.region)
    elif args.command == "collect":
        result = _collect(args.stack, args.region, args.out)
    elif args.command == "stop":
        result = _stop(args.stack, args.region, args.execute)
    else:
        result = analysis.analyze(args.input)
        _write_new(args.out, json.dumps(result, indent=2).encode("utf-8") + b"\n")
    print(json.dumps(result, indent=2, default=str))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
