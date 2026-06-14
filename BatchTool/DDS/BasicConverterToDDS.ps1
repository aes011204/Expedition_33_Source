$ErrorActionPreference = "Stop"

$DryRun = $false
foreach ($arg in $args) {
    if ($arg -ieq "--dry-run" -or $arg -ieq "-DryRun") {
        $DryRun = $true
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")

Write-Host ""
Write-Host "============================================="
Write-Host " Basic Converter To DDS"
Write-Host "============================================="
Write-Host " 1. Map"
Write-Host " 2. Model"
Write-Host " 3. Effect"
Write-Host " 4. All"
Write-Host "============================================="
$TargetMode = (Read-Host "Select target [1-4]").Trim()

if ($TargetMode -notin @("1", "2", "3", "4")) {
    Write-Host "[ERROR] Invalid target selection."
    exit 1
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

$textureDirs = @()
$targetNames = @()
$LevelName = ""

if ($TargetMode -eq "1" -or $TargetMode -eq "4") {
    Write-Host ""
    Write-Host "Enter a path relative to Resources\Map."
    Write-Host "Example: SmallLevel_MF_Zone_01"
    $LevelName = (Read-Host "Map path").Trim()
    if ([string]::IsNullOrWhiteSpace($LevelName)) {
        Write-Host "[ERROR] Map path is required."
        exit 1
    }

    $textureDirs += Join-Path $repoRoot "Client\Bin\Resources\Map\$LevelName\Textures"
    $targetNames += "Map:$LevelName"
}

if ($TargetMode -eq "2" -or $TargetMode -eq "4") {
    Write-Host ""
    Write-Host "Enter a path relative to Resources\Models."
    Write-Host "Example: Heros\Lune"
    $modelPath = (Read-Host "Model path").Trim()
    if ([string]::IsNullOrWhiteSpace($modelPath)) {
        Write-Host "[ERROR] Model path is required."
        exit 1
    }

    $textureDirs += Join-Path $repoRoot "Client\Bin\Resources\Models\$modelPath"
    $targetNames += "Model:$modelPath"
}

if ($TargetMode -eq "3" -or $TargetMode -eq "4") {
    Write-Host ""
    Write-Host "Enter a path relative to Resources\Effects."
    Write-Host "Example: Character\Lune"
    $effectPath = (Read-Host "Effect path").Trim()
    if ([string]::IsNullOrWhiteSpace($effectPath)) {
        Write-Host "[ERROR] Effect path is required."
        exit 1
    }

    $textureDirs += Join-Path $repoRoot "Client\Bin\Resources\Effects\$effectPath"
    $targetNames += "Effect:$effectPath"
}

$targetName = $targetNames -join ", "
$missingDirs = @($textureDirs | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missingDirs.Count -gt 0) {
    Write-Host "[ERROR] Target folder not found:"
    foreach ($dir in $missingDirs) { Write-Host "  $dir" }
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

function Test-IsPureNormalName {
    param([string]$FileName)

    $name = $FileName.ToLowerInvariant()
    $isPackedNormal =
        ($name -match "(^|[_\-\.\+])nmao([_\-\.\+]|$)") -or
        ($name -match "(^|[_\-\.\+])nao([_\-\.\+]|$)") -or
        ($name -match "(^|[_\-\.\+])nme([_\-\.\+]|$)") -or
        ($name -match "(^|[_\-\.\+])nre([_\-\.\+]|$)") -or
        ($name -match "normal.*rough|normal.*metal|normal.*emissive|normal.*mask")

    if ($isPackedNormal) { return $false }
    return $name -match "normal|normals|norm|nrm|(^|[_\-\.])n([_\-\.]|$)"
}

function Get-DdsClassification {
    param([string]$FileName)

    $name = $FileName.ToLowerInvariant()

    if (Test-IsPureNormalName $FileName) {
        return [PSCustomObject]@{
            Format = "BC5_UNORM"
            Kind = "Normal"
        }
    }

    $isPackedColor = $name -match "(^|_)(ar)(_|\.|$)|albedo.*roughness|basecolor.*roughness|base_color.*roughness|diffuse.*roughness|albedoalpha|basecoloralpha|coloralpha|colouralpha"
    $isColor = $name -match "basecolor|base_color|albedo|diffuse|diff|emissive|color|colour|_bc|_c\b|_d\b|_albedo"

    if ($isPackedColor -or $isColor) {
        return [PSCustomObject]@{
            Format = "BC7_UNORM_SRGB"
            Kind = "Color"
        }
    }

    return [PSCustomObject]@{
        Format = "BC7_UNORM"
        Kind = "Data"
    }
}

function Get-NormalizedDdsPathKey {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    $normalized = $Path.Replace('\', '/').Trim().ToLowerInvariant()
    $extension = [System.IO.Path]::GetExtension($normalized)
    if (-not [string]::IsNullOrWhiteSpace($extension)) {
        $normalized = $normalized.Substring(0, $normalized.Length - $extension.Length)
    }

    return $normalized + ".dds"
}

function Get-TextureStemKey {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    return [System.IO.Path]::GetFileNameWithoutExtension($Path).ToLowerInvariant()
}

function Add-TextureClassificationHint {
    param(
        [hashtable]$Index,
        [string]$Key,
        [bool]$IsColor
    )

    if ($null -eq $Index -or [string]::IsNullOrWhiteSpace($Key)) {
        return
    }

    if ($Index.ContainsKey($Key)) {
        if ($IsColor) {
            $Index[$Key] = $true
        }
        return
    }

    $Index[$Key] = $IsColor
}

function Add-TextureSemanticHint {
    param(
        [hashtable]$Index,
        [string]$TexturePath,
        [bool]$IsColor
    )

    $pathKey = Get-NormalizedDdsPathKey $TexturePath
    $stemKey = Get-TextureStemKey $TexturePath

    Add-TextureClassificationHint -Index $Index -Key $pathKey -IsColor $IsColor
    Add-TextureClassificationHint -Index $Index -Key $stemKey -IsColor $IsColor
}

function Test-IsColorSemanticRole {
    param([string]$Role)

    $normalized = $Role.ToLowerInvariant()
    return $normalized -match "basecolor|base_color|albedo|diffuse|color|colour|emissive|subsurface"
}

function Test-IsDataSemanticRole {
    param([string]$Role)

    $normalized = $Role.ToLowerInvariant()
    return $normalized -match "normal|orm|roughness|metal|spec|mask|opacity|coverage|ao|height|depth|cavity|id|iris"
}

function Resolve-SemanticClassification {
    param([bool]$IsColor)

    if ($IsColor) {
        return [PSCustomObject]@{
            Format = "BC7_UNORM_SRGB"
            Kind = "Color"
        }
    }

    return [PSCustomObject]@{
        Format = "BC7_UNORM"
        Kind = "Data"
    }
}

function Get-TextureDomain {
    param([string]$FilePath)

    $normalized = $FilePath.Replace('\', '/').ToLowerInvariant()
    if ($normalized.Contains("/resources/map/")) {
        return "Map"
    }

    if ($normalized.Contains("/resources/effects/")) {
        return "Effects"
    }

    if ($normalized.Contains("/resources/models/")) {
        return "Models"
    }

    return "Unknown"
}

function Get-ResourceRelativeDdsPath {
    param(
        [string]$FullPath,
        [string]$ResourcesRoot
    )

    $fullPathNormalized = Get-NormalizedDdsPathKey $FullPath
    $resourcesRootNormalized = $ResourcesRoot.Replace('\', '/').TrimEnd('/').ToLowerInvariant()
    if (-not $fullPathNormalized.StartsWith($resourcesRootNormalized)) {
        return ""
    }

    $relative = $fullPathNormalized.Substring($resourcesRootNormalized.Length).TrimStart('/')
    return $relative
}

function Build-MapTextureSemanticIndex {
    param(
        [string]$RepoRoot,
        [string]$LevelName
    )

    $index = @{}
    $needTextureRoot = Join-Path $RepoRoot "Client\Bin\Resources\Map\NeedTextureJson"
    if (-not (Test-Path -LiteralPath $needTextureRoot)) {
        return $index
    }

    $txtFiles = @()
    if ([string]::IsNullOrWhiteSpace($LevelName)) {
        $txtFiles = Get-ChildItem -LiteralPath $needTextureRoot -Filter "*_textures.txt" -File
    }
    else {
        $candidate = Join-Path $needTextureRoot ($LevelName + "_textures.txt")
        if (Test-Path -LiteralPath $candidate) {
            $txtFiles = @(Get-Item -LiteralPath $candidate)
        }
    }

    foreach ($txtFile in $txtFiles) {
        $levelStem = [System.IO.Path]::GetFileNameWithoutExtension($txtFile.Name)
        $levelKey = $levelStem -replace "_textures$", ""
        $currentTextureName = ""

        foreach ($rawLine in Get-Content -LiteralPath $txtFile.FullName) {
            $line = $rawLine.Trim()
            if ([string]::IsNullOrWhiteSpace($line)) {
                continue
            }

            if ($line.StartsWith("Texture extraction list", [System.StringComparison]::OrdinalIgnoreCase) -or
                $line.StartsWith("Save extracted textures", [System.StringComparison]::OrdinalIgnoreCase) -or
                $line.StartsWith("Resources/", [System.StringComparison]::OrdinalIgnoreCase) -or
                $line.StartsWith("==")) {
                continue
            }

            if ($line.StartsWith("Source :", [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }

            if ($line.StartsWith("Used by:", [System.StringComparison]::OrdinalIgnoreCase)) {
                if ([string]::IsNullOrWhiteSpace($currentTextureName)) {
                    continue
                }

                $relativeTexturePath = "Map/$levelKey/Textures/$currentTextureName.dds"
                $roleMatches = [regex]::Matches($line, "\(([^)]+)\)")
                foreach ($match in $roleMatches) {
                    $role = $match.Groups[1].Value
                    if (Test-IsColorSemanticRole $role) {
                        Add-TextureSemanticHint -Index $index -TexturePath $relativeTexturePath -IsColor $true
                    }
                    elseif (Test-IsDataSemanticRole $role) {
                        Add-TextureSemanticHint -Index $index -TexturePath $relativeTexturePath -IsColor $false
                    }
                }

                continue
            }

            $currentTextureName = $line
        }
    }

    return $index
}

function Add-EffectTextureHintsFromNode {
    param(
        $Node,
        [hashtable]$Index
    )

    if ($null -eq $Node -or $Node -is [string] -or $Node -is [ValueType]) {
        return
    }

    if ($Node -is [System.Collections.IEnumerable] -and -not ($Node -is [System.Management.Automation.PSCustomObject])) {
        foreach ($item in $Node) {
            Add-EffectTextureHintsFromNode -Node $item -Index $Index
        }
        return
    }

    foreach ($property in $Node.PSObject.Properties) {
        $propertyName = $property.Name
        $propertyValue = $property.Value

        switch -Regex ($propertyName) {
            "^mainTexturePath$" {
                Add-TextureSemanticHint -Index $Index -TexturePath $propertyValue -IsColor $true
                continue
            }
            "^(noiseTexturePath|maskTexturePath)$" {
                Add-TextureSemanticHint -Index $Index -TexturePath $propertyValue -IsColor $false
                continue
            }
        }

        Add-EffectTextureHintsFromNode -Node $propertyValue -Index $Index
    }
}

function Build-EffectTextureSemanticIndex {
    param([string]$RepoRoot)

    $index = @{}
    $effectsRoot = Join-Path $RepoRoot "Client\Bin\Resources\Effects"
    if (-not (Test-Path -LiteralPath $effectsRoot)) {
        return $index
    }

    $metadataFiles = Get-ChildItem -LiteralPath $effectsRoot -Recurse -File |
        Where-Object { $_.Name.EndsWith(".effectmaterial.json", [System.StringComparison]::OrdinalIgnoreCase) -or $_.Name.EndsWith(".effect.json", [System.StringComparison]::OrdinalIgnoreCase) }

    foreach ($metadataFile in $metadataFiles) {
        try {
            $jsonRoot = Get-Content -LiteralPath $metadataFile.FullName -Raw | ConvertFrom-Json
            Add-EffectTextureHintsFromNode -Node $jsonRoot -Index $index
        }
        catch {
            Write-Host "[WARN] Effect metadata parse failed: $($metadataFile.FullName)"
        }
    }

    return $index
}

function Get-TexturePathFromMaterialSlot {
    param($SlotValue)

    if ($null -eq $SlotValue) {
        return ""
    }

    if ($SlotValue -is [string]) {
        return $SlotValue
    }

    $texturePathProperty = $SlotValue.PSObject.Properties["texture_path"]
    if ($null -ne $texturePathProperty -and $texturePathProperty.Value -is [string]) {
        return $texturePathProperty.Value
    }

    return ""
}

function Build-ModelTextureSemanticIndex {
    param([string]$RepoRoot)

    $index = @{}
    $materialsRoot = Join-Path $RepoRoot "Client\Bin\Resources\Materials"
    if (-not (Test-Path -LiteralPath $materialsRoot)) {
        return $index
    }

    $materialFiles = Get-ChildItem -LiteralPath $materialsRoot -Recurse -Filter "*.material.json" -File
    foreach ($materialFile in $materialFiles) {
        try {
            $jsonRoot = Get-Content -LiteralPath $materialFile.FullName -Raw | ConvertFrom-Json
        }
        catch {
            Write-Host "[WARN] Material metadata parse failed: $($materialFile.FullName)"
            continue
        }

        foreach ($instance in @($jsonRoot.instances)) {
            if ($null -eq $instance) {
                continue
            }

            $textures = $instance.textures
            if ($null -eq $textures) {
                continue
            }

            foreach ($slotProperty in $textures.PSObject.Properties) {
                $slotName = $slotProperty.Name
                $texturePath = Get-TexturePathFromMaterialSlot $slotProperty.Value
                if ([string]::IsNullOrWhiteSpace($texturePath)) {
                    continue
                }

                if (Test-IsColorSemanticRole $slotName) {
                    Add-TextureSemanticHint -Index $index -TexturePath $texturePath -IsColor $true
                }
                elseif (Test-IsDataSemanticRole $slotName) {
                    Add-TextureSemanticHint -Index $index -TexturePath $texturePath -IsColor $false
                }
            }
        }
    }

    return $index
}

function Resolve-DdsClassificationForFile {
    param(
        [System.IO.FileInfo]$File,
        [hashtable]$MapIndex,
        [hashtable]$EffectIndex,
        [hashtable]$ModelIndex,
        [string]$ResourcesRoot
    )

    if (Test-IsPureNormalName $File.Name) {
        return [PSCustomObject]@{
            Format = "BC5_UNORM"
            Kind = "Normal"
        }
    }

    $domain = Get-TextureDomain $File.FullName
    $relativePathKey = Get-ResourceRelativeDdsPath -FullPath $File.FullName -ResourcesRoot $ResourcesRoot
    $stemKey = Get-TextureStemKey $File.Name

    $candidateKeys = @()
    if (-not [string]::IsNullOrWhiteSpace($relativePathKey)) {
        $candidateKeys += $relativePathKey
    }
    if (-not [string]::IsNullOrWhiteSpace($stemKey)) {
        $candidateKeys += $stemKey
    }

    $domainIndex = $null
    switch ($domain) {
        "Map" { $domainIndex = $MapIndex }
        "Effects" { $domainIndex = $EffectIndex }
        "Models" { $domainIndex = $ModelIndex }
    }

    if ($null -ne $domainIndex) {
        foreach ($key in $candidateKeys) {
            if ($domainIndex.ContainsKey($key)) {
                return Resolve-SemanticClassification -IsColor ([bool]$domainIndex[$key])
            }
        }
    }

    return Get-DdsClassification $File.Name
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

$resourcesRoot = Join-Path $repoRoot "Client\Bin\Resources"
$mapTextureSemanticIndex = Build-MapTextureSemanticIndex -RepoRoot $repoRoot -LevelName $LevelName
$effectTextureSemanticIndex = Build-EffectTextureSemanticIndex -RepoRoot $repoRoot
$modelTextureSemanticIndex = Build-ModelTextureSemanticIndex -RepoRoot $repoRoot

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
    $classification = Resolve-DdsClassificationForFile `
        -File $file `
        -MapIndex $mapTextureSemanticIndex `
        -EffectIndex $effectTextureSemanticIndex `
        -ModelIndex $modelTextureSemanticIndex `
        -ResourcesRoot $resourcesRoot
    $format = $classification.Format

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

    switch ($classification.Kind) {
        "Color" {
            # Color: BC7 sRGB with the full mip chain.
            & $texconv -y -ft dds -f $format -m 0 -o $file.DirectoryName $file.FullName | Out-Host
        }
        "Normal" {
            # Pure normal: BC5 with the full mip chain.
            & $texconv -y -ft dds -f $format -m 0 -o $file.DirectoryName $file.FullName | Out-Host
        }
        default {
            # Other linear data keeps the existing single-mip behavior.
            & $texconv -y -ft dds -f $format -m 1 -o $file.DirectoryName $file.FullName | Out-Host
        }
    }

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
