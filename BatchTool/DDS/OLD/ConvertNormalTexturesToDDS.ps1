$ErrorActionPreference = "Stop"

$LevelName = ""
$Overwrite = $true
$DryRun = $false

foreach ($arg in $args) {
    if ($arg -ieq "--overwrite" -or $arg -ieq "-Overwrite") {
        # Kept for compatibility. Normal conversion overwrites by default so
        # material paths keep resolving to the same .dds file.
        $Overwrite = $true
        continue
    }

    if ($arg -ieq "--no-overwrite" -or $arg -ieq "-NoOverwrite") {
        $Overwrite = $false
        continue
    }

    if ($arg -ieq "--dry-run" -or $arg -ieq "-DryRun") {
        $DryRun = $true
        continue
    }

    $LevelName = $arg
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")

$textureDirs = @()
if ([string]::IsNullOrWhiteSpace($LevelName)) {
    $targetName = "Effects + Map + Models normal maps"
    $textureDirs = @(
        (Join-Path $repoRoot "Client\Bin\Resources\Effects"),
        (Join-Path $repoRoot "Client\Bin\Resources\Map"),
        (Join-Path $repoRoot "Client\Bin\Resources\Models")
    )
} else {
    $targetName = "$LevelName normal maps"
    $textureDirs = @(
        (Join-Path $repoRoot "Client\Bin\Resources\Map\$LevelName\Textures")
    )
}

$missingDirs = $textureDirs | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missingDirs.Count -gt 0) {
    Write-Host "[ERROR] Target folder not found:"
    foreach ($dir in $missingDirs) {
        Write-Host "  $dir"
    }
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

function Test-IsPackedNormalName {
    param([string]$FileName)

    $name = $FileName.ToLowerInvariant()
    return ($name -match "(^|[_\-\.\+])nmao([_\-\.\+]|$)") -or
           ($name -match "(^|[_\-\.\+])nao([_\-\.\+]|$)") -or
           ($name -match "(^|[_\-\.\+])nme([_\-\.\+]|$)") -or
           ($name -match "(^|[_\-\.\+])nre([_\-\.\+]|$)") -or
           ($name -match "(^|[_\-\.\+])orm([_\-\.\+]|$)") -or
           ($name -match "(^|[_\-\.\+])dpr([_\-\.\+]|$)") -or
           ($name -match "normal\+") -or
           ($name -match "normal.*rough") -or
           ($name -match "normal.*metal") -or
           ($name -match "normal.*emissive") -or
           ($name -match "roughness|metallic|metalness|emissive|mask|height|cavity")
}

function Test-IsNormalName {
    param([string]$FileName)

    $name = $FileName.ToLowerInvariant()
    if (Test-IsPackedNormalName $name) {
        return $false
    }

    return $name -match "normal|normals|norm|nrm|(^|_)n(_|\.|$)"
}

function Test-IsNormalMetadata {
    param([System.IO.FileInfo]$File)

    $metadataPath = Join-Path $File.DirectoryName ($File.BaseName + ".json")
    if (-not (Test-Path -LiteralPath $metadataPath)) {
        return $false
    }

    try {
        $metadataText = Get-Content -LiteralPath $metadataPath -Raw
    } catch {
        return $false
    }

    if (Test-IsPackedNormalName $File.Name) {
        return $false
    }

    return ($metadataText -match '"CompressionSettings"\s*:\s*"TextureCompressionSettings::TC_Normalmap"') -or
           ($metadataText -match '"PixelFormat"\s*:\s*"PF_BC5"') -or
           ($metadataText -match '"LODGroup"\s*:\s*"TextureGroup::TEXTUREGROUP_WorldNormalMap"')
}

function Test-IsPureNormalTexture {
    param([System.IO.FileInfo]$File)

    if (Test-IsNormalMetadata $File) {
        return $true
    }

    return Test-IsNormalName $File.Name
}

$texconv = Resolve-TexconvPath
if ($null -eq $texconv) {
    Write-Host "[ERROR] texconv.exe not found. Place texconv.exe next to this script or set TEXCONV_PATH."
    exit 1
}

Write-Host "============================================="
Write-Host " Normal DDS Converter"
Write-Host " Target   : $targetName"
Write-Host " Format   : BC5_UNORM"
Write-Host " Mips     : full chain (-m 0)"
Write-Host " Overwrite: $Overwrite (default for normal maps)"
Write-Host " Dry-run  : $DryRun"
Write-Host "============================================="
Write-Host ""

$sourceExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp")
$files = foreach ($dir in $textureDirs) {
    Get-ChildItem -LiteralPath $dir -Recurse -File |
        Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() } |
        Where-Object { Test-IsPureNormalTexture $_ }
}

$total = 0
$converted = 0
$skipped = 0
$dryRunCount = 0
$failed = 0
$tempRoot = Join-Path $env:TEMP ("Expedition_NormalDDS_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempRoot | Out-Null

foreach ($file in $files) {
    $total++

    $outDds = Join-Path $file.DirectoryName ($file.BaseName + ".dds")
    $tempOutDir = Join-Path $tempRoot ([System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tempOutDir | Out-Null
    $tempDds = Join-Path $tempOutDir ($file.BaseName + ".dds")

    if ($DryRun) {
        Write-Host "[DRY]  $($file.Name) -> $($file.BaseName).dds [BC5_UNORM, mips]"
        $dryRunCount++
        continue
    }

    if ((Test-Path -LiteralPath $outDds) -and -not $Overwrite) {
        Write-Host "[SKIP] $($file.Name) (dds already exists)"
        $skipped++
        continue
    }

    Write-Host "[DDS]  $($file.Name) -> $($file.BaseName).dds [BC5_UNORM, mips]"
    & $texconv -y -ft dds -f BC5_UNORM -m 0 -o $tempOutDir $file.FullName | Out-Host

    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $tempDds)) {
        Write-Host "[FAIL] $($file.Name) (temporary dds was not created)"
        $failed++
        continue
    }

    try {
        Copy-Item -LiteralPath $tempDds -Destination $outDds -Force
    } catch {
        Write-Host "[FAIL] $($file.Name) (could not replace existing dds: $($_.Exception.Message))"
        $failed++
        continue
    }

    $converted++
}

Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "============================================="
Write-Host " Done"
Write-Host "============================================="
Write-Host " Total    : $total"
Write-Host " Converted: $converted"
Write-Host " Skipped  : $skipped"
Write-Host " DryRun   : $dryRunCount"
Write-Host " Failed   : $failed"
Write-Host "============================================="
Write-Host ""

if ($failed -gt 0) {
    exit 1
}

exit 0
