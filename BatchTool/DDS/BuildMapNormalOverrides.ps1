$ErrorActionPreference = "Stop"

$LevelName = ""
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

    $LevelName = $arg
}

if ([string]::IsNullOrWhiteSpace($LevelName)) {
    Write-Host "[ERROR] Level name is required."
    Write-Host "Usage: BuildMapNormalOverrides.bat <LevelName> [--dry-run] [--keep-intermediate]"
    exit 1
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$texturesRoot = Join-Path $repoRoot "Client\Bin\Resources\Map\$LevelName\Textures"
$normalRoot = Join-Path $texturesRoot "Normal"
$intermediateRoot = Join-Path $normalRoot "_Intermediate\BC5"
$nvttPresetPath = Join-Path $scriptDir "nvtt_normal.dpf"

if (-not (Test-Path -LiteralPath $texturesRoot)) {
    Write-Host "[ERROR] Textures folder not found:"
    Write-Host "  $texturesRoot"
    exit 1
}

if (-not (Test-Path -LiteralPath $nvttPresetPath)) {
    Write-Host "[ERROR] NVTT preset not found:"
    Write-Host "  $nvttPresetPath"
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

function Resolve-NvttExportPath {
    $localNvtt = Join-Path $scriptDir "nvtt_export.exe"
    if (Test-Path -LiteralPath $localNvtt) {
        return $localNvtt
    }

    if (-not [string]::IsNullOrWhiteSpace($env:NVTT_EXPORT_PATH) -and (Test-Path -LiteralPath $env:NVTT_EXPORT_PATH)) {
        return $env:NVTT_EXPORT_PATH
    }

    $pathNvtt = Get-Command "nvtt_export.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathNvtt) {
        return $pathNvtt.Source
    }

    return $null
}

function Get-NormalMatchToken {
    param([string]$BaseName)

    $orderedTokens = @("_NMAo", "_NAo", "_NR", "Normal", "_N")
    foreach ($token in $orderedTokens) {
        if ($BaseName.IndexOf($token, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            return $token
        }
    }

    return $null
}

function Get-RelativeDisplayPath {
    param(
        [string]$RootPath,
        [string]$FullPath
    )

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

function Copy-NormalSource {
    param(
        [object]$Candidate,
        [string]$NormalRootPath
    )

    $targetPath = Join-Path $NormalRootPath $Candidate.RelativePath
    $targetDir = Split-Path -Parent $targetPath
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    Copy-Item -LiteralPath $Candidate.FullPath -Destination $targetPath -Force
    return $targetPath
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
        [string]$NormalRootPath,
        [string]$RelativePath
    )

    $finalRelativeDds = [System.IO.Path]::ChangeExtension($RelativePath, ".dds")
    $finalPath = Join-Path $NormalRootPath $finalRelativeDds
    $finalDir = Split-Path -Parent $finalPath
    New-Item -ItemType Directory -Path $finalDir -Force | Out-Null

    & $NvttExportPath --preset $PresetPath --output $finalPath $IntermediateDdsPath | Out-Host
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $finalPath)) {
        throw "NVTT conversion failed for '$RelativePath'."
    }

    return $finalPath
}

$sourceExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp")
$normalRootPrefix = ($normalRoot.TrimEnd('\') + '\').ToLowerInvariant()

$candidates = Get-ChildItem -LiteralPath $texturesRoot -Recurse -File |
    Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() } |
    Where-Object { -not $_.FullName.ToLowerInvariant().StartsWith($normalRootPrefix) } |
    ForEach-Object {
        $token = Get-NormalMatchToken -BaseName $_.BaseName
        if ($null -eq $token) {
            return
        }

        [PSCustomObject]@{
            RelativePath = (Get-RelativeDisplayPath -RootPath $texturesRoot -FullPath $_.FullName)
            FullPath = $_.FullName
            BaseName = $_.Name
            MatchToken = $token
        }
    } |
    Sort-Object RelativePath

$basenameCounts = @{}
foreach ($candidate in $candidates) {
    if (-not $basenameCounts.ContainsKey($candidate.BaseName)) {
        $basenameCounts[$candidate.BaseName] = 0
    }

    $basenameCounts[$candidate.BaseName]++
}

Write-Host "============================================="
Write-Host " Map Normal Override Builder"
Write-Host "============================================="
Write-Host " Level            : $LevelName"
Write-Host " Textures         : $texturesRoot"
Write-Host " Normal Root      : $normalRoot"
Write-Host " Intermediate Root: $intermediateRoot"
Write-Host " Dry-run          : $DryRun"
Write-Host "============================================="

$confirmedCandidates = [System.Collections.Generic.List[object]]::new()
foreach ($candidate in $candidates) {
    $confirmedCandidates.Add($candidate)
}

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
        break
    }

    if ($trimmed.Equals("cancel", [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host "[CANCELLED] No files were changed."
        exit 0
    }

    $relativeMatches = @($confirmedCandidates | Where-Object {
        $_.RelativePath.Equals($trimmed, [System.StringComparison]::OrdinalIgnoreCase)
    })

    $match = $null
    if ($relativeMatches.Count -eq 1) {
        $match = $relativeMatches[0]
    } elseif ($relativeMatches.Count -gt 1) {
        Write-Host "[NO MATCH] Multiple relative-path matches found unexpectedly. Use the exact displayed relative path."
        continue
    } else {
        $basenameMatches = @($confirmedCandidates | Where-Object {
            $_.BaseName.Equals($trimmed, [System.StringComparison]::OrdinalIgnoreCase)
        })

        if ($basenameMatches.Count -eq 1 -and $basenameCounts[$basenameMatches[0].BaseName] -eq 1) {
            $match = $basenameMatches[0]
        } elseif ($basenameMatches.Count -gt 0) {
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

if ($DryRun) {
    Write-Host "[DRY-RUN] Candidate review completed. No files were copied or converted."
    exit 0
}

$texconv = Resolve-TexconvPath
if ($null -eq $texconv) {
    Write-Host "[ERROR] texconv.exe not found. Place texconv.exe next to this script or set TEXCONV_PATH."
    exit 1
}

$nvttExport = Resolve-NvttExportPath
if ($null -eq $nvttExport) {
    Write-Host "[ERROR] nvtt_export.exe not found. Place it next to this script, add it to PATH, or set NVTT_EXPORT_PATH."
    exit 1
}

$copied = 0
$bc5Converted = 0
$nvttConverted = 0
$failed = 0

foreach ($candidate in $confirmedCandidates) {
    try {
        Write-Host ""
        Write-Host "[COPY] $($candidate.RelativePath)"
        $copiedSource = Copy-NormalSource -Candidate $candidate -NormalRootPath $normalRoot
        $copied++

        Write-Host "[BC5]  $($candidate.RelativePath)"
        $intermediateDds = Convert-ToBc5Intermediate `
            -TexconvPath $texconv `
            -SourcePath $copiedSource `
            -IntermediateRootPath $intermediateRoot `
            -RelativePath $candidate.RelativePath
        $bc5Converted++

        Write-Host "[NVTT] $($candidate.RelativePath)"
        [void](Convert-ToNvttFinal `
            -NvttExportPath $nvttExport `
            -PresetPath $nvttPresetPath `
            -IntermediateDdsPath $intermediateDds `
            -NormalRootPath $normalRoot `
            -RelativePath $candidate.RelativePath)
        $nvttConverted++
    } catch {
        Write-Host "[FAIL] $($candidate.RelativePath) - $($_.Exception.Message)"
        $failed++
    }
}

if (-not $KeepIntermediate -and (Test-Path -LiteralPath $intermediateRoot)) {
    Remove-Item -LiteralPath $intermediateRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "============================================="
Write-Host " Done"
Write-Host "============================================="
Write-Host " Confirmed : $($confirmedCandidates.Count)"
Write-Host " Copied    : $copied"
Write-Host " BC5       : $bc5Converted"
Write-Host " NVTT      : $nvttConverted"
Write-Host " Failed    : $failed"
Write-Host "============================================="

if ($failed -gt 0) {
    exit 1
}

exit 0
