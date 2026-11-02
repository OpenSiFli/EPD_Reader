param(
    [string]$SourceDirectory,
    [string]$OutputDirectory = "$PSScriptRoot/backend",
    [string]$TargetDirectory,
    [ValidateSet('debug', 'release')][string]$Profile = 'debug'
)
$ErrorActionPreference = 'Stop'
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$targetPath = if ($TargetDirectory) { [IO.Path]::GetFullPath($TargetDirectory) } else { $null }
$sourceInfo = Get-Content -LiteralPath "$PSScriptRoot/backend/source.json" -Raw | ConvertFrom-Json
$patchPath = Join-Path "$PSScriptRoot/backend" $sourceInfo.patch
$patchHash = (Get-FileHash -LiteralPath $patchPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($patchHash -ne $sourceInfo.patch_sha256) { throw 'Backend patch hash mismatch' }
$repository = if ($SourceDirectory) { [IO.Path]::GetFullPath($SourceDirectory) } else { $sourceInfo.upstream }
$buildRoot = Join-Path $env:TEMP ('SiFliFlashTool-backend-' + [guid]::NewGuid().ToString('N'))
if (-not $targetPath) { $targetPath = Join-Path $buildRoot 'target' }
& git clone --no-hardlinks --no-checkout $repository $buildRoot
if ($LASTEXITCODE -ne 0) { throw 'Cannot clone sftool source' }
Push-Location $buildRoot
try {
    & git checkout --detach $sourceInfo.commit
    if ($LASTEXITCODE -ne 0) { throw 'Cannot check out pinned backend source' }
    & git apply --check $patchPath
    if ($LASTEXITCODE -ne 0) { throw 'Backend patch does not apply' }
    & git apply $patchPath
    if ($LASTEXITCODE -ne 0) { throw 'Cannot apply backend patch' }
    & cargo test -p sftool-lib erase_ack_tests --locked --target-dir $targetPath
    if ($LASTEXITCODE -ne 0) { throw 'Backend regression tests failed' }
    $buildArgs = @('build', '-p', 'sftool', '--locked', '--target-dir', $targetPath)
    if ($Profile -eq 'release') { $buildArgs += '--release' }
    & cargo @buildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Backend build failed' }
    New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
    Copy-Item -LiteralPath "$targetPath/$Profile/sftool.exe" -Destination "$outputPath/sftool.exe"
    @{
        revision = $sourceInfo.revision
        commit = $sourceInfo.commit
        patch_sha256 = $patchHash
        exe_sha256 = (Get-FileHash -LiteralPath "$outputPath/sftool.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
        profile = $Profile
    } | ConvertTo-Json | Set-Content -LiteralPath "$outputPath/backend-build.json" -Encoding UTF8
    Write-Host "Fixed sftool and build manifest: $outputPath"
} finally { Pop-Location }
