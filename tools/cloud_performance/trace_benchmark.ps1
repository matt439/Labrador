[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BundleRoot,
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ReleaseSHA256,
    [Parameter(Mandatory = $true)][ValidateSet('d3d11', 'd3d12', 'gl', 'vulkan')][string]$Backend,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [ValidateSet('software', 'presentation')][string]$Pacing = 'software',
    [ValidateRange(1, 100000)][int]$Warmup = 1800,
    [ValidateRange(1, 100000)][int]$Sample = 3600,
    [ValidateRange(1, 1000)][int]$Refresh = 60,
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 210,
    [string]$WprExecutable = 'wpr.exe'
)

# Local only. A fresh, named WPR instance keeps every stop scoped to this run.
# Never use global -cancel: an unrelated recording belongs to its owner.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Equal($Actual, $Expected, [string]$Name) {
    if ($Actual -ne $Expected) { throw "$Name differs: expected '$Expected', got '$Actual'" }
}

function Resolve-Inside([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative) -or
        $Relative.Contains('\') -or $Relative.Contains(':') -or
        $Relative.Split('/') -contains '..') {
        throw "Unsafe bundle member: '$Relative'"
    }
    $base = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $file = [IO.Path]::GetFullPath((Join-Path $Root $Relative.Replace('/', '\')))
    if (-not $file.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Bundle member escapes root: '$Relative'"
    }
    return $file
}

function Write-Json([string]$Path, $Value) {
    $Value | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Quote-NativeArguments([string[]]$Values) {
    # All arguments are fixed tokens, bounded integers or Windows file paths.
    # A quote is illegal in a Windows path; reject it rather than pass shell code.
    return (@($Values | ForEach-Object {
        if ($_ -match '"|[\r\n]' -or $_.EndsWith('\')) {
            throw 'An argument cannot contain quotes/newlines or end in a backslash'
        }
        '"' + $_ + '"'
    }) -join ' ')
}

function Invoke-RecordedProcess([string]$File, [string[]]$Arguments,
    [string]$Name, [int]$LimitSeconds) {
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $File
    $start.Arguments = Quote-NativeArguments $Arguments
    $start.WorkingDirectory = $script:outputRoot
    $start.UseShellExecute = $false
    # Suppress a helper console without SW_HIDE: the benchmark must create and
    # size its own visible render window for meaningful presentation evidence.
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    $started = $false
    $stdout = $null
    $stderr = $null
    try {
        $started = $process.Start()
        if (-not $started) { throw "$Name did not start" }
        $processHandle = $process.Handle
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $processRecord = [ordered]@{ pid = $process.Id; started_utc = [DateTimeOffset]::UtcNow.ToString('o') }
        Write-Json (Join-Path $script:outputRoot "$Name.process.json") $processRecord
        if (-not $process.WaitForExit($LimitSeconds * 1000)) {
            $process.Kill()
            $process.WaitForExit()
            throw "$Name exceeded its $LimitSeconds second deadline; its process was terminated"
        }
        if ($null -eq $process.ExitCode) { throw "$Name has no exit code" }
        return $process.ExitCode
    }
    finally {
        # Every operation after Start can throw, including writing its PID.
        # Termination precedes log writes; one failed stream write must not
        # prevent the other write or release of the process handle.
        try {
            if ($started -and -not $process.HasExited) {
                $process.Kill()
                $process.WaitForExit()
            }
            try {
                if ($null -ne $stdout) {
                    $stdout.GetAwaiter().GetResult() | Set-Content `
                        -LiteralPath (Join-Path $script:outputRoot "$Name.stdout.log") -Encoding UTF8
                }
            }
            finally {
                if ($null -ne $stderr) {
                    $stderr.GetAwaiter().GetResult() | Set-Content `
                        -LiteralPath (Join-Path $script:outputRoot "$Name.stderr.log") -Encoding UTF8
                }
            }
        }
        finally { $process.Dispose() }
    }
}

$bundlePath = (Resolve-Path -LiteralPath $BundleRoot).ProviderPath
$releasePath = Join-Path $bundlePath 'release.json'
$releaseHash = (Get-FileHash -LiteralPath $releasePath -Algorithm SHA256).Hash.ToLowerInvariant()
Assert-Equal $releaseHash $ReleaseSHA256.ToLowerInvariant() 'release manifest SHA256'
$release = Get-Content -LiteralPath $releasePath -Raw -Encoding UTF8 | ConvertFrom-Json
Assert-Equal $release.schema_version 1 'release schema'
Assert-Equal $release.scope 'labrador-cloud-performance-payload' 'release scope'
$executableMember = "payload/$Backend/LineSweeperFrameBench.exe"
if ($null -eq $release.files.PSObject.Properties[$executableMember]) {
    throw 'Release manifest does not contain the selected benchmark'
}
foreach ($entry in $release.files.PSObject.Properties) {
    $file = Resolve-Inside $bundlePath $entry.Name
    Assert-Equal (Get-Item -LiteralPath $file).Length ([int64]$entry.Value.bytes) $entry.Name
    Assert-Equal (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() `
        $entry.Value.sha256.ToLowerInvariant() "$($entry.Name) SHA256"
}
$executable = Resolve-Inside $bundlePath $executableMember
$wpr = (Get-Command -Name $WprExecutable -CommandType Application -ErrorAction Stop).Source
if ($Pacing -eq 'software' -and ($Warmup + $Sample) / [double]$Refresh -ge $TimeoutSeconds) {
    throw 'Benchmark deadline must exceed its nominal warmup plus sample duration'
}
$script:outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $script:outputRoot) {
    throw "Refusing to overwrite an existing evidence directory: $script:outputRoot"
}
New-Item -ItemType Directory -Path $script:outputRoot | Out-Null
$instance = 'LabradorGpu-' + [Guid]::NewGuid().ToString('N')
$resultPath = Join-Path $script:outputRoot 'result.json'
$tracePath = Join-Path $script:outputRoot 'gpu.etl'
$arguments = @('--output', $resultPath, '--run', $instance,
    '--release-hash', $releaseHash, '--warmup', [string]$Warmup,
    '--sample', [string]$Sample, '--refresh', [string]$Refresh)
# Omitting the default option also admits the frozen software-paced binaries.
if ($Pacing -eq 'presentation') { $arguments += @('--pacing', 'presentation') }
$request = [ordered]@{
    schema_version = 1; scope = 'labrador-local-gpu-trace'; instance = $instance
    started_utc = [DateTimeOffset]::UtcNow.ToString('o')
    bundle_root = $bundlePath; release_sha256 = $releaseHash
    executable = $executable; executable_sha256 = $release.files.$executableMember.sha256
    backend = $Backend; pacing = $Pacing; benchmark_arguments = $arguments
    benchmark_timeout_seconds = $TimeoutSeconds; wpr_executable = $wpr; wpr_profile = 'GPU'
    wrapper_sha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
    stop_command = @($wpr, '-stop', $tracePath, '-skipPdbGen', '-instancename', $instance)
}
Write-Json (Join-Path $script:outputRoot 'request.json') $request
Copy-Item -LiteralPath $releasePath -Destination (Join-Path $script:outputRoot 'release.json')
$traceStarted = $false
$traceStartAttempted = $false
$traceStopped = $false
$failure = $null
$benchmarkExit = $null
try {
    # Status is evidence only: WPR output is localized and its default instance
    # does not enumerate named recordings. Isolation comes from -instancename.
    $statusExit = Invoke-RecordedProcess $wpr @('-status') 'wpr-status-before' 30
    Assert-Equal $statusExit 0 'WPR status exit code'
    $profilesExit = Invoke-RecordedProcess $wpr @('-profiles') 'wpr-profiles' 30
    Assert-Equal $profilesExit 0 'WPR profiles exit code'
    $profiles = Get-Content -LiteralPath (Join-Path $script:outputRoot 'wpr-profiles.stdout.log') -Raw
    if ($profiles -notmatch '(?m)^\s*GPU\s') { throw 'Installed WPR has no GPU profile' }
    $temporary = Join-Path $script:outputRoot 'wpr-temporary'
    New-Item -ItemType Directory -Path $temporary | Out-Null
    $traceStartAttempted = $true
    $startExit = Invoke-RecordedProcess $wpr @('-start', 'GPU', '-filemode',
        '-recordtempto', $temporary, '-instancename', $instance) 'wpr-start' 30
    Assert-Equal $startExit 0 'WPR start exit code'
    $traceStarted = $true
    $benchmarkExit = Invoke-RecordedProcess $executable $arguments 'benchmark' $TimeoutSeconds
    Assert-Equal $benchmarkExit 0 'benchmark exit code'
}
catch { $failure = $_.Exception.Message }
finally {
    # A failed/timed-out start can have created some collectors. The GUID was
    # generated by this invocation, so trying its named stop cannot touch an
    # unrelated recording even when start did not report success.
    if ($traceStartAttempted) {
        try {
            $stopExit = Invoke-RecordedProcess $wpr @('-stop', $tracePath, '-skipPdbGen',
                '-instancename', $instance) 'wpr-stop' 120
            Assert-Equal $stopExit 0 'WPR stop exit code'
            if (-not (Test-Path -LiteralPath $tracePath -PathType Leaf) -or
                (Get-Item -LiteralPath $tracePath).Length -eq 0) {
                throw 'WPR did not produce a nonempty ETL'
            }
            $traceStopped = $true
        }
        catch {
            $failure = (@($failure, $_.Exception.Message) | Where-Object { $_ }) -join '; '
        }
    }
}
if ($null -eq $failure) {
    try {
        $result = Get-Content -LiteralPath $resultPath -Raw -Encoding UTF8 | ConvertFrom-Json
        Assert-Equal $result.schema_version 1 'benchmark schema'
        Assert-Equal $result.scope 'linesweeper_frame_benchmark' 'benchmark scope'
        Assert-Equal $result.status 'complete' 'benchmark status'
        Assert-Equal $result.run $instance 'benchmark run'
        Assert-Equal $result.release_sha256 $releaseHash 'benchmark release'
        Assert-Equal $result.build.configuration 'release' 'benchmark build'
        Assert-Equal $result.build.render_backend $Backend 'benchmark backend'
        Assert-Equal $result.measurement_class 'hardware_raster' 'benchmark device class'
        Assert-Equal $result.workload.warmup_frames $Warmup 'warmup count'
        Assert-Equal $result.workload.sample_frames $Sample 'sample count'
        Assert-Equal $result.workload.refresh_hz $Refresh 'nominal refresh'
        Assert-Equal $result.timing.sample_count $Sample 'retained count'
        Assert-Equal @($result.samples).Count $Sample 'raw retained count'
        $expectedPacer = if ($Pacing -eq 'software') {
            'win32_high_resolution_waitable_timer'
        } else { 'presentation_driven' }
        Assert-Equal $result.timing.pacer $expectedPacer 'pacer'
    }
    catch { $failure = $_.Exception.Message }
}
$files = [ordered]@{}
Get-ChildItem -LiteralPath $script:outputRoot -File | ForEach-Object {
    $files[$_.Name] = @{ bytes = $_.Length; sha256 =
        (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
}
Write-Json (Join-Path $script:outputRoot 'capture.json') ([ordered]@{
    schema_version = 1; scope = 'labrador-local-gpu-trace'
    status = $(if ($null -eq $failure) { 'complete' } else { 'failed' })
    finished_utc = [DateTimeOffset]::UtcNow.ToString('o'); instance = $instance
    trace_start_attempted = $traceStartAttempted
    trace_started = $traceStarted; trace_saved = $traceStopped
    benchmark_exit_code = $benchmarkExit; failure = $failure; files = $files
})
if ($null -ne $failure) {
    throw "$failure. Evidence: $script:outputRoot. If WPR cleanup failed, use only the named stop command in request.json."
}
Write-Output "GPU trace and benchmark evidence: $script:outputRoot"
