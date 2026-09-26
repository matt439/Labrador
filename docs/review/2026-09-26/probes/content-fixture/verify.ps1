param([string]$CMakeExecutable = 'cmake')
$ErrorActionPreference = 'Stop'
$fixtureRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$sourceRoot = Join-Path $fixtureRoot 'source'
$buildRoot = Join-Path $fixtureRoot 'build'
$inputAsset = Join-Path $sourceRoot 'textures/white.dds'
$copiedAsset = Join-Path $buildRoot 'textures/white.dds'
if (Test-Path -LiteralPath $buildRoot) { throw 'Fixture build directory already exists; preserve evidence and use a fresh fixture.' }
& $CMakeExecutable -S $sourceRoot -B $buildRoot -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Fixture configure failed' }
& $CMakeExecutable --build $buildRoot --target LineSweeperFrameBench
if ($LASTEXITCODE -ne 0) { throw 'Fixture first build failed' }
$before = Get-Content -LiteralPath $copiedAsset -Raw -Encoding UTF8
'fixture asset version two' | Set-Content -LiteralPath $inputAsset -Encoding UTF8
& $CMakeExecutable --build $buildRoot --target LineSweeperFrameBench
if ($LASTEXITCODE -ne 0) { throw 'Fixture incremental build failed' }
$after = Get-Content -LiteralPath $copiedAsset -Raw -Encoding UTF8
# Single file inside this ignored fixture only; no real source or build asset.
if (-not $copiedAsset.StartsWith($fixtureRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Copy target escaped fixture' }
Remove-Item -LiteralPath $copiedAsset
& $CMakeExecutable --build $buildRoot --target LineSweeperFrameBench
if ($LASTEXITCODE -ne 0) { throw 'Fixture missing-content rebuild failed' }
[ordered]@{
    initial_copy = $before.Trim()
    changed_source = (Get-Content -LiteralPath $inputAsset -Raw -Encoding UTF8).Trim()
    after_incremental_build = $after.Trim()
    source_only_edit_refreshed = $after -ne $before
    missing_deployed_asset_restored = Test-Path -LiteralPath $copiedAsset
} | ConvertTo-Json
