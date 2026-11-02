param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$Sftool = "$PSScriptRoot/backend/sftool.exe",
    [string]$BackendManifest = "$PSScriptRoot/backend/backend-build.json",
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Sftool -PathType Leaf)) { throw "sftool not found: $Sftool" }
$sourceInfo = Get-Content -LiteralPath "$PSScriptRoot/backend/source.json" -Raw | ConvertFrom-Json
$backendInfo = Get-Content -LiteralPath $BackendManifest -Raw | ConvertFrom-Json
$patchHash = (Get-FileHash -LiteralPath (Join-Path "$PSScriptRoot/backend" $sourceInfo.patch) -Algorithm SHA256).Hash.ToLowerInvariant()
$exeHash = (Get-FileHash -LiteralPath $Sftool -Algorithm SHA256).Hash.ToLowerInvariant()
if ($backendInfo.commit -ne $sourceInfo.commit -or $backendInfo.revision -ne $sourceInfo.revision -or
    $patchHash -ne $sourceInfo.patch_sha256 -or $backendInfo.patch_sha256 -ne $patchHash -or $backendInfo.exe_sha256 -ne $exeHash) {
    throw 'Fixed backend provenance/hash mismatch; rebuild with build_backend.ps1. Refusing to package an unverified sftool.'
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$buildPath = Join-Path $env:TEMP 'SiFliFlashTool-build-1.2.1'
New-Item -ItemType Directory -Force -Path "$buildPath/backend" | Out-Null
Copy-Item -LiteralPath $Sftool -Destination "$buildPath/backend/sftool.exe"
Copy-Item -LiteralPath $BackendManifest -Destination "$buildPath/backend/backend-build.json"
& $Python -m PyInstaller --noconfirm --clean --noupx --onefile --windowed --name SiFliFlashTool --paths "$PSScriptRoot/.." --add-binary "$buildPath/backend/sftool.exe;." --add-data "$buildPath/backend/backend-build.json;." --distpath $outputPath --workpath "$buildPath/work" --specpath $buildPath "$PSScriptRoot/flash_gui.py"
if ($LASTEXITCODE -ne 0) { throw "PyInstaller failed: $LASTEXITCODE" }
Copy-Item -LiteralPath "$PSScriptRoot/README.md" -Destination "$outputPath/使用说明.md"
Copy-Item -LiteralPath $BackendManifest -Destination "$outputPath/backend-build.json"
Copy-Item -LiteralPath "$PSScriptRoot/backend/LICENSE-sftool" -Destination "$outputPath/LICENSE-sftool"
