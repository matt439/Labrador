[CmdletBinding()]
param(
    [ValidateSet('build', 'test', 'play')]
    [string] $Action = 'build',
    [ValidateSet('x64-debug', 'x64-release')]
    [string] $Preset = 'x64-debug',
    [switch] $SmokeTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Read-StarterProcess {
    param([Diagnostics.ProcessStartInfo] $Start, [int] $TimeoutMilliseconds, [string] $Phase)

    $Start.UseShellExecute = $false
    $Start.CreateNoWindow = $true
    $Start.RedirectStandardOutput = $true
    $Start.RedirectStandardError = $true
    $Start.StandardOutputEncoding = [Text.Encoding]::UTF8
    $Start.StandardErrorEncoding = [Text.Encoding]::UTF8
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $Start
    try {
        [void] $process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            $process.Kill()
            throw "$Phase did not finish within $($TimeoutMilliseconds / 1000) seconds. Run the Visual Studio setup command manually to inspect its output."
        }
        return [pscustomobject]@{ ExitCode = $process.ExitCode; Output = $stdout.Result; Error = $stderr.Result }
    } finally {
        $process.Dispose()
    }
}

function Find-StarterTools {
    param([string] $VsWherePath = (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'))

    if (-not (Test-Path -LiteralPath $VsWherePath -PathType Leaf)) {
        throw 'Visual Studio discovery (vswhere.exe) is missing. Follow Install the tools, then run this command again.'
    }
    $query = @('-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        'Microsoft.VisualStudio.Component.VC.CMake.Project', '-format', 'json', '-utf8')
    # vswhere -utf8 must be decoded as UTF-8 even in an OEM-code-page prompt.
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $VsWherePath
    $start.Arguments = $query -join ' '
    $result = Read-StarterProcess $start 30000 'Visual Studio discovery'
    if ($result.ExitCode -ne 0) {
        $failure = [InvalidOperationException]::new("Visual Studio discovery failed (exit $($result.ExitCode)): $($result.Output)`n$($result.Error)")
        $failure.Data['ExitCode'] = $result.ExitCode
        throw $failure
    }
    try {
        $decoded = ConvertFrom-Json -InputObject $result.Output
        $installations = @()
        foreach ($installation in $decoded) { $installations += $installation }
    } catch {
        throw 'Visual Studio discovery returned invalid data. Run vswhere.exe from the Visual Studio Installer folder to inspect its output.'
    }
    if ($installations.Count -ne 1) {
        throw 'No suitable Visual Studio was found. Install Desktop development with C++, including C++ CMake tools for Windows, then run this command again.'
    }
    $installation = $installations[0]
    if (-not $installation.PSObject.Properties['installationPath'] -or
        -not $installation.PSObject.Properties['installationVersion']) {
        throw 'Visual Studio discovery returned no installation path or version. Run vswhere.exe from the Visual Studio Installer folder to inspect its output.'
    }
    $root = [string] $installation.installationPath
    $tools = [pscustomobject]@{
        Installation = $root
        Version = [string] $installation.installationVersion
        VcVars = Join-Path $root 'VC\Auxiliary\Build\vcvars64.bat'
        CMake = Join-Path $root 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        CTest = Join-Path $root 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
        Ninja = Join-Path $root 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
        Vcpkg = $env:VCPKG_ROOT
    }
    foreach ($name in @('VcVars', 'CMake', 'CTest', 'Ninja')) {
        if (-not (Test-Path -LiteralPath $tools.$name -PathType Leaf)) {
            throw "Visual Studio is missing $name at '$($tools.$name)'. Use Visual Studio Installer to add Desktop development with C++ and C++ CMake tools for Windows."
        }
    }
    if ([string]::IsNullOrWhiteSpace($tools.Vcpkg)) {
        $tools.Vcpkg = Join-Path $root 'VC\vcpkg'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $tools.Vcpkg 'scripts\buildsystems\vcpkg.cmake') -PathType Leaf)) {
        throw "VCPKG_ROOT '$($tools.Vcpkg)' has no scripts\buildsystems\vcpkg.cmake. Correct VCPKG_ROOT, or remove its override to use Visual Studio's bundled vcpkg."
    }
    return $tools
}

function Get-StarterEnvironment {
    param([Parameter(Mandatory)] [string] $VcVars)

    $marker = 'LABRADOR_TOOLS_' + [Guid]::NewGuid().ToString('N')
    $export = "`$ProgressPreference = 'SilentlyContinue'; [Console]::OutputEncoding = [Text.UTF8Encoding]::new(`$false); [Environment]::GetEnvironmentVariables('Process') | ConvertTo-Json -Compress -Depth 2"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($export))
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $env:SystemRoot 'System32\cmd.exe'
    $start.Arguments = '/d /s /v:off /c ""%LABRADOR_VCVARS_PATH%" && echo ' + $marker + ' && "%LABRADOR_POWERSHELL_PATH%" -NoProfile -NonInteractive -EncodedCommand ' + $encoded + '"'
    $start.EnvironmentVariables['LABRADOR_VCVARS_PATH'] = $VcVars
    $start.EnvironmentVariables['LABRADOR_POWERSHELL_PATH'] = Join-Path $PSHOME 'powershell.exe'
    $result = Read-StarterProcess $start 60000 'Visual Studio x64 environment setup'
    if ($result.ExitCode -ne 0) {
        $failure = [InvalidOperationException]::new("Visual Studio x64 environment setup failed (exit $($result.ExitCode)).`n$($result.Output)`n$($result.Error)")
        $failure.Data['ExitCode'] = $result.ExitCode
        throw $failure
    }
    $lines = $result.Output -split '\r?\n'
    $begin = -1
    for ($index = 0; $index -lt $lines.Count; ++$index) {
        if ($lines[$index].TrimEnd() -eq $marker) { $begin = $index; break }
    }
    if ($begin -lt 0) { throw 'Visual Studio setup returned no environment. Run vcvars64.bat in an ordinary Command Prompt to inspect its output.' }
    if ($begin -gt 0) { Write-Host ($lines[0..($begin - 1)] -join [Environment]::NewLine) }
    if ($result.Error) { [Console]::Error.Write($result.Error) }
    # Environment values may contain newlines, percent signs and equals
    # signs. JSON preserves them; the environment itself is never printed.
    $decoded = (($lines | Select-Object -Skip ($begin + 1)) -join [Environment]::NewLine) | ConvertFrom-Json
    $environment = @{}
    foreach ($property in $decoded.PSObject.Properties) {
        if (-not $property.Name.StartsWith('=')) { $environment[$property.Name] = [string] $property.Value }
    }
    $environment.Remove('LABRADOR_VCVARS_PATH')
    $environment.Remove('LABRADOR_POWERSHELL_PATH')
    return $environment
}

function Invoke-StarterTool {
    param([string] $File, [string[]] $Arguments, [string] $Directory, [hashtable] $Environment, [string] $Display)

    Write-Host "> $Display"
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $File
    # Every argument here is a fixed switch or a validated preset, not a shell
    # command. FileName/WorkingDirectory carry paths without shell interpolation.
    $start.Arguments = $Arguments -join ' '
    $start.WorkingDirectory = $Directory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.Encoding]::UTF8
    $start.StandardErrorEncoding = [Text.Encoding]::UTF8
    $start.EnvironmentVariables.Clear()
    foreach ($name in $Environment.Keys) { $start.EnvironmentVariables[$name] = $Environment[$name] }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        [void] $process.Start()
        $stdout = $process.StandardOutput.ReadLineAsync()
        $stderr = $process.StandardError.ReadLineAsync()
        while ($null -ne $stdout -or $null -ne $stderr) {
            $drained = $false
            if ($null -ne $stdout -and $stdout.IsCompleted) {
                $drained = $true
                $line = $stdout.Result
                if ($null -eq $line) { $stdout = $null } else {
                    [Console]::Out.WriteLine($line)
                    $stdout = $process.StandardOutput.ReadLineAsync()
                }
            }
            if ($null -ne $stderr -and $stderr.IsCompleted) {
                $drained = $true
                $line = $stderr.Result
                if ($null -eq $line) { $stderr = $null } else {
                    [Console]::Error.WriteLine($line)
                    $stderr = $process.StandardError.ReadLineAsync()
                }
            }
            if (-not $drained) { [Threading.Thread]::Sleep(10) }
        }
        $process.WaitForExit()
        return $process.ExitCode
    } finally {
        $process.Dispose()
    }
}

function Invoke-StarterCommand {
    param([string] $Project, [string] $Intent, [string] $SelectedPreset, [switch] $StartupCheck)

    if ($StartupCheck -and $Intent -ne 'play') { throw '-SmokeTest is available with play.cmd only; test.cmd already runs the startup test.' }
    foreach ($file in @('CMakeLists.txt', 'CMakePresets.json', 'game\CMakeLists.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Project $file) -PathType Leaf)) {
            throw "This helper needs the starter project, including '$file'. Keep the helper files together in the extracted project."
        }
    }
    $tools = Find-StarterTools
    Write-Host "Visual Studio $($tools.Version): $($tools.Installation)"
    Write-Host "Project: $Project"
    Write-Host "Action: $Intent; preset: $SelectedPreset"
    $environment = Get-StarterEnvironment $tools.VcVars
    $environment['VCPKG_ROOT'] = $tools.Vcpkg
    $commands = @(
        [pscustomobject]@{ File = $tools.CMake; Arguments = @('--preset', $SelectedPreset); Display = "cmake --preset $SelectedPreset"; Phase = 'Configure' },
        [pscustomobject]@{ File = $tools.CMake; Arguments = @('--build', '--preset', $SelectedPreset); Display = "cmake --build --preset $SelectedPreset"; Phase = 'Build' }
    )
    if ($Intent -eq 'test') {
        $commands += [pscustomobject]@{ File = $tools.CTest; Arguments = @('--preset', $SelectedPreset); Display = "ctest --preset $SelectedPreset"; Phase = 'Tests' }
    }
    foreach ($command in $commands) {
        $code = Invoke-StarterTool $command.File $command.Arguments $Project $environment $command.Display
        if ($code -ne 0) {
            [Console]::Error.WriteLine("$($command.Phase) failed (exit $code). Read the output above, fix the reported problem, then run this helper again.")
            return $code
        }
    }
    if ($Intent -eq 'play') {
        $cache = Join-Path $Project "out\build\$SelectedPreset\CMakeCache.txt"
        $names = @()
        foreach ($line in (Get-Content -LiteralPath $cache)) {
            if ($line -match '^CMAKE_PROJECT_NAME:STATIC=(.+)$') { $names += $Matches[1] }
        }
        if ($names.Count -ne 1 -or $names[0] -notmatch '^[A-Za-z_][A-Za-z0-9_-]*$') {
            throw "Could not read the game project name from '$cache'. Follow the manual play command for a customised project."
        }
        $relative = "out\build\$SelectedPreset\game\$($names[0]).exe"
        $game = Join-Path $Project $relative
        if (-not (Test-Path -LiteralPath $game -PathType Leaf)) { throw "The build did not produce '$game'. Follow the manual play command if the game target has a different name." }
        $arguments = @()
        if ($StartupCheck) { $arguments = @('--smoke-test') }
        return Invoke-StarterTool $game $arguments $Project $environment ($relative + $(if ($StartupCheck) { ' --smoke-test' }))
    }
    return 0
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        $project = [IO.Directory]::GetParent($PSScriptRoot).FullName
        exit (Invoke-StarterCommand $project $Action $Preset -StartupCheck:$SmokeTest)
    } catch {
        [Console]::Error.WriteLine("Starter command: $($_.Exception.Message)")
        if ($_.Exception.Data.Contains('ExitCode')) { exit [int] $_.Exception.Data['ExitCode'] }
        exit 2
    }
}
