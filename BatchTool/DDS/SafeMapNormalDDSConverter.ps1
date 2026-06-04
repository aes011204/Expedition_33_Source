$ErrorActionPreference = "Stop"

$LevelName = ""
$DryRun = $false

foreach ($arg in $args) {
    if ($arg -ieq "--dry-run" -or $arg -ieq "-DryRun") {
        $DryRun = $true
        continue
    }

    $LevelName = $arg
}

if ([string]::IsNullOrWhiteSpace($LevelName)) {
    Write-Host "[ERROR] Level name is required."
    Write-Host "Usage: SafeMapNormalDDSConverter.bat <LevelName> [--dry-run]"
    exit 1
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$targetDir = Join-Path $repoRoot "Client\Bin\Resources\Map\$LevelName\Textures\Normal"
$intermediateDir = Join-Path $targetDir "_Intermediate"

if (-not (Test-Path -LiteralPath $targetDir)) {
    Write-Host "[ERROR] Target folder not found:"
    Write-Host "  $targetDir"
    exit 1
}

function Resolve-TexconvPath {
    $localTexconv = Join-Path $scriptDir "texconv.exe"
    if (Test-Path -LiteralPath $localTexconv) {
        return $localTexconv
    }

    if (-not [string]::IsNullOrWhiteSpace($env:TEXCONV_PATH) -and (Test-Path -LiteralPath $env:TEXCONV_PATH)) {
        return $env:TEXCONV_PATH
    }

    $pathTexconv = Get-Command "texconv.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathTexconv) {
        return $pathTexconv.Source
    }

    return $null
}

$texconv = $null
if (-not $DryRun) {
    $texconv = Resolve-TexconvPath
    if ($null -eq $texconv) {
        Write-Host "[ERROR] texconv.exe not found."
        Write-Host "  Put texconv.exe next to SafeMapNormalDDSConverter.ps1"
        Write-Host "  or set TEXCONV_PATH to the full texconv.exe path."
        exit 1
    }
}

$sourceExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp")
$intermediatePrefix = ($intermediateDir.TrimEnd('\') + '\').ToLowerInvariant()

$files = Get-ChildItem -LiteralPath $targetDir -Recurse -File |
    Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() } |
    Where-Object { -not $_.FullName.ToLowerInvariant().StartsWith($intermediatePrefix) }

Write-Host ""
Write-Host "============================================="
Write-Host " Safe Map Normal DDS Converter"
Write-Host "============================================="
Write-Host " Level   : $LevelName"
Write-Host " Folder  : $targetDir"
Write-Host " Format  : BC7_UNORM"
Write-Host " Mode    : overwrite all existing DDS"
Write-Host " Texconv : $(if ($DryRun) { '(dry-run)' } else { $texconv })"
Write-Host " DryRun  : $DryRun"
Write-Host "============================================="
Write-Host ""

$total = 0
$converted = 0
$dryRunCount = 0
$failed = 0

foreach ($file in $files) {
    $total++
    $outDds = Join-Path $file.DirectoryName ($file.BaseName + ".dds")

    if ($DryRun) {
        Write-Host "[DRY]  $($file.Name) -> $($file.BaseName).dds [BC7_UNORM]"
        $dryRunCount++
        continue
    }

    Write-Host "[DDS]  $($file.Name) -> $($file.BaseName).dds [BC7_UNORM]"
    & $texconv -y -ft dds -f BC7_UNORM -m 0 -o $file.DirectoryName $file.FullName | Out-Host

    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outDds)) {
        Write-Host "[FAIL] $($file.Name) (dds was not created)"
        $failed++
        continue
    }

    $converted++
}

Write-Host ""
Write-Host "============================================="
Write-Host " Done"
Write-Host "============================================="
Write-Host " Total    : $total"
Write-Host " Converted: $converted"
Write-Host " DryRun   : $dryRunCount"
Write-Host " Failed   : $failed"
Write-Host "============================================="
Write-Host ""

if ($failed -gt 0) {
    exit 1
}

exit 0
