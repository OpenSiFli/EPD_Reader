param([int]$Jobs = 12)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'SiFli-SDK/export.ps1')
$env:SIFLI_SDK_BOARD_SEARCH_PATH = Join-Path $PSScriptRoot 'epdiy-epub'
Push-Location (Join-Path $PSScriptRoot 'epdiy-epub/project')
try {
    scons --board=sf32lb527-oed-e0470a03_hcpu "-j$Jobs"
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
} finally { Pop-Location }
