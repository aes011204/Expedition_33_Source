$ErrorActionPreference = "Stop"

$LevelName = ""
$Overwrite = $false
$DryRun = $false

foreach ($arg in $args) {
    if ($arg -ieq "--overwrite" -or $arg -ieq "-Overwrite") {
        # Support the batch-style option used from ConvertMapTexturesToDDS.bat.
        $Overwrite = $true
        continue
    }

    if ($arg -ieq "--dry-run" -or $arg -ieq "-DryRun") {
        # Dry-run must never call texconv; it only prints the recursive target list.
        $DryRun = $true
        continue
    }

    # The only positional argument is the optional level folder name.
    $LevelName = $arg
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")

$textureDirs = @()

if ([string]::IsNullOrWhiteSpace($LevelName)) {
    $targetName = "Effects + Map + Models"

    # No argument means the runtime-heavy texture roots only.
    # Data/UI/Fonts/Materials are intentionally excluded from this DDS pass.
    $textureDirs = @(
        (Join-Path $repoRoot "Client\Bin\Resources\Effects"),
        (Join-Path $repoRoot "Client\Bin\Resources\Map"),
        (Join-Path $repoRoot "Client\Bin\Resources\Models")
    )
} else {
    $targetName = $LevelName
    # A positional level name keeps the old map-only workflow.
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

function Get-DdsFormat {
    param([string]$FileName)

    $name = $FileName.ToLowerInvariant()
    $isColor = $false
    $isData = $false

    # Color textures need sRGB sampling after DDS replacement.
    # Model exports often use short suffixes such as _c or _diff instead of BaseColor.
    if ($name -match "basecolor|base_color|albedo|diffuse|diff|emissive|color|colour|_bc|_c\b|_d\b|_ar|_albedo") {
        $isColor = $true
    }

    # Packed normal/data textures must stay linear even if their name also has a color-like token.
    if ($name -match "normal|norm|nmao|nao|orm|occlusionroughnessmetallic|\bao\b|mask|height|cavity|roughness|metallic|metalness|spec|_n\b|_s\b|_m\b") {
        $isData = $true
    }

    # These are data only when the file was not already classified as a color texture.
    if (-not $isColor -and $name -match "roughness|metalness|opacity|alpha") {
        $isData = $true
    }

    if ($isData) {
        return "BC7_UNORM"
    }

    if ($isColor) {
        return "BC7_UNORM_SRGB"
    }

    return "BC7_UNORM"
}

function Test-ShouldSkipDds {
    param([System.IO.FileInfo]$File)

    $path = $File.FullName.ToLowerInvariant().Replace('\', '/')
    $name = $File.Name.ToLowerInvariant()

    $isHairTexture =
        $path.Contains("hair") -or
        $name.Contains("hair")

    $isEyelashOrEyebrowTexture =
        $name.Contains("eyelash") -or
        $name.Contains("eyebrow")

    # Hair cards keep better distant alpha coverage from the original PNG path.
    return $isHairTexture -or $isEyelashOrEyebrowTexture
}

$texconv = $null
if (-not $DryRun) {
    $texconv = Resolve-TexconvPath
    if ($null -eq $texconv) {
        Write-Host "[ERROR] texconv.exe not found."
        Write-Host "  Put texconv.exe next to ConvertMapTexturesToDDS.bat"
        Write-Host "  or set TEXCONV_PATH to the full texconv.exe path."
        exit 1
    }
}

Write-Host ""
Write-Host "============================================="
Write-Host " Convert Runtime Textures To DDS"
Write-Host "============================================="
Write-Host " Target   : $targetName"
Write-Host " Folders  :"
foreach ($dir in $textureDirs) {
    Write-Host "   $dir"
}
Write-Host " Texconv  : $(if ($DryRun) { '(dry-run)' } else { $texconv })"
Write-Host " Overwrite: $Overwrite"
Write-Host " DryRun   : $DryRun"
Write-Host "============================================="
Write-Host ""

$sourceExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp")
$files = foreach ($dir in $textureDirs) {
    Get-ChildItem -LiteralPath $dir -Recurse -File |
        Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() }
}

$total = 0
$converted = 0
$skipped = 0
$dryRunCount = 0
$failed = 0

foreach ($file in $files) {
    $total++

    if (Test-ShouldSkipDds $file) {
        Write-Host "[SKIP] $($file.Name) (hair alpha/data texture keeps original)"
        $skipped++
        continue
    }

    $outDds = Join-Path $file.DirectoryName ($file.BaseName + ".dds")
    $format = Get-DdsFormat $file.Name

    if ((Test-Path -LiteralPath $outDds) -and -not $Overwrite) {
        Write-Host "[SKIP] $($file.Name) (dds already exists)"
        $skipped++
        continue
    }

    if ($DryRun) {
        Write-Host "[DRY]  $($file.Name) -> $($file.BaseName).dds [$format]"
        $dryRunCount++
        continue
    }

    Write-Host "[DDS]  $($file.Name) -> $($file.BaseName).dds [$format]"

    # Pass each texconv argument separately; this avoids cmd quote parsing failures.
    & $texconv -y -ft dds -f $format -m 0 -o $file.DirectoryName $file.FullName | Out-Host

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
Write-Host " Skipped  : $skipped"
Write-Host " DryRun   : $dryRunCount"
Write-Host " Failed   : $failed"
Write-Host "============================================="
Write-Host ""

if ($failed -gt 0) {
    exit 1
}

exit 0
