$ErrorActionPreference = "Stop"

$LevelName = ""
$DryRun = $false
$Overwrite = $false

foreach ($arg in $args) {
    if ($arg -ieq "--dry-run" -or $arg -ieq "-DryRun") {
        $DryRun = $true
        continue
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")

function Resolve-TexconvPath {
    $localTexconv = Join-Path $scriptDir "texconv.exe"
    if (Test-Path -LiteralPath $localTexconv) {
        return $localTexconv
    }

    if (-not [string]::IsNullOrWhiteSpace($env:TEXCONV_PATH) -and
        (Test-Path -LiteralPath $env:TEXCONV_PATH)) {
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

    return $name -match "normal|normals|norm|nrm|(^|[_\-\.])n([_\-\.]|$)"
}

function Get-SourceFiles {
    param(
        [string]$TargetType,
        [string]$TargetDir
    )

    $sourceExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp")

    $files = Get-ChildItem -LiteralPath $TargetDir -Recurse -File |
        Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() }

    if ($TargetType -eq "Model") {
        # 모델 폴더에는 다른 종류의 텍스처도 있으므로 순수 노멀 이름만 처리한다.
        $files = $files | Where-Object { Test-IsNormalName $_.Name }
    }

    return @($files)
}

function Invoke-NormalConversion {
    param(
        [string]$TargetType,
        [string]$TargetName,
        [string]$TargetDir,
        [string]$Texconv,
        [bool]$Overwrite,
        [bool]$DryRun
    )

    if (-not (Test-Path -LiteralPath $TargetDir)) {
        Write-Host "[ERROR] Target folder not found:"
        Write-Host "  $TargetDir"

        return [PSCustomObject]@{
            Total = 0
            Converted = 0
            Skipped = 0
            DryRun = 0
            Failed = 1
        }
    }

    $files = Get-SourceFiles -TargetType $TargetType -TargetDir $TargetDir

    Write-Host ""
    Write-Host "============================================="
    Write-Host " Normal DDS Converter"
    Write-Host "============================================="
    Write-Host " Type     : $TargetType"
    Write-Host " Target   : $TargetName"
    Write-Host " Folder   : $TargetDir"
    Write-Host " Format   : BC7_UNORM"
    Write-Host " Mips     : full chain (-m 0)"
    Write-Host " Overwrite: $Overwrite"
    Write-Host " DryRun   : $DryRun"
    Write-Host "============================================="
    Write-Host ""

    $total = 0
    $converted = 0
    $skipped = 0
    $dryRunCount = 0
    $failed = 0

    foreach ($file in $files) {
        $total++

        # 맵과 모델 모두 최종 DDS는 선택된 원본 이미지와 같은 폴더에 만든다.
        # 맵은 검색 시작점 자체가 Textures\Normal이므로 기존 구조가 유지된다.
        $outDds = Join-Path $file.DirectoryName ($file.BaseName + ".dds")

        if ((Test-Path -LiteralPath $outDds) -and -not $Overwrite) {
            Write-Host "[SKIP] $($file.FullName) (dds already exists)"
            $skipped++
            continue
        }

        if ($DryRun) {
            Write-Host "[DRY]  $($file.FullName) -> $outDds [BC7_UNORM, mips]"
            $dryRunCount++
            continue
        }

        Write-Host "[DDS]  $($file.FullName) -> $outDds [BC7_UNORM, mips]"

        & $Texconv `
            -y `
            -ft dds `
            -f BC7_UNORM `
            -m 0 `
            -o $file.DirectoryName `
            $file.FullName | Out-Host

        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outDds)) {
            Write-Host "[FAIL] $($file.FullName) (dds was not created)"
            $failed++
            continue
        }

        $converted++
    }

    Write-Host ""
    Write-Host "---------------------------------------------"
    Write-Host " Result: $TargetType / $TargetName"
    Write-Host "---------------------------------------------"
    Write-Host " Total    : $total"
    Write-Host " Converted: $converted"
    Write-Host " Skipped  : $skipped"
    Write-Host " DryRun   : $dryRunCount"
    Write-Host " Failed   : $failed"
    Write-Host "---------------------------------------------"

    return [PSCustomObject]@{
        Total = $total
        Converted = $converted
        Skipped = $skipped
        DryRun = $dryRunCount
        Failed = $failed
    }
}

Write-Host ""
Write-Host "============================================="
Write-Host " Safe Normal DDS Converter"
Write-Host "============================================="
Write-Host " 1. Map"
Write-Host " 2. Model"
Write-Host " 3. Effect"
Write-Host " 4. All (Map + Model)"
Write-Host "============================================="
Write-Host ""

$targetSelection = Read-Host "Select target (1-4)"

if ($targetSelection -notin @("1", "2", "3", "4")) {
    Write-Host "[ERROR] Invalid target selection."
    exit 1
}

if ($targetSelection -eq "3") {
    Write-Host "[NOT IMPLEMENTED] Effect normal conversion is not implemented."
    exit 0
}

Write-Host ""
Write-Host "Existing DDS handling:"
Write-Host " 1. Overwrite existing DDS"
Write-Host " 2. Skip when DDS already exists"
Write-Host ""

$overwriteSelection = Read-Host "Select mode (1-2)"

switch ($overwriteSelection) {
    "1" {
        $Overwrite = $true
    }
    "2" {
        $Overwrite = $false
    }
    default {
        Write-Host "[ERROR] Invalid overwrite selection."
        exit 1
    }
}

$targets = @()

if ($targetSelection -eq "1" -or $targetSelection -eq "4") {
    Write-Host ""
    Write-Host "Enter a path relative to Resources\Map."
    Write-Host "Example: SmallLevel_MF_Zone_01"
    $LevelName = Read-Host "Map path"

    if ([string]::IsNullOrWhiteSpace($LevelName)) {
        Write-Host "[ERROR] Map path is required."
        exit 1
    }

    $mapTargetDir = Join-Path $repoRoot "Client\Bin\Resources\Map\$LevelName\Textures\Normal"

    $targets += [PSCustomObject]@{
        Type = "Map"
        Name = $LevelName
        Dir = $mapTargetDir
    }
}

if ($targetSelection -eq "2" -or $targetSelection -eq "4") {
    Write-Host ""
    Write-Host "Enter a path relative to Resources\Models."
    Write-Host "Example: Heros\Lune"
    $modelPath = Read-Host "Model path"

    if ([string]::IsNullOrWhiteSpace($modelPath)) {
        Write-Host "[ERROR] Model path is required."
        exit 1
    }

    $modelTargetDir = Join-Path $repoRoot "Client\Bin\Resources\Models\$modelPath"

    $targets += [PSCustomObject]@{
        Type = "Model"
        Name = $modelPath
        Dir = $modelTargetDir
    }
}

$texconv = $null
if (-not $DryRun) {
    $texconv = Resolve-TexconvPath

    if ($null -eq $texconv) {
        Write-Host "[ERROR] texconv.exe not found."
        Write-Host "  Put texconv.exe next to this script"
        Write-Host "  or set TEXCONV_PATH to the full texconv.exe path."
        exit 1
    }
}

$grandTotal = 0
$grandConverted = 0
$grandSkipped = 0
$grandDryRun = 0
$grandFailed = 0

foreach ($target in $targets) {
    $result = Invoke-NormalConversion `
        -TargetType $target.Type `
        -TargetName $target.Name `
        -TargetDir $target.Dir `
        -Texconv $texconv `
        -Overwrite $Overwrite `
        -DryRun $DryRun

    $grandTotal += $result.Total
    $grandConverted += $result.Converted
    $grandSkipped += $result.Skipped
    $grandDryRun += $result.DryRun
    $grandFailed += $result.Failed
}

Write-Host ""
Write-Host "============================================="
Write-Host " All Done"
Write-Host "============================================="
Write-Host " Total    : $grandTotal"
Write-Host " Converted: $grandConverted"
Write-Host " Skipped  : $grandSkipped"
Write-Host " DryRun   : $grandDryRun"
Write-Host " Failed   : $grandFailed"
Write-Host "============================================="
Write-Host ""

if ($grandFailed -gt 0) {
    exit 1
}

exit 0
