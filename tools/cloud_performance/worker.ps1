[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Bucket,
    [Parameter(Mandatory = $true)][string]$ConfigKey,
    [Parameter(Mandatory = $true)][string]$ConfigSHA256,
    [Parameter(Mandatory = $true)][string]$BundleKey,
    [Parameter(Mandatory = $true)][string]$BundleSHA256,
    [Parameter(Mandatory = $true)][string]$WorkerSHA256,
    [Parameter(Mandatory = $true)][string]$TemplateKey,
    [Parameter(Mandatory = $true)][string]$TemplateSHA256,
    [Parameter(Mandatory = $true)][string]$OutputPrefix,
    [Parameter(Mandatory = $true)][string]$Region
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$workRoot = 'C:\ProgramData\LabradorPerformance\Current'
$evidenceRoot = Join-Path $workRoot 'evidence'
$success = $false
$failure = $null
$declaredInstance = $false
$config = $null

function Assert-Equal($Actual, $Expected, [string]$Name) {
    if ($Actual -ne $Expected) {
        throw "$Name differs from the declaration: expected '$Expected', got '$Actual'"
    }
}

function Resolve-Inside([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative) -or
        $Relative.Contains('\') -or $Relative.Contains(':') -or
        $Relative.Split('/') -contains '..') {
        throw "Unsafe relative path '$Relative'"
    }
    $base = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath((Join-Path $Root ($Relative.Replace('/', '\'))))
    if (-not $candidate.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes its declared root: '$Relative'"
    }
    return $candidate
}

function Get-ImdsDocument {
    $token = Invoke-RestMethod -Method Put -Uri 'http://169.254.169.254/latest/api/token' `
        -Headers @{'X-aws-ec2-metadata-token-ttl-seconds' = '21600'} -TimeoutSec 2
    return Invoke-RestMethod -Method Get `
        -Uri 'http://169.254.169.254/latest/dynamic/instance-identity/document' `
        -Headers @{'X-aws-ec2-metadata-token' = $token} -TimeoutSec 2
}

function Get-ImdsRoleName {
    $token = Invoke-RestMethod -Method Put -Uri 'http://169.254.169.254/latest/api/token' `
        -Headers @{'X-aws-ec2-metadata-token-ttl-seconds' = '21600'} -TimeoutSec 2
    return Invoke-RestMethod -Method Get `
        -Uri 'http://169.254.169.254/latest/meta-data/iam/security-credentials/' `
        -Headers @{'X-aws-ec2-metadata-token' = $token} -TimeoutSec 2
}

function Convert-VendorId($Value) {
    if ($Value -is [string] -and $Value.StartsWith('0x', [StringComparison]::OrdinalIgnoreCase)) {
        return [Convert]::ToInt64($Value.Substring(2), 16)
    }
    return [int64]$Value
}

# The benchmark cannot run where this worker runs. SSM executes it as SYSTEM
# in session 0, whose window station has no display: DXGI refuses a swap chain
# there (DXGI_ERROR_NOT_CURRENTLY_AVAILABLE), and the two APIs that do not
# refuse present into nothing at a throttled rate. The console session is
# where the NVIDIA display, DWM and a logged-on user are, which is also what a
# player's game gets. A scheduled task with an interactive logon type is the
# supported way for a service to start a process on that desktop, and the
# task's last result is the process exit code.
function Invoke-ConsoleBenchmark([string]$User, [string]$Executable, [string[]]$Arguments,
    [string]$StdoutPath, [string]$StderrPath, [string]$TaskName, [int]$TimeoutSeconds) {
    $quoted = @($Arguments | ForEach-Object { "'" + $_.Replace("'", "''") + "'" }) -join ' '
    $inner = "& '" + $Executable.Replace("'", "''") + "' $quoted 1> '" +
        $StdoutPath.Replace("'", "''") + "' 2> '" + $StderrPath.Replace("'", "''") +
        "'; exit `$LASTEXITCODE"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($inner))
    $action = New-ScheduledTaskAction -Execute 'powershell.exe' `
        -Argument "-NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand $encoded" `
        -WorkingDirectory (Split-Path -Parent $Executable)
    $principal = New-ScheduledTaskPrincipal -UserId $User -LogonType Interactive -RunLevel Limited
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
        -ExecutionTimeLimit (New-TimeSpan -Seconds $TimeoutSeconds) -MultipleInstances IgnoreNew
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
    Register-ScheduledTask -TaskName $TaskName -Action $action -Principal $principal `
        -Settings $settings | Out-Null
    try {
        Start-ScheduledTask -TaskName $TaskName
        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        do {
            Start-Sleep -Milliseconds 500
            $state = [string](Get-ScheduledTask -TaskName $TaskName).State
            $result = [int64](Get-ScheduledTaskInfo -TaskName $TaskName).LastTaskResult
            # 0x41301 is "currently running" and 0x41303 "has not yet run".
            $busy = ($state -eq 'Running') -or ($result -in 267009, 267011)
        } while ($busy -and $stopwatch.Elapsed.TotalSeconds -lt ($TimeoutSeconds + 30))
        if ($busy) {
            Stop-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
            throw "Benchmark task '$TaskName' did not finish within $TimeoutSeconds seconds"
        }
        return $result
    }
    finally {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
    }
}

function Write-Json([string]$Path, $Value) {
    $Value | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Send-Evidence([string]$Root, [string]$Prefix) {
    Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
        if ($_.Name -notin @('success.json', 'failure.json')) {
            $relative = $_.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
            Write-S3Object -BucketName $Bucket -Key "$Prefix/$relative" -File $_.FullName `
                -ServerSideEncryption AES256 -Region $Region | Out-Null
        }
    }
}

function Get-EvidenceManifest([string]$Root) {
    $manifest = [ordered]@{}
    Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
        if ($_.Name -in @('success.json', 'failure.json')) {
            return
        }
        $relative = $_.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
        $manifest[$relative] = [ordered]@{
            bytes = [int64]$_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    return $manifest
}

try {
    New-Item -ItemType Directory -Force -Path $workRoot, $evidenceRoot | Out-Null
    $actualWorkerHash = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-Equal $actualWorkerHash $WorkerSHA256.ToLowerInvariant() 'worker SHA-256'

    $configPath = Join-Path $workRoot 'config.json'
    Read-S3Object -BucketName $Bucket -Key $ConfigKey -File $configPath -Region $Region | Out-Null
    $actualConfigHash = (Get-FileHash -LiteralPath $configPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-Equal $actualConfigHash $ConfigSHA256.ToLowerInvariant() 'configuration SHA-256'
    $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
    Copy-Item -LiteralPath $configPath -Destination (Join-Path $evidenceRoot 'config.json')
    Assert-Equal $config.bundle.sha256.ToLowerInvariant() $BundleSHA256.ToLowerInvariant() 'bundle declaration'
    $identity = Get-ImdsDocument
    Assert-Equal $identity.instanceType $config.instance_type 'instance type'
    Assert-Equal $identity.imageId $config.ami_id 'AMI'
    Assert-Equal $identity.region $config.region 'region'
    Assert-Equal $identity.availabilityZone $config.availability_zone 'availability zone'
    $processors = @(Get-CimInstance -ClassName Win32_Processor)
    $coreCount = ($processors | Measure-Object -Property NumberOfCores -Sum).Sum
    $logicalCount = ($processors | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum
    Assert-Equal ([int]$coreCount) ([int]$config.cpu_options.core_count) 'physical core count'
    Assert-Equal ([int]$logicalCount) `
        ([int]$config.cpu_options.core_count * [int]$config.cpu_options.threads_per_core) `
        'logical processor count'
    $sessionId = [Diagnostics.Process]::GetCurrentProcess().SessionId
    $sessionUser = [Environment]::UserName
    Assert-Equal $sessionId 0 'SSM session ID'
    Assert-Equal $sessionUser 'SYSTEM' 'SSM session user'
    $consoleUser = [string]$config.expected_ami_tags.ConsoleUser
    if ([string]::IsNullOrWhiteSpace($consoleUser)) {
        throw 'The declared image tags name no ConsoleUser for the benchmark to run as'
    }
    # The image logs that user on to the console automatically. Its desktop
    # takes a few seconds after boot, so wait for the shell rather than the
    # session alone; Invoke-ConsoleBenchmark needs both.
    $consoleSessionId = -1
    $consoleWait = [Diagnostics.Stopwatch]::StartNew()
    do {
        $consoleSessionId = -1
        foreach ($line in @(& query.exe session 2>&1)) {
            if ($line -match '^\s*>?console\s+(\S+)\s+(\d+)\s+Active\b') {
                Assert-Equal $Matches[1] $consoleUser 'console session user'
                $consoleSessionId = [int]$Matches[2]
            }
        }
        $shell = @(Get-Process -Name explorer -ErrorAction SilentlyContinue |
            Where-Object { $_.SessionId -eq $consoleSessionId })
        if ($consoleSessionId -ge 0 -and $shell.Count -gt 0) { break }
        Start-Sleep -Seconds 2
    } while ($consoleWait.Elapsed.TotalSeconds -lt 120)
    if ($consoleSessionId -lt 0) {
        throw "No active console session for '$consoleUser'; the benchmark needs an interactive desktop"
    }
    if ($shell.Count -eq 0) {
        throw "The console session for '$consoleUser' has no desktop shell yet"
    }
    $declaredInstance = $true
    if ($env:AWS_ACCESS_KEY_ID -or $env:AWS_SECRET_ACCESS_KEY -or $env:AWS_SESSION_TOKEN -or
        $env:AWS_PROFILE -or (Test-Path -LiteralPath (Join-Path $env:USERPROFILE '.aws\credentials'))) {
        throw 'Static or profile AWS credentials are forbidden on the runner'
    }
    $roleName = [string](Get-ImdsRoleName)
    if ([string]::IsNullOrWhiteSpace($roleName)) {
        throw 'Instance metadata did not name the runner role'
    }
    $caller = Get-STSCallerIdentity -Region $Region
    Assert-Equal $caller.Account $identity.accountId 'AWS caller account'
    if ($caller.Arn -notmatch (':assumed-role/' + [Regex]::Escape($roleName) + '/')) {
        throw 'AWS calls are not using the instance profile role'
    }
    if ($config.authorization.allow_launch -isnot [bool] -or
        -not $config.authorization.allow_launch) {
        throw 'Launch authorization is not the literal boolean true'
    }
    if ([DateTimeOffset]::UtcNow -ge [DateTimeOffset]::Parse($config.authorization.deadline_utc)) {
        throw 'Run deadline expired before measurement started'
    }
    $templatePath = Join-Path $evidenceRoot 'template.json'
    Read-S3Object -BucketName $Bucket -Key $TemplateKey -File $templatePath -Region $Region | Out-Null
    $actualTemplateHash = (Get-FileHash -LiteralPath $templatePath -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-Equal $actualTemplateHash $TemplateSHA256.ToLowerInvariant() 'CloudFormation template SHA-256'

    $bundlePath = Join-Path $workRoot 'payload.zip'
    Read-S3Object -BucketName $Bucket -Key $BundleKey -File $bundlePath -Region $Region | Out-Null
    $actualBundleHash = (Get-FileHash -LiteralPath $bundlePath -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-Equal $actualBundleHash $BundleSHA256.ToLowerInvariant() 'bundle archive SHA-256'
    $extractRoot = Join-Path $workRoot 'bundle'
    if (Test-Path -LiteralPath $extractRoot) {
        throw 'Bundle extraction destination already exists'
    }
    Expand-Archive -LiteralPath $bundlePath -DestinationPath $extractRoot
    $releasePath = Join-Path $extractRoot 'release.json'
    $actualReleaseHash = (Get-FileHash -LiteralPath $releasePath -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-Equal $actualReleaseHash $config.bundle.release_sha256.ToLowerInvariant() `
        'release manifest SHA-256'
    $release = Get-Content -LiteralPath $releasePath -Raw | ConvertFrom-Json
    Assert-Equal ([int]$release.schema_version) 1 'release schema'
    Assert-Equal $release.scope 'labrador-cloud-performance-payload' 'release scope'
    $workerEntries = @($release.files.PSObject.Properties | Where-Object {
        $_.Name -eq 'source/tools/cloud_performance/worker.ps1'
    })
    Assert-Equal $workerEntries.Count 1 'release worker entry count'
    Assert-Equal $workerEntries[0].Value.sha256.ToLowerInvariant() $actualWorkerHash `
        'release worker identity'
    foreach ($entry in $release.files.PSObject.Properties) {
        $file = Resolve-Inside $extractRoot $entry.Name
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "Bundle member is missing: $($entry.Name)"
        }
        Assert-Equal (Get-Item -LiteralPath $file).Length ([int64]$entry.Value.bytes) `
            "bundle member length $($entry.Name)"
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        Assert-Equal $hash $entry.Value.sha256.ToLowerInvariant() "bundle member hash $($entry.Name)"
    }
    Copy-Item -LiteralPath $releasePath -Destination (Join-Path $evidenceRoot 'release.json')

    $video = @(Get-CimInstance -ClassName Win32_VideoController | Select-Object `
        Name, PNPDeviceID, DriverVersion, AdapterRAM, Status, `
        CurrentHorizontalResolution, CurrentVerticalResolution, CurrentRefreshRate, `
        VideoModeDescription)
    if (-not ($video | Where-Object { $_.Name -match 'NVIDIA' })) {
        throw 'No NVIDIA hardware adapter is visible to the worker session'
    }
    $activeDisplays = @($video | Where-Object {
        $_.Name -match 'NVIDIA' -and
        [int]$_.CurrentHorizontalResolution -ge [int]$config.workload.width -and
        [int]$_.CurrentVerticalResolution -ge [int]$config.workload.height -and
        [Math]::Abs([int]$_.CurrentRefreshRate - [int]$config.workload.refresh_hz) -le 1
    })
    if ($activeDisplays.Count -eq 0) {
        throw 'No NVIDIA display exposes the declared resolution and refresh to the worker session'
    }
    $runtime = @('MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll') | ForEach-Object {
        $path = Join-Path $env:WINDIR "System32\$_"
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required Visual C++ runtime is absent: $_"
        }
        $item = Get-Item -LiteralPath $path
        [ordered]@{ name = $_; path = $item.FullName; version = $item.VersionInfo.FileVersion }
    }
    $nvidiaBefore = @()
    $nvidiaCommand = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
    if ($null -eq $nvidiaCommand) {
        throw 'nvidia-smi.exe is absent from the qualified image'
    }
    $nvidiaBefore = @(& $nvidiaCommand.Source `
        '--query-gpu=name,pci.device_id,driver_version,pstate,temperature.gpu,clocks.gr,clocks.mem' `
        '--format=csv,noheader,nounits')
    if ($LASTEXITCODE -ne 0) {
        throw 'nvidia-smi failed before measurement'
    }
    $driverVersions = @($nvidiaBefore | ForEach-Object {
        $fields = @($_ -split ',' | Select-Object -First 3 | ForEach-Object { $_.Trim() })
        if ($fields.Count -ne 3) { throw 'nvidia-smi identity before measurement was malformed' }
        $fields[2]
    } | Select-Object -Unique)
    if ($driverVersions.Count -ne 1) {
        throw 'The runner exposed more than one NVIDIA driver version'
    }
    Assert-Equal $driverVersions[0] $config.expected_ami_tags.NvidiaDriver `
        'NVIDIA driver version'
    & powercfg.exe /SetActive SCHEME_MIN | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not select the High performance Windows power plan'
    }
    $powerPlan = @(& powercfg.exe /GetActiveScheme) -join "`n"
    if ($LASTEXITCODE -ne 0 -or
        $powerPlan -notmatch '(?i)8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c') {
        throw 'The active Windows power plan is not High performance'
    }
    $operatingSystem = Get-CimInstance -ClassName Win32_OperatingSystem
    Assert-Equal ([string]$operatingSystem.BuildNumber) `
        ([string]$config.expected_ami_tags.WindowsBuild) 'Windows build number'
    $computer = Get-CimInstance -ClassName Win32_ComputerSystem
    $hostRecord = [ordered]@{
        schema_version = 1
        run_id = $config.run_id
        captured_utc = [DateTimeOffset]::UtcNow.ToString('o')
        instance = $identity
        cpu_options = [ordered]@{ core_count = [int]$coreCount; logical_processors = [int]$logicalCount }
        cpu = @($processors | Select-Object Name, Manufacturer, NumberOfCores, NumberOfLogicalProcessors,
            MaxClockSpeed)
        memory_bytes = [int64]$computer.TotalPhysicalMemory
        os = [ordered]@{ caption = $operatingSystem.Caption; version = $operatingSystem.Version;
            build_number = $operatingSystem.BuildNumber }
        video = $video
        visual_cpp_runtime = @($runtime)
        power_plan = $powerPlan
        nvidia_smi_before = $nvidiaBefore
        session = [ordered]@{ id = $sessionId; user = $sessionUser; session_name = $env:SESSIONNAME }
        console_session = [ordered]@{ id = $consoleSessionId; user = $consoleUser }
        aws_identity = [ordered]@{
            role_name = $roleName; account_id = $caller.Account; caller_arn = $caller.Arn
        }
        bundle_sha256 = $actualBundleHash
        release_sha256 = $actualReleaseHash
        config_sha256 = $actualConfigHash
        worker_sha256 = $actualWorkerHash
        template_sha256 = $actualTemplateHash
    }
    Write-Json (Join-Path $evidenceRoot 'host.json') $hostRecord

    $resultRoot = Join-Path $evidenceRoot 'results'
    New-Item -ItemType Directory -Path $resultRoot | Out-Null
    # The console user is not an administrator. It reads the payload and
    # writes its result and logs; everything else stays SYSTEM's.
    & icacls.exe $extractRoot /grant "${consoleUser}:(OI)(CI)RX" /T /Q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not grant the console user read access to the payload' }
    & icacls.exe $resultRoot /grant "${consoleUser}:(OI)(CI)M" /Q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not grant the console user write access to the results' }
    $repetitionSeconds = [int][Math]::Ceiling(
        ([int]$config.workload.warmup_frames + [int]$config.workload.sample_frames) /
        [double]$config.workload.refresh_hz) + 120
    foreach ($backend in $config.workload.backends) {
        $executable = Resolve-Inside $extractRoot $backend.executable
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "Benchmark executable is missing: $($backend.executable)"
        }
        for ($repetition = 1; $repetition -le [int]$config.workload.repetitions; $repetition++) {
            if ([DateTimeOffset]::UtcNow -ge [DateTimeOffset]::Parse($config.authorization.deadline_utc)) {
                throw 'Run deadline expired between repetitions'
            }
            $ordinal = '{0:D3}' -f $repetition
            $resultPath = Join-Path $resultRoot "result-$($backend.name)-$ordinal.json"
            $stdoutPath = Join-Path $resultRoot "stdout-$($backend.name)-$ordinal.log"
            $stderrPath = Join-Path $resultRoot "stderr-$($backend.name)-$ordinal.log"
            $arguments = @(
                '--output', $resultPath,
                '--run', [string]$config.run_id,
                '--release-hash', [string]$config.bundle.release_sha256,
                '--warmup', [string]$config.workload.warmup_frames,
                '--sample', [string]$config.workload.sample_frames,
                '--refresh', [string]$config.workload.refresh_hz
            )
            $exitCode = Invoke-ConsoleBenchmark -User $consoleUser -Executable $executable `
                -Arguments $arguments -StdoutPath $stdoutPath -StderrPath $stderrPath `
                -TaskName "LabradorPerformance-$($backend.name)-$ordinal" `
                -TimeoutSeconds $repetitionSeconds
            if ($exitCode -ne 0) {
                throw "Benchmark '$($backend.name)' repetition $repetition exited $exitCode"
            }
            if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
                throw "Benchmark '$($backend.name)' did not write its result"
            }
            $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
            if ([DateTimeOffset]::Parse($result.finished_utc) -ge
                [DateTimeOffset]::Parse($config.authorization.deadline_utc)) {
                throw "Benchmark '$($backend.name)' repetition $repetition crossed the deadline"
            }
            Assert-Equal ([int]$result.schema_version) 1 'benchmark result schema'
            Assert-Equal $result.scope 'linesweeper_frame_benchmark' 'benchmark scope'
            Assert-Equal $result.status 'complete' 'benchmark completion status'
            Assert-Equal $result.run $config.run_id 'benchmark run identity'
            Assert-Equal $result.release_sha256 $config.bundle.release_sha256 `
                'benchmark release identity'
            Assert-Equal $result.build.configuration 'release' 'benchmark build configuration'
            Assert-Equal $result.build.render_backend $backend.name 'benchmark backend'
            Assert-Equal $result.measurement_class 'hardware_raster' 'benchmark measurement class'
            if ($result.render_device.device_name -notmatch $backend.expected_device) {
                throw "Benchmark device '$($result.render_device.device_name)' differs from '$($backend.expected_device)'"
            }
            Assert-Equal ([int]$result.workload.resolution.width) ([int]$config.workload.width) `
                'benchmark width'
            Assert-Equal ([int]$result.workload.resolution.height) ([int]$config.workload.height) `
                'benchmark height'
            Assert-Equal ([int]$result.workload.live_particles) ([int]$config.workload.live_particles) `
                'benchmark live particle count'
            Assert-Equal ([int]$result.workload.warmup_frames) ([int]$config.workload.warmup_frames) `
                'benchmark warmup count'
            Assert-Equal ([int]$result.workload.sample_frames) ([int]$config.workload.sample_frames) `
                'benchmark declared sample count'
            Assert-Equal ([int]$result.workload.refresh_hz) ([int]$config.workload.refresh_hz) `
                'benchmark refresh rate'
            $targetFrameNs = [int64][Math]::Floor((1000000000 + `
                [Math]::Floor(([int]$config.workload.refresh_hz) / 2)) / `
                ([int]$config.workload.refresh_hz))
            Assert-Equal ([int64]$result.workload.target_frame_ns) $targetFrameNs `
                'benchmark target frame interval'
            Assert-Equal (Convert-VendorId $result.render_device.vendor_id) `
                ([int64]$backend.expected_vendor_id) 'benchmark GPU vendor'
            if ($result.render_device.kind -ne 'hardware') {
                throw "Benchmark selected a non-hardware renderer: $($result.render_device.kind)"
            }
            Assert-Equal (@($result.samples).Count) ([int]$config.workload.sample_frames) `
                'benchmark raw sample count'
            Assert-Equal ([int]$result.timing.sample_count) ([int]$config.workload.sample_frames) `
                'benchmark timing sample count'
            Assert-Equal $result.timing.interval_scope `
                'software-paced frame-start interval; not display scan-out' `
                'benchmark interval scope'
            Assert-Equal (@($result.timing.scheduled_interval_ns).Count) `
                ([int]$config.workload.sample_frames) 'benchmark scheduled interval count'
            for ($sampleIndex = 0; $sampleIndex -lt [int]$config.workload.sample_frames; $sampleIndex++) {
                Assert-Equal ([int64]$result.timing.scheduled_interval_ns[$sampleIndex]) `
                    ([int64]$result.samples[$sampleIndex].scheduled_interval_ns) `
                    "benchmark scheduled interval $sampleIndex"
            }
        }
    }
    $nvidiaAfter = @(& $nvidiaCommand.Source `
        '--query-gpu=name,pci.device_id,driver_version,pstate,temperature.gpu,clocks.gr,clocks.mem' `
        '--format=csv,noheader,nounits')
    if ($LASTEXITCODE -ne 0) {
        throw 'nvidia-smi failed after measurement'
    }
    $gpuIdentityBefore = @($nvidiaBefore | ForEach-Object {
        $fields = @($_ -split ',' | Select-Object -First 3 | ForEach-Object { $_.Trim() })
        if ($fields.Count -ne 3) { throw 'nvidia-smi identity before measurement was malformed' }
        $fields -join '|'
    })
    $gpuIdentityAfter = @($nvidiaAfter | ForEach-Object {
        $fields = @($_ -split ',' | Select-Object -First 3 | ForEach-Object { $_.Trim() })
        if ($fields.Count -ne 3) { throw 'nvidia-smi identity after measurement was malformed' }
        $fields -join '|'
    })
    if (@(Compare-Object $gpuIdentityBefore $gpuIdentityAfter).Count -ne 0) {
        throw 'GPU name, PCI device or driver changed during measurement'
    }
    $hostRecord['nvidia_smi_after'] = $nvidiaAfter
    $hostRecord['nvidia_smi_identity'] = $gpuIdentityAfter
    $hostRecord['completed_utc'] = [DateTimeOffset]::UtcNow.ToString('o')
    Write-Json (Join-Path $evidenceRoot 'host.json') $hostRecord
    $success = $true
}
catch {
    $failure = $_.Exception.ToString()
}
finally {
    try {
        $evidenceManifest = Get-EvidenceManifest $evidenceRoot
        Send-Evidence $evidenceRoot $OutputPrefix
        if ($success -and $null -ne $config -and
            [DateTimeOffset]::UtcNow -ge [DateTimeOffset]::Parse($config.authorization.deadline_utc)) {
            $success = $false
            $failure = 'Evidence collection crossed the absolute run deadline.'
        }
        $terminal = [ordered]@{
            schema_version = 1
            status = $(if ($success) { 'success' } else { 'failure' })
            run_id = $(if ($null -ne $config) { $config.run_id } else { 'configuration-unavailable' })
            completed_utc = [DateTimeOffset]::UtcNow.ToString('o')
            bundle_sha256 = $BundleSHA256.ToLowerInvariant()
            release_sha256 = $(if ($null -ne $config) { $config.bundle.release_sha256 } else { $null })
            config_sha256 = $(if ($null -ne $config) { $actualConfigHash } else { $null })
            worker_sha256 = $actualWorkerHash
            template_sha256 = $TemplateSHA256.ToLowerInvariant()
            evidence_files = $evidenceManifest
            error = $failure
        }
        $terminalName = $(if ($success) { 'success.json' } else { 'failure.json' })
        $terminalPath = Join-Path $evidenceRoot $terminalName
        Write-Json $terminalPath $terminal
        Write-S3Object -BucketName $Bucket -Key "$OutputPrefix/$terminalName" `
            -File $terminalPath -ServerSideEncryption AES256 -Region $Region | Out-Null
    }
    catch {
        $success = $false
        Write-Error "Evidence upload failed; no successful terminal marker exists: $($_.Exception.Message)"
    }
    finally {
        if ($declaredInstance) {
            & shutdown.exe /s /t 0 /f
        }
    }
}

if (-not $success) {
    exit 1
}
