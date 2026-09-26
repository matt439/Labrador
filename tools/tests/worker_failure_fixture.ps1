param(
    [Parameter(Mandatory = $true)][string]$FixtureRoot,
    [Parameter(Mandatory = $true)][string]$WorkerSource,
    [switch]$WrongIdentity,
    [switch]$UploadFailure
)

$ErrorActionPreference = 'Stop'
$fixture = [IO.Path]::GetFullPath($FixtureRoot)
$workerPath = Join-Path $fixture 'worker.ps1'
$source = [IO.File]::ReadAllText($WorkerSource)
$original = '$workRoot = ''C:\ProgramData\LabradorPerformance\Current'''
if (-not $source.Contains($original)) { throw 'Worker output-root assignment changed' }
$replacement = '$workRoot = ''' + (Join-Path $fixture 'work').Replace("'", "''") + ''''
[IO.File]::WriteAllText($workerPath, $source.Replace($original, $replacement))
$documentPath = Join-Path $fixture 'declaration.json'
$declaration = Get-Content -LiteralPath $documentPath -Raw | ConvertFrom-Json

# Execute the real worker with local I/O and every reached platform/cloud
# boundary stubbed. No network, process launch, or shutdown can reach a host.
function Invoke-RestMethod {
    param($Method, $Uri, $Headers, $TimeoutSec)
    if ($Method -eq 'Put') { return 'offline-token' }
    if ($Uri -ne 'http://169.254.169.254/latest/dynamic/instance-identity/document') {
        throw 'Unexpected metadata request'
    }
    return [pscustomobject]@{
        instanceType = $declaration.instance_type
        imageId = $(if ($WrongIdentity) { 'ami-wrong' } else { $declaration.ami_id })
        region = $declaration.region
        availabilityZone = $declaration.availability_zone
    }
}
function Read-S3Object {
    param($BucketName, $Key, $File, $Region)
    if ($Key -ne 'offline-config') { throw 'Unexpected object request' }
    Copy-Item -LiteralPath $documentPath -Destination $File
}
function Write-S3Object {
    param($BucketName, $Key, $File, $ServerSideEncryption, $Region)
    if ($UploadFailure) { throw 'Offline upload refusal' }
}
function Get-CimInstance {
    param($ClassName)
    throw 'Offline early processor refusal'
}
function shutdown.exe {
    $args | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'shutdown.json')
}

$arguments = @{
    Bucket = 'offline'; ConfigKey = 'offline-config'
    ConfigSHA256 = (Get-FileHash -LiteralPath $documentPath -Algorithm SHA256).Hash
    BundleKey = 'unused'; BundleSHA256 = $declaration.bundle.sha256
    WorkerSHA256 = (Get-FileHash -LiteralPath $workerPath -Algorithm SHA256).Hash
    TemplateKey = 'unused'; TemplateSHA256 = ('c' * 64)
    OutputPrefix = 'unused'; Region = $declaration.region
}
& $workerPath @arguments
exit $LASTEXITCODE
