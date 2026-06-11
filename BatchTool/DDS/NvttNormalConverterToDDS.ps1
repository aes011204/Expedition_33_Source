$ErrorActionPreference = "Stop"

$DryRun = $false
$KeepIntermediate = $false

foreach ($arg in $args) {
    if ($arg -ieq "--dry-run" -or $arg -ieq "-DryRun") {
        $DryRun = $true
        continue
    }

    if ($arg -ieq "--keep-intermediate" -or $arg -ieq "-KeepIntermediate") {
        $KeepIntermediate = $true
        continue
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$nvttPresetPath = Join-Path $scriptDir "nvtt_normal.dpf"

function Resolve-TexconvPath {
    $localTexconv = Join-Path $scriptDir "texconv.exe"
    if (Test-Path -LiteralPath $localTexconv) { return $localTexconv }

    if (-not [string]::IsNullOrWhiteSpace($env:TEXCONV_PATH) -and
        (Test-Path -LiteralPath $env:TEXCONV_PATH)) {
        return $env:TEXCONV_PATH
    }

    $pathTexconv = Get-Command "texconv.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathTexconv) { return $pathTexconv.Source }

    return $null
}

function Resolve-NvttExportPath {
    $localNvtt = Join-Path $scriptDir "nvtt_export.exe"
    if (Test-Path -LiteralPath $localNvtt) { return $localNvtt }

    if (-not [string]::IsNullOrWhiteSpace($env:NVTT_EXPORT_PATH) -and
        (Test-Path -LiteralPath $env:NVTT_EXPORT_PATH)) {
        return $env:NVTT_EXPORT_PATH
    }

    $pathNvtt = Get-Command "nvtt_export.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathNvtt) { return $pathNvtt.Source }

    return $null
}

function Get-NormalMatchToken {
    param([string]$BaseName)

    # Map flow is intentionally kept compatible with the old builder.
    $orderedTokens = @("_NMAo", "_NAo", "_NR", "Normal", "_N")
    foreach ($token in $orderedTokens) {
        if ($BaseName.IndexOf($token, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            return $token
        }
    }

    return $null
}

function Get-RelativeDisplayPath {
    param([string]$RootPath, [string]$FullPath)

    $normalizedRoot = [System.IO.Path]::GetFullPath($RootPath)
    if (-not $normalizedRoot.EndsWith([System.IO.Path]::DirectorySeparatorChar) -and
        -not $normalizedRoot.EndsWith([System.IO.Path]::AltDirectorySeparatorChar)) {
        $normalizedRoot += [System.IO.Path]::DirectorySeparatorChar
    }

    $rootUri = New-Object System.Uri($normalizedRoot)
    $fullUri = New-Object System.Uri([System.IO.Path]::GetFullPath($FullPath))
    $relativeUri = $rootUri.MakeRelativeUri($fullUri)
    return [System.Uri]::UnescapeDataString($relativeUri.ToString()).Replace('/', '\')
}

function Write-CandidateList {
    param([object[]]$Candidates)

    Write-Host ""
    Write-Host "Matched normal candidates: $($Candidates.Count)"
    if ($Candidates.Count -eq 0) {
        Write-Host "  (none)"
        return
    }

    for ($i = 0; $i -lt $Candidates.Count; $i++) {
        $number = "{0:D3}" -f ($i + 1)
        Write-Host ("[{0}] {1}" -f $number, $Candidates[$i].RelativePath)
    }
}

function Confirm-Candidates {
    param([object[]]$Candidates)

    $basenameCounts = @{}
    foreach ($candidate in $Candidates) {
        if (-not $basenameCounts.ContainsKey($candidate.BaseName)) {
            $basenameCounts[$candidate.BaseName] = 0
        }
        $basenameCounts[$candidate.BaseName]++
    }

    $confirmedCandidates = [System.Collections.Generic.List[object]]::new()
    foreach ($candidate in $Candidates) { $confirmedCandidates.Add($candidate) }

    while ($true) {
        Write-CandidateList -Candidates $confirmedCandidates
        Write-Host ""
        $inputText = Read-Host "Enter file name to exclude, 'go' to continue, or 'cancel' to abort"
        $trimmed = $inputText.Trim()

        if ([string]::IsNullOrWhiteSpace($trimmed)) {
            Write-Host "[NO MATCH] Empty input"
            continue
        }

        if ($trimmed.Equals("go", [System.StringComparison]::OrdinalIgnoreCase) -or
            $trimmed.Equals("ok", [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Host "[CONFIRMED] $($confirmedCandidates.Count) files"
            return @($confirmedCandidates)
        }

        if ($trimmed.Equals("cancel", [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Host "[CANCELLED] No files were changed."
            return $null
        }

        $relativeMatches = @($confirmedCandidates | Where-Object {
            $_.RelativePath.Equals($trimmed, [System.StringComparison]::OrdinalIgnoreCase)
        })

        $match = $null
        if ($relativeMatches.Count -eq 1) {
            $match = $relativeMatches[0]
        }
        else {
            $basenameMatches = @($confirmedCandidates | Where-Object {
                $_.BaseName.Equals($trimmed, [System.StringComparison]::OrdinalIgnoreCase)
            })

            if ($basenameMatches.Count -eq 1 -and $basenameCounts[$basenameMatches[0].BaseName] -eq 1) {
                $match = $basenameMatches[0]
            }
            elseif ($basenameMatches.Count -gt 0) {
                Write-Host "[NO MATCH] '$trimmed' is duplicated. Enter the relative path shown in the list."
                continue
            }
        }

        if ($null -eq $match) {
            Write-Host "[NO MATCH] $trimmed"
            continue
        }

        [void]$confirmedCandidates.Remove($match)
        Write-Host "[REMOVED] $($match.RelativePath)"
    }
}

function Convert-ToBc5Intermediate {
    param(
        [string]$TexconvPath,
        [string]$SourcePath,
        [string]$IntermediateRootPath,
        [string]$RelativePath
    )

    $relativeDirectory = Split-Path -Parent $RelativePath
    $outputDir = if ([string]::IsNullOrWhiteSpace($relativeDirectory)) {
        $IntermediateRootPath
    } else {
        Join-Path $IntermediateRootPath $relativeDirectory
    }

    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    $expectedDds = Join-Path $outputDir (([System.IO.Path]::GetFileNameWithoutExtension($RelativePath)) + ".dds")

    & $TexconvPath -y -ft dds -f BC5_UNORM -m 0 -o $outputDir $SourcePath | Out-Host
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $expectedDds)) {
        throw "BC5 conversion failed for '$RelativePath'."
    }

    return $expectedDds
}

function Convert-ToNvttFinal {
    param(
        [string]$NvttExportPath,
        [string]$PresetPath,
        [string]$IntermediateDdsPath,
        [string]$FinalPath
    )

    $finalDir = Split-Path -Parent $FinalPath
    New-Item -ItemType Directory -Path $finalDir -Force | Out-Null

    & $NvttExportPath --preset $PresetPath --output $FinalPath $IntermediateDdsPath | Out-Host
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $FinalPath)) {
        throw "NVTT conversion failed for '$FinalPath'."
    }
}

function Invoke-MapNvttConversion {
    param(
        [string]$MapPath,
        [bool]$Overwrite,
        [string]$TexconvPath,
        [string]$NvttExportPath
    )

    $texturesRoot = Join-Path $repoRoot "Client\Bin\Resources\Map\$MapPath\Textures"
    $normalRoot = Join-Path $texturesRoot "Normal"
    $intermediateRoot = Join-Path $normalRoot "_Intermediate\BC5"

    if (-not (Test-Path -LiteralPath $texturesRoot)) {
        Write-Host "[ERROR] Textures folder not found:"
        Write-Host "  $texturesRoot"
        return @{ Total = 0; Converted = 0; Skipped = 0; Failed = 1 }
    }

    $sourceExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp")
    $normalRootPrefix = ($normalRoot.TrimEnd('\') + '\').ToLowerInvariant()

    $candidates = @(Get-ChildItem -LiteralPath $texturesRoot -Recurse -File |
        Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() } |
        Where-Object { -not $_.FullName.ToLowerInvariant().StartsWith($normalRootPrefix) } |
        ForEach-Object {
            $token = Get-NormalMatchToken -BaseName $_.BaseName
            if ($null -ne $token) {
                [PSCustomObject]@{
                    RelativePath = Get-RelativeDisplayPath -RootPath $texturesRoot -FullPath $_.FullName
                    FullPath = $_.FullName
                    BaseName = $_.Name
                    MatchToken = $token
                }
            }
        } | Sort-Object RelativePath)

    Write-Host ""
    Write-Host "============================================="
    Write-Host " NVTT Normal Converter - Map"
    Write-Host "============================================="
    Write-Host " Map              : $MapPath"
    Write-Host " Textures         : $texturesRoot"
    Write-Host " Normal Root      : $normalRoot"
    Write-Host " Intermediate Root: $intermediateRoot"
    Write-Host " Overwrite        : $Overwrite"
    Write-Host " Dry-run          : $DryRun"
    Write-Host "============================================="

    $confirmedCandidates = Confirm-Candidates -Candidates $candidates
    if ($null -eq $confirmedCandidates) {
        return @{ Total = 0; Converted = 0; Skipped = 0; Failed = 0 }
    }

    if ($DryRun) {
        Write-Host "[DRY-RUN] Candidate review completed. No files were copied or converted."
        return @{ Total = $confirmedCandidates.Count; Converted = 0; Skipped = 0; Failed = 0 }
    }

    $converted = 0
    $skipped = 0
    $failed = 0

    foreach ($candidate in $confirmedCandidates) {
        $finalRelativeDds = [System.IO.Path]::ChangeExtension($candidate.RelativePath, ".dds")
        $finalPath = Join-Path $normalRoot $finalRelativeDds

        if ((Test-Path -LiteralPath $finalPath) -and -not $Overwrite) {
            Write-Host "[SKIP] $($candidate.RelativePath) (dds already exists)"
            $skipped++
            continue
        }

        try {
            Write-Host ""
            Write-Host "[COPY] $($candidate.RelativePath)"
            $copiedSource = Join-Path $normalRoot $candidate.RelativePath
            $copiedDir = Split-Path -Parent $copiedSource
            New-Item -ItemType Directory -Path $copiedDir -Force | Out-Null
            Copy-Item -LiteralPath $candidate.FullPath -Destination $copiedSource -Force

            Write-Host "[BC5]  $($candidate.RelativePath)"
            $intermediateDds = Convert-ToBc5Intermediate `
                -TexconvPath $TexconvPath `
                -SourcePath $copiedSource `
                -IntermediateRootPath $intermediateRoot `
                -RelativePath $candidate.RelativePath

            Write-Host "[NVTT] $($candidate.RelativePath)"
            Convert-ToNvttFinal `
                -NvttExportPath $NvttExportPath `
                -PresetPath $nvttPresetPath `
                -IntermediateDdsPath $intermediateDds `
                -FinalPath $finalPath

            $converted++
        }
        catch {
            Write-Host "[FAIL] $($candidate.RelativePath) - $($_.Exception.Message)"
            $failed++
        }
    }

    if (-not $KeepIntermediate -and (Test-Path -LiteralPath $intermediateRoot)) {
        Remove-Item -LiteralPath $intermediateRoot -Recurse -Force -ErrorAction SilentlyContinue
    }

    return @{ Total = $confirmedCandidates.Count; Converted = $converted; Skipped = $skipped; Failed = $failed }
}

function Invoke-ModelNvttConversion {
    param(
        [string]$ModelPath,
        [bool]$Overwrite,
        [string]$TexconvPath,
        [string]$NvttExportPath
    )

    $modelRoot = Join-Path $repoRoot "Client\Bin\Resources\Models\$ModelPath"
    if (-not (Test-Path -LiteralPath $modelRoot)) {
        Write-Host "[ERROR] Model folder not found:"
        Write-Host "  $modelRoot"
        return @{ Total = 0; Converted = 0; Skipped = 0; Failed = 1 }
    }

    $sourceExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp")
    $candidates = @(Get-ChildItem -LiteralPath $modelRoot -Recurse -File |
        Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() } |
        Where-Object { $_.BaseName.IndexOf("Normal", [System.StringComparison]::OrdinalIgnoreCase) -ge 0 } |
        ForEach-Object {
            [PSCustomObject]@{
                RelativePath = Get-RelativeDisplayPath -RootPath $modelRoot -FullPath $_.FullName
                FullPath = $_.FullName
                BaseName = $_.Name
            }
        } | Sort-Object RelativePath)

    Write-Host ""
    Write-Host "============================================="
    Write-Host " NVTT Normal Converter - Model"
    Write-Host "============================================="
    Write-Host " Model     : $ModelPath"
    Write-Host " Folder    : $modelRoot"
    Write-Host " Match     : file name contains 'Normal'"
    Write-Host " Output    : next to the source texture"
    Write-Host " Overwrite : $Overwrite"
    Write-Host " Dry-run   : $DryRun"
    Write-Host "============================================="

    $confirmedCandidates = Confirm-Candidates -Candidates $candidates
    if ($null -eq $confirmedCandidates) {
        return @{ Total = 0; Converted = 0; Skipped = 0; Failed = 0 }
    }

    if ($DryRun) {
        Write-Host "[DRY-RUN] Candidate review completed. No files were converted."
        return @{ Total = $confirmedCandidates.Count; Converted = 0; Skipped = 0; Failed = 0 }
    }

    $tempRoot = Join-Path $env:TEMP ("Expedition_NVTT_Model_" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    $converted = 0
    $skipped = 0
    $failed = 0

    foreach ($candidate in $confirmedCandidates) {
        $finalPath = [System.IO.Path]::ChangeExtension($candidate.FullPath, ".dds")

        if ((Test-Path -LiteralPath $finalPath) -and -not $Overwrite) {
            Write-Host "[SKIP] $($candidate.RelativePath) (dds already exists)"
            $skipped++
            continue
        }

        try {
            Write-Host ""
            Write-Host "[BC5]  $($candidate.RelativePath)"
            $intermediateDds = Convert-ToBc5Intermediate `
                -TexconvPath $TexconvPath `
                -SourcePath $candidate.FullPath `
                -IntermediateRootPath $tempRoot `
                -RelativePath $candidate.RelativePath

            Write-Host "[NVTT] $($candidate.RelativePath)"
            Convert-ToNvttFinal `
                -NvttExportPath $NvttExportPath `
                -PresetPath $nvttPresetPath `
                -IntermediateDdsPath $intermediateDds `
                -FinalPath $finalPath

            $converted++
        }
        catch {
            Write-Host "[FAIL] $($candidate.RelativePath) - $($_.Exception.Message)"
            $failed++
        }
    }

    if (-not $KeepIntermediate -and (Test-Path -LiteralPath $tempRoot)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }

    return @{ Total = $confirmedCandidates.Count; Converted = $converted; Skipped = $skipped; Failed = $failed }
}

Write-Host ""
Write-Host "============================================="
Write-Host " NVTT Normal Converter To DDS"
Write-Host "============================================="
Write-Host " 1. Map"
Write-Host " 2. Model"
Write-Host " 3. Effect"
Write-Host " 4. All"
Write-Host "============================================="
$targetMode = (Read-Host "Select target [1-4]").Trim()

if ($targetMode -notin @("1", "2", "3", "4")) {
    Write-Host "[ERROR] Invalid target selection."
    exit 1
}

if ($targetMode -eq "3") {
    Write-Host "[NOT IMPLEMENTED] Effect NVTT conversion is not implemented. Use BasicConverterToDDS instead."
    exit 0
}

Write-Host ""
Write-Host "Existing DDS handling:"
Write-Host " 1. Overwrite existing DDS"
Write-Host " 2. Skip when DDS already exists"
$overwriteMode = (Read-Host "Select mode [1-2]").Trim()

switch ($overwriteMode) {
    "1" { $Overwrite = $true }
    "2" { $Overwrite = $false }
    default {
        Write-Host "[ERROR] Invalid overwrite selection."
        exit 1
    }
}

$mapPath = ""
$modelPath = ""

if ($targetMode -eq "1" -or $targetMode -eq "4") {
    Write-Host ""
    Write-Host "Enter a path relative to Resources\Map."
    Write-Host "Example: SmallLevel_MF_Zone_01"
    $mapPath = (Read-Host "Map path").Trim()
    if ([string]::IsNullOrWhiteSpace($mapPath)) {
        Write-Host "[ERROR] Map path is required."
        exit 1
    }
}

if ($targetMode -eq "2" -or $targetMode -eq "4") {
    Write-Host ""
    Write-Host "Enter a path relative to Resources\Models."
    Write-Host "Example: Heros\Lune"
    $modelPath = (Read-Host "Model path").Trim()
    if ([string]::IsNullOrWhiteSpace($modelPath)) {
        Write-Host "[ERROR] Model path is required."
        exit 1
    }
}

if ($targetMode -eq "4") {
    Write-Host "[INFO] Effect NVTT conversion is not implemented. Effect is skipped; use BasicConverterToDDS instead."
}

if (-not (Test-Path -LiteralPath $nvttPresetPath)) {
    Write-Host "[ERROR] NVTT preset not found:"
    Write-Host "  $nvttPresetPath"
    exit 1
}

$texconv = $null
$nvttExport = $null
if (-not $DryRun) {
    $texconv = Resolve-TexconvPath
    if ($null -eq $texconv) {
        Write-Host "[ERROR] texconv.exe not found. Place it next to this script or set TEXCONV_PATH."
        exit 1
    }

    $nvttExport = Resolve-NvttExportPath
    if ($null -eq $nvttExport) {
        Write-Host "[ERROR] nvtt_export.exe not found. Place it next to this script, add it to PATH, or set NVTT_EXPORT_PATH."
        exit 1
    }
}

$grandTotal = 0
$grandConverted = 0
$grandSkipped = 0
$grandFailed = 0

if ($targetMode -eq "1" -or $targetMode -eq "4") {
    $result = Invoke-MapNvttConversion `
        -MapPath $mapPath `
        -Overwrite $Overwrite `
        -TexconvPath $texconv `
        -NvttExportPath $nvttExport

    $grandTotal += $result.Total
    $grandConverted += $result.Converted
    $grandSkipped += $result.Skipped
    $grandFailed += $result.Failed
}

if ($targetMode -eq "2" -or $targetMode -eq "4") {
    $result = Invoke-ModelNvttConversion `
        -ModelPath $modelPath `
        -Overwrite $Overwrite `
        -TexconvPath $texconv `
        -NvttExportPath $nvttExport

    $grandTotal += $result.Total
    $grandConverted += $result.Converted
    $grandSkipped += $result.Skipped
    $grandFailed += $result.Failed
}

Write-Host ""
Write-Host "============================================="
Write-Host " Done"
Write-Host "============================================="
Write-Host " Total    : $grandTotal"
Write-Host " Converted: $grandConverted"
Write-Host " Skipped  : $grandSkipped"
Write-Host " Failed   : $grandFailed"
Write-Host "============================================="
if ($grandFailed -gt 0) { exit 1 }
exit 0
