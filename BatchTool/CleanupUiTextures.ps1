param(
    [switch]$DryRunOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$uiRoot = Join-Path $repoRoot "Client\Bin\Resources\Textures\UI"
$resourceRoot = Join-Path $repoRoot "Client\Bin\Resources"
$assetDataRoot = Join-Path $resourceRoot "Data\Assets"
$dryRunManifestPath = Join-Path $assetDataRoot "UITextureCleanupManifest.dry-run.json"
$manifestPath = Join-Path $assetDataRoot "UITextureCleanupManifest.json"

$allowedTopFolders = @(
    "Battle",
    "Character",
    "Common",
    "Exploration",
    "GameMenu",
    "Inventory",
    "Progression",
    "Scene",
    "WorldMap"
)

function Get-FullPath([string]$path) {
    return [System.IO.Path]::GetFullPath($path)
}

function Assert-UnderRoot([string]$path, [string]$root) {
    $full = Get-FullPath $path
    $rootFull = Get-FullPath $root
    $rootPrefix = $rootFull
    if (-not $rootPrefix.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $rootPrefix += [System.IO.Path]::DirectorySeparatorChar
    }

    if ($full.Equals($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return
    }

    if (-not $full.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes root. path='$full' root='$rootFull'"
    }
}

function Normalize-Rel([string]$rel) {
    return ($rel -replace "/", "\").TrimStart(".\")
}

function Join-Rel([string[]]$parts) {
    return (($parts | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join "\")
}

function Get-Tail([string[]]$parts, [int]$startIndex) {
    if ($parts.Count -le $startIndex) {
        return @()
    }

    return $parts[$startIndex..($parts.Count - 1)]
}

function Normalize-Components([string[]]$parts) {
    $normalized = foreach ($part in $parts) {
        switch -Regex ($part) {
            "^(UniqueMecanics|UniqueMechaniques)$" { "UniqueMechanics"; continue }
            "^Level_POI$" { "LevelPOI"; continue }
            "^Enemies_TurnOver$" { "Enemies"; continue }
            "^CrossAir$" { "Targeting"; continue }
            default { $part }
        }
    }

    return @($normalized)
}

function Get-OriginDestinationDir([string[]]$dirParts, [string]$fileName) {
    if ($dirParts.Count -eq 0) {
        return "Common\Generic"
    }

    $first = $dirParts[0]
    $second = if ($dirParts.Count -gt 1) { $dirParts[1] } else { "" }

    switch ($first) {
        "Icons" {
            switch ($second) {
                "Wardrobe" { return Join-Rel (@("Character", "Wardrobe") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Skills" { return Join-Rel (@("Progression", "Skills") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Weapons" { return Join-Rel (@("Inventory", "Weapons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Items" { return Join-Rel (@("Inventory", "Items") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Consumables" { return Join-Rel (@("Inventory", "Consumables") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "UniqueMechaniques" { return Join-Rel (@("Progression", "UniqueMechanics") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "StatusEffects" { return Join-Rel (@("Progression", "StatusEffects") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Picto" { return Join-Rel (@("Progression", "Picto") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Levels" { return Join-Rel (@("WorldMap", "LevelIcons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Elements" { return Join-Rel (@("Progression", "Elements") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "SkillDescriptions" { return Join-Rel (@("Progression", "SkillDescriptions") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Indicators" { return Join-Rel (@("Common", "Indicators") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Menus" { return Join-Rel (@("GameMenu", "Icons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Common", "Icons") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "TurnOrder" {
            if ($second -eq "Enemies") {
                return Join-Rel (@("Battle", "TurnOrder", "Enemies") + (Normalize-Components (Get-Tail $dirParts 2)))
            }
            return Join-Rel (@("Battle", "TurnOrder") + (Normalize-Components (Get-Tail $dirParts 1)))
        }
        "HUD" {
            switch ($second) {
                "UniqueMecanics" { return Join-Rel (@("Battle", "UniqueMechanics") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "BattleWheel" { return Join-Rel (@("Battle", "BattleWheel") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "HealthBar" { return Join-Rel (@("Battle", "HUD", "HealthBar") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "EndBattle" { return Join-Rel (@("Battle", "EndBattle") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "PopUp_Damage" { return Join-Rel (@("Battle", "DamagePopup") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "LevelAnnounce" { return Join-Rel (@("Exploration", "LevelAnnounce") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Interact" { return Join-Rel (@("Exploration", "Interact") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "ToolTip" { return Join-Rel (@("GameMenu", "Tooltips") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Battle", "HUD") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "GameMenu" {
            switch ($second) {
                "Blacksmith" { return Join-Rel (@("GameMenu", "Blacksmith") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Merchant" { return Join-Rel (@("GameMenu", "Merchant") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Settings" { return Join-Rel (@("GameMenu", "Settings") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Journal" { return Join-Rel (@("GameMenu", "Journal") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "TeamOverview" { return Join-Rel (@("GameMenu", "TeamOverview") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Lumina" { return Join-Rel (@("Progression", "Lumina") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Stats" { return Join-Rel (@("Progression", "Stats") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Pictos" { return Join-Rel (@("Progression", "Picto") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "PortraitsCharacters" { return Join-Rel (@("Character", "Portraits") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Backgrounds" { return Join-Rel (@("GameMenu", "Backgrounds") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Outlines" { return Join-Rel (@("GameMenu", "Outlines") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Ico" { return Join-Rel (@("GameMenu", "Icons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Generic" { return Join-Rel (@("GameMenu", "Common") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("GameMenu", "Common") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "Generic" {
            switch ($second) {
                "Inputs" { return Join-Rel (@("Common", "Input") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "BackgroundPaint" { return Join-Rel (@("Common", "Paint") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Modal" { return Join-Rel (@("Common", "Modal") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "ScrollBar" { return Join-Rel (@("Common", "ScrollBar") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Notifications" { return Join-Rel (@("Common", "Notifications") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Favorite" { return Join-Rel (@("Common", "Icons", "Favorite") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Icons" { return Join-Rel (@("Common", "Icons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Common", "Generic") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "MiniMap" {
            if ($second -eq "Level_POI") {
                return Join-Rel (@("WorldMap", "MiniMap", "LevelPOI") + (Normalize-Components (Get-Tail $dirParts 2)))
            }
            return Join-Rel (@("WorldMap", "MiniMap") + (Normalize-Components (Get-Tail $dirParts 1)))
        }
        "CharactersWidget" {
            switch ($second) {
                "BattlePortraits" { return Join-Rel (@("Battle", "HUD", "CharacterPortraits") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "VictoryPortraits" { return Join-Rel (@("Character", "Victory") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Character", "CharacterHUD") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "Exploration" { return Join-Rel (@("Exploration") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "WheelBattle" {
            if ($second -eq "SkillTexture") {
                return Join-Rel (@("Battle", "BattleWheel", "Skills") + (Normalize-Components (Get-Tail $dirParts 2)))
            }
            return Join-Rel (@("Battle", "BattleWheel") + (Normalize-Components (Get-Tail $dirParts 1)))
        }
        "Buttons" {
            switch ($second) {
                "Mask" { return Join-Rel (@("Common", "Masks") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Common", "Buttons") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "RestMenu" { return Join-Rel (@("GameMenu", "Rest") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "MainMenu" { return Join-Rel (@("Scene", "MainMenu") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "LoadingScreen" { return Join-Rel (@("Scene", "Loading") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "StatModifier" { return Join-Rel (@("Progression", "StatModifiers") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "BattleGradient" { return Join-Rel (@("Battle", "Gradients") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "QTE" { return Join-Rel (@("Battle", "QTE") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "UniqueMechanics" { return Join-Rel (@("Battle", "UniqueMechanics") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "Sandfall" { return Join-Rel (@("Scene", "Sandfall") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "Defense" { return Join-Rel (@("Battle", "Defense") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "DoNotShip" { return Join-Rel (@("Common", "Unclassified", "DoNotShip") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "LifeEnemy" { return Join-Rel (@("Battle", "HUD", "LifeEnemy") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "DriveGauge" { return Join-Rel (@("Battle", "HUD", "DriveGauge") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "Credits" { return Join-Rel (@("Scene", "Credits") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "PhotoMode" { return Join-Rel (@("Scene", "PhotoMode") + (Normalize-Components (Get-Tail $dirParts 1))) }
        default { return Join-Rel (@("Common", "Unclassified") + (Normalize-Components $dirParts)) }
    }
}

function Get-CuratedDestinationDir([string[]]$dirParts, [string]$fileName) {
    if ($dirParts.Count -eq 0) {
        return "Common\Unclassified"
    }

    $first = $dirParts[0]
    $second = if ($dirParts.Count -gt 1) { $dirParts[1] } else { "" }

    if ($allowedTopFolders -contains $first) {
        return Join-Rel (Normalize-Components $dirParts)
    }

    switch ($first) {
        "Blacksmith" { return Join-Rel (@("GameMenu", "Blacksmith") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "Background" {
            if ($fileName -like "*OverlayQuestObjective*") { return "Exploration\Overlay" }
            if ($fileName -like "*SavePoint*") { return "Exploration\SavePoint" }
            if ($fileName -like "*Lumina*") { return "Progression\Lumina" }
            if ($fileName -like "*SkillPoints*") { return "Progression\Skills" }
            if ($fileName -like "*Tooltip*") { return "GameMenu\Tooltips" }
            if ($fileName -eq "T_White.png") { return "Common\Utility" }
            return "GameMenu\Backgrounds"
        }
        "Character" {
            switch ($second) {
                "HUD" { return Join-Rel (@("Character", "CharacterHUD") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Victory" { return Join-Rel (@("Character", "Victory") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Character") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "Font" { return Join-Rel (@("Common", "Font") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "HUD" {
            switch ($second) {
                "Buttons" { return Join-Rel (@("GameMenu", "Buttons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "UniqueMecanics" { return Join-Rel (@("Battle", "UniqueMechanics") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "CrossAir" { return Join-Rel (@("Battle", "Targeting") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "LevelAnnounce" { return Join-Rel (@("Exploration", "LevelAnnounce") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Battle", "HUD") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "Icon" {
            switch ($second) {
                "Skill" { return Join-Rel (@("Progression", "Skills") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "SkillDescriptions" { return Join-Rel (@("Progression", "SkillDescriptions") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "UniqueMechaniques" { return Join-Rel (@("Progression", "UniqueMechanics") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Weapons" { return Join-Rel (@("Inventory", "Weapons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Items" { return Join-Rel (@("Inventory", "Items") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Consumables" { return Join-Rel (@("Inventory", "Consumables") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "StatusEffects" { return Join-Rel (@("Progression", "StatusEffects") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Elements" { return Join-Rel (@("Progression", "Elements") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Picto" { return Join-Rel (@("Progression", "Picto") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Levels" { return Join-Rel (@("WorldMap", "LevelIcons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Indicators" { return Join-Rel (@("Common", "Indicators") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Input" { return Join-Rel (@("Common", "Input") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Arrow" { return Join-Rel (@("Common", "Arrows") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Generic" { return Join-Rel (@("Common", "Icons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Battle" { return Join-Rel (@("Battle", "Icons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Elixir" { return Join-Rel (@("Inventory", "Elixir") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "FileType" { return Join-Rel (@("Common", "FileType") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Menus" { return Join-Rel (@("GameMenu", "Icons") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Wardrobe" { return Join-Rel (@("Character", "Wardrobe") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Common", "Icons") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        "Monster" { return Join-Rel (@("Battle", "TurnOrder", "Enemies") + (Normalize-Components (Get-Tail $dirParts 2))) }
        "Outline" { return Join-Rel (@("GameMenu", "Outlines") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "Overlay" {
            if ($second -eq "Compass") {
                return Join-Rel (@("WorldMap", "Compass") + (Normalize-Components (Get-Tail $dirParts 2)))
            }
            return Join-Rel (@("Exploration", "Overlay") + (Normalize-Components (Get-Tail $dirParts 1)))
        }
        "Pattern" { return Join-Rel (@("Common", "Patterns") + (Normalize-Components (Get-Tail $dirParts 1))) }
        "Scene" {
            switch ($second) {
                "MiniMap" { return Join-Rel (@("WorldMap", "MiniMap") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "Loading" { return Join-Rel (@("Scene", "Loading") + (Normalize-Components (Get-Tail $dirParts 2))) }
                "MainMenu" { return Join-Rel (@("Scene", "MainMenu") + (Normalize-Components (Get-Tail $dirParts 2))) }
                default { return Join-Rel (@("Scene") + (Normalize-Components (Get-Tail $dirParts 1))) }
            }
        }
        default { return Join-Rel (@("Common", "Unclassified") + (Normalize-Components $dirParts)) }
    }
}

function Get-DestinationDir([string]$oldRel, [string]$fileName) {
    $rel = Normalize-Rel $oldRel
    $parts = @($rel -split "\\")
    $dirParts = if ($parts.Count -gt 1) { @($parts[0..($parts.Count - 2)]) } else { @() }

    if ($dirParts.Count -gt 0 -and $dirParts[0] -eq "origin") {
        if ($dirParts.Count -ge 2 -and $dirParts[1] -eq "Textures") {
            $originParts = if ($dirParts.Count -gt 2) { @($dirParts[2..($dirParts.Count - 1)]) } else { @() }
            return Get-OriginDestinationDir $originParts $fileName
        }

        return Join-Rel (@("Common", "Unclassified") + (Normalize-Components (Get-Tail $dirParts 1)))
    }

    return Get-CuratedDestinationDir $dirParts $fileName
}

function Get-MetaGuid([string]$metaPath) {
    if (-not (Test-Path -LiteralPath $metaPath)) {
        return ""
    }

    $text = Get-Content -Raw -LiteralPath $metaPath
    $match = [regex]::Match($text, '"guid"\s*:\s*"([^"]+)"')
    if ($match.Success) {
        return $match.Groups[1].Value
    }

    return ""
}

function Get-MetaType([string]$metaPath) {
    if (-not (Test-Path -LiteralPath $metaPath)) {
        return "Texture"
    }

    $text = Get-Content -Raw -LiteralPath $metaPath
    $match = [regex]::Match($text, '"type"\s*:\s*"([^"]+)"')
    if ($match.Success) {
        return $match.Groups[1].Value
    }

    return "Texture"
}

function New-UniqueDestination([string]$destDir, [string]$fileName, [hashtable]$usedDestinations, [string]$hash, [string]$sourceRel) {
    $stem = [System.IO.Path]::GetFileNameWithoutExtension($fileName)
    $extension = [System.IO.Path]::GetExtension($fileName)
    $candidate = Join-Rel @($destDir, $fileName)

    if (-not $usedDestinations.ContainsKey($candidate)) {
        $usedDestinations[$candidate] = $hash
        return $candidate
    }

    if ($usedDestinations[$candidate] -eq $hash) {
        return $candidate
    }

    $sourceDirName = [System.IO.Path]::GetFileName(([System.IO.Path]::GetDirectoryName((Normalize-Rel $sourceRel))))
    if ([string]::IsNullOrWhiteSpace($sourceDirName)) {
        $sourceDirName = "Variant"
    }

    $safeSuffix = ($sourceDirName -replace "[^A-Za-z0-9]+", "")
    if ([string]::IsNullOrWhiteSpace($safeSuffix)) {
        $safeSuffix = "Variant"
    }

    $index = 1
    do {
        $suffix = if ($index -eq 1) { $safeSuffix } else { "$safeSuffix$index" }
        $candidateName = "${stem}_${suffix}${extension}"
        $candidate = Join-Rel @($destDir, $candidateName)
        $index++
    } while ($usedDestinations.ContainsKey($candidate))

    $usedDestinations[$candidate] = $hash
    return $candidate
}

function To-SlashRel([string]$rel) {
    return (Normalize-Rel $rel) -replace "\\", "/"
}

function Escape-RegistryPath([string]$path) {
    return $path.Replace("\", "\\")
}

function Unescape-RegistryPath([string]$path) {
    return $path.Replace("\\", "\")
}

function ConvertTo-RegistryEntry([string]$fullPath, [string]$guid, [string]$modelType, [string]$relativePath, [string]$type) {
    $fullEscaped = Escape-RegistryPath $fullPath
    $relativeEscaped = Escape-RegistryPath $relativePath
    return "        {`r`n            `"fullPath`": `"$fullEscaped`",`r`n            `"guid`": `"$guid`",`r`n            `"modelType`": `"$modelType`",`r`n            `"relativePath`": `"$relativeEscaped`",`r`n            `"type`": `"$type`"`r`n        }"
}

function Update-TextReferences([hashtable]$oldPngToNewPng, [hashtable]$oldGuidToNewGuid) {
    $files = Get-ChildItem -LiteralPath (Join-Path $resourceRoot "UI") -File -Recurse -Include *.json
    $changedFiles = @()
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    $utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)

    foreach ($file in $files) {
        $original = [System.IO.File]::ReadAllText($file.FullName, $utf8Strict)
        $content = $original

        foreach ($oldRel in $oldPngToNewPng.Keys) {
            $oldSlash = To-SlashRel $oldRel
            $newSlash = To-SlashRel $oldPngToNewPng[$oldRel]
            $pattern = [regex]::Escape("Textures/UI/$oldSlash")
            $content = [regex]::Replace($content, $pattern, "Textures/UI/$newSlash", [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        }

        foreach ($oldGuid in $oldGuidToNewGuid.Keys) {
            $newGuid = $oldGuidToNewGuid[$oldGuid]
            if (-not [string]::IsNullOrWhiteSpace($oldGuid) -and $oldGuid -ne $newGuid) {
                $content = $content.Replace($oldGuid, $newGuid)
            }
        }

        if ($content -ne $original) {
            [System.IO.File]::WriteAllText($file.FullName, $content, $utf8NoBom)
            $changedFiles += $file.FullName.Substring($repoRoot.Length + 1)
        }
    }

    return $changedFiles
}

function Update-AssetRegistryFile([string]$path, [hashtable]$oldPngToNewPng, [hashtable]$oldGuidToNewGuid) {
    if (-not (Test-Path -LiteralPath $path)) {
        return [pscustomobject]@{ File = $path; Updated = 0; Removed = 0; Kept = 0 }
    }

    $text = Get-Content -Raw -LiteralPath $path
    $entryRegex = [regex]'(?ms)\s*\{\s*"fullPath"\s*:\s*"(?<full>[^"]*)"\s*,\s*"guid"\s*:\s*"(?<guid>[^"]*)"\s*,\s*"modelType"\s*:\s*"(?<model>[^"]*)"\s*,\s*"relativePath"\s*:\s*"(?<rel>[^"]*)"\s*,\s*"type"\s*:\s*"(?<type>[^"]*)"\s*\}\s*,?'
    $matches = $entryRegex.Matches($text)
    if ($matches.Count -eq 0) {
        throw "Could not parse registry entries in '$path'."
    }

    $entries = New-Object System.Collections.Generic.List[string]
    $seenUiRel = @{}
    $updated = 0
    $removed = 0
    $kept = 0

    foreach ($match in $matches) {
        $guid = $match.Groups["guid"].Value
        $modelType = $match.Groups["model"].Value
        $relativeRaw = $match.Groups["rel"].Value
        $type = $match.Groups["type"].Value
        $relative = Unescape-RegistryPath $relativeRaw

        if ($relative.StartsWith("Textures\UI\", [System.StringComparison]::OrdinalIgnoreCase)) {
            $uiRel = $relative.Substring("Textures\UI\".Length)
            if ($oldPngToNewPng.ContainsKey($uiRel)) {
                $mappedGuid = if ($oldGuidToNewGuid.ContainsKey($guid)) { $oldGuidToNewGuid[$guid] } else { $guid }
                if ($mappedGuid -ne $guid) {
                    $removed++
                    continue
                }

                $newUiRel = Normalize-Rel $oldPngToNewPng[$uiRel]
                $newRelative = "Textures\UI\$newUiRel"
                if ($seenUiRel.ContainsKey($newRelative)) {
                    $removed++
                    continue
                }

                $newFullPath = Join-Path $resourceRoot $newRelative
                $entries.Add((ConvertTo-RegistryEntry $newFullPath $guid $modelType $newRelative $type))
                $seenUiRel[$newRelative] = $true
                $updated++
                continue
            }

            $removed++
            continue
        }

        $entries.Add($match.Value.Trim().TrimEnd(","))
        $kept++
    }

    $newText = "{`r`n    `"assets`": [`r`n" + ($entries -join ",`r`n") + "`r`n    ]`r`n}"
    Set-Content -LiteralPath $path -Value $newText -NoNewline

    return [pscustomobject]@{
        File = $path.Substring($repoRoot.Length + 1)
        Updated = $updated
        Removed = $removed
        Kept = $kept
    }
}

function Test-TextureReferences() {
    $textureRoot = Join-Path $resourceRoot "Textures\UI"
    $pattern = '"(?:texturePath|maskTexturePath)"\s*:\s*"([^"]*Textures/UI/[^"]+)"'
    $missing = @()
    $refs = 0
    $files = Get-ChildItem -LiteralPath (Join-Path $resourceRoot "UI") -File -Recurse -Include *.json

    foreach ($file in $files) {
        $text = Get-Content -Raw -LiteralPath $file.FullName
        foreach ($match in [regex]::Matches($text, $pattern)) {
            $refs++
            $runtimePath = $match.Groups[1].Value.Replace("\", "/")
            $lowered = $runtimePath.ToLowerInvariant()
            $relative = $runtimePath
            foreach ($prefix in @("../bin/resources/", "bin/resources/", "resources/")) {
                if ($lowered.StartsWith($prefix)) {
                    $relative = $runtimePath.Substring($prefix.Length)
                    break
                }
            }

            $resolved = Join-Path $resourceRoot ($relative -replace "/", "\")
            if (-not (Test-Path -LiteralPath $resolved)) {
                $missing += [pscustomobject]@{
                    Source = $file.FullName.Substring($repoRoot.Length + 1)
                    TexturePath = $match.Groups[1].Value
                }
            }
        }
    }

    return [pscustomobject]@{
        ReferenceCount = $refs
        MissingCount = $missing.Count
        Missing = @($missing)
    }
}

function Get-RegistryUiSummary([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) {
        return [pscustomobject]@{ File = $path; UIEntries = 0; OriginEntries = 0; InvalidTopEntries = @() }
    }

    $text = Get-Content -Raw -LiteralPath $path
    $matches = [regex]::Matches($text, '"relativePath"\s*:\s*"Textures\\\\UI\\\\([^"\r\n]+)"')
    $invalid = @()
    $origin = 0
    foreach ($match in $matches) {
        $uiRel = Unescape-RegistryPath $match.Groups[1].Value
        $top = ($uiRel -split "\\")[0]
        if ($top -eq "origin") {
            $origin++
        }
        if ($allowedTopFolders -notcontains $top) {
            $invalid += $uiRel
        }
    }

    return [pscustomobject]@{
        File = $path.Substring($repoRoot.Length + 1)
        UIEntries = $matches.Count
        OriginEntries = $origin
        InvalidTopEntries = @($invalid)
    }
}

Assert-UnderRoot $uiRoot $repoRoot
Assert-UnderRoot $assetDataRoot $repoRoot

$pngFiles = @(Get-ChildItem -LiteralPath $uiRoot -File -Recurse -Filter *.png)
$records = @()

foreach ($file in $pngFiles) {
    $oldRel = Normalize-Rel $file.FullName.Substring($uiRoot.Length + 1)
    $metaPath = $file.FullName + ".meta"
    $jsonPath = [System.IO.Path]::ChangeExtension($file.FullName, ".json")
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash
    $destDir = Get-DestinationDir $oldRel $file.Name
    $guid = Get-MetaGuid $metaPath
    $metaType = Get-MetaType $metaPath

    $records += [pscustomobject]@{
        OldRel = $oldRel
        OldFull = $file.FullName
        FileName = $file.Name
        Hash = $hash
        IsOrigin = $oldRel.StartsWith("origin\", [System.StringComparison]::OrdinalIgnoreCase)
        Guid = $guid
        MetaType = $metaType
        MetaRel = if (Test-Path -LiteralPath $metaPath) { Normalize-Rel ($metaPath.Substring($uiRoot.Length + 1)) } else { "" }
        MetaFull = if (Test-Path -LiteralPath $metaPath) { $metaPath } else { "" }
        JsonRel = if (Test-Path -LiteralPath $jsonPath) { Normalize-Rel ($jsonPath.Substring($uiRoot.Length + 1)) } else { "" }
        JsonFull = if (Test-Path -LiteralPath $jsonPath) { $jsonPath } else { "" }
        DestDir = $destDir
        IsKeeper = $false
        PlannedNewRel = ""
        PlannedMetaRel = ""
        PlannedJsonRel = ""
        SelectedJsonRel = ""
        SelectedJsonFull = ""
        Reason = ""
    }
}

$usedDestinations = @{}
$keeperByHash = @{}
$oldPngToNewPng = @{}
$oldGuidToNewGuid = @{}
$operations = @()
$removedDuplicateGuids = @()

foreach ($group in ($records | Group-Object Hash)) {
    $keeper = @($group.Group | Sort-Object `
        @{ Expression = { if ($_.IsOrigin) { 1 } else { 0 } }; Ascending = $true }, `
        @{ Expression = { $_.OldRel }; Ascending = $true } | Select-Object -First 1)[0]

    $keeper.IsKeeper = $true
    $keeper.Reason = if ($keeper.IsOrigin) { "origin_unique_keeper" } else { "curated_keeper" }
    $keeperByHash[$group.Name] = $keeper

    $newRel = New-UniqueDestination $keeper.DestDir $keeper.FileName $usedDestinations $keeper.Hash $keeper.OldRel
    $keeper.PlannedNewRel = $newRel
    $keeper.PlannedMetaRel = "$newRel.meta"

    $selectedJson = @($group.Group | Where-Object { -not [string]::IsNullOrWhiteSpace($_.JsonRel) } | Sort-Object `
        @{ Expression = { if ($_.OldRel -eq $keeper.OldRel) { 0 } else { 1 } }; Ascending = $true }, `
        @{ Expression = { if ($_.IsOrigin) { 1 } else { 0 } }; Ascending = $true }, `
        @{ Expression = { $_.OldRel }; Ascending = $true } | Select-Object -First 1)
    if ($selectedJson.Count -gt 0) {
        $keeper.SelectedJsonRel = $selectedJson[0].JsonRel
        $keeper.SelectedJsonFull = $selectedJson[0].JsonFull
        $keeper.PlannedJsonRel = [System.IO.Path]::ChangeExtension($newRel, ".json")
    }

    foreach ($record in $group.Group) {
        $oldPngToNewPng[$record.OldRel] = $newRel

        if (-not [string]::IsNullOrWhiteSpace($record.Guid) -and -not [string]::IsNullOrWhiteSpace($keeper.Guid)) {
            $oldGuidToNewGuid[$record.Guid] = $keeper.Guid
            if ($record.Guid -ne $keeper.Guid) {
                $removedDuplicateGuids += $record.Guid
            }
        }

        if ($record.OldRel -eq $keeper.OldRel) {
            continue
        }

        $record.Reason = "duplicate_content_of:$($keeper.OldRel)"
    }
}

foreach ($record in $records) {
    if ($record.IsKeeper) {
        $operations += [pscustomobject]@{
            action = "move"
            source = $record.OldRel
            destination = $record.PlannedNewRel
            reason = $record.Reason
            keeper = $record.PlannedNewRel
            duplicateHash = $record.Hash
            guid = $record.Guid
        }
    }
    else {
        $keeper = $keeperByHash[$record.Hash]
        $operations += [pscustomobject]@{
            action = "dedupe"
            source = $record.OldRel
            destination = $keeper.PlannedNewRel
            reason = $record.Reason
            keeper = $keeper.OldRel
            duplicateHash = $record.Hash
            guid = $record.Guid
        }
    }
}

$dryRunManifest = [pscustomobject]@{
    generatedAt = (Get-Date).ToString("o")
    dryRunOnly = [bool]$DryRunOnly
    uiRoot = $uiRoot
    counts = [pscustomobject]@{
        pngFiles = $records.Count
        keepers = @($records | Where-Object IsKeeper).Count
        deduped = @($records | Where-Object { -not $_.IsKeeper }).Count
        originPngFiles = @($records | Where-Object IsOrigin).Count
        originPngKeepers = @($records | Where-Object { $_.IsOrigin -and $_.IsKeeper }).Count
    }
    operations = @($operations)
}

$dryRunManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $dryRunManifestPath

if ($DryRunOnly) {
    Write-Host "Dry-run manifest written: $dryRunManifestPath"
    Write-Host "PNG keepers: $(@($records | Where-Object IsKeeper).Count), deduped PNGs: $(@($records | Where-Object { -not $_.IsKeeper }).Count)"
    return
}

$stagingRoot = Join-Path $uiRoot ("__cleanup_staging_" + [guid]::NewGuid().ToString("N"))
Assert-UnderRoot $stagingRoot $uiRoot
New-Item -ItemType Directory -Path $stagingRoot | Out-Null

try {
    $stagedItems = @{}
    $moveIndex = 0
    foreach ($record in @($records | Where-Object IsKeeper)) {
        $slot = Join-Path $stagingRoot ("item_{0:D5}" -f $moveIndex)
        New-Item -ItemType Directory -Path $slot | Out-Null
        $moveIndex++

        $stagedPng = Join-Path $slot $record.FileName
        Move-Item -LiteralPath $record.OldFull -Destination $stagedPng
        $stagedItems[$record.OldRel] = $stagedPng

        if (-not [string]::IsNullOrWhiteSpace($record.MetaFull) -and (Test-Path -LiteralPath $record.MetaFull)) {
            $stagedMeta = Join-Path $slot ($record.FileName + ".meta")
            Move-Item -LiteralPath $record.MetaFull -Destination $stagedMeta
            $stagedItems[$record.MetaRel] = $stagedMeta
        }

        if (-not [string]::IsNullOrWhiteSpace($record.SelectedJsonFull) -and (Test-Path -LiteralPath $record.SelectedJsonFull)) {
            $stagedJson = Join-Path $slot ([System.IO.Path]::GetFileName($record.SelectedJsonFull))
            Move-Item -LiteralPath $record.SelectedJsonFull -Destination $stagedJson
            $stagedItems[$record.SelectedJsonRel] = $stagedJson
        }
    }

    foreach ($record in @($records | Where-Object { -not $_.IsKeeper })) {
        foreach ($path in @($record.OldFull, $record.MetaFull, $record.JsonFull)) {
            if ([string]::IsNullOrWhiteSpace($path)) {
                continue
            }
            if ($stagedItems.Values -contains $path) {
                continue
            }
            if (Test-Path -LiteralPath $path) {
                Assert-UnderRoot $path $uiRoot
                Remove-Item -LiteralPath $path -Force
            }
        }
    }

    foreach ($record in @($records | Where-Object IsKeeper)) {
        $destinationPng = Join-Path $uiRoot $record.PlannedNewRel
        Assert-UnderRoot $destinationPng $uiRoot
        New-Item -ItemType Directory -Path ([System.IO.Path]::GetDirectoryName($destinationPng)) -Force | Out-Null
        Move-Item -LiteralPath $stagedItems[$record.OldRel] -Destination $destinationPng

        if ($stagedItems.ContainsKey($record.MetaRel)) {
            $destinationMeta = Join-Path $uiRoot $record.PlannedMetaRel
            Assert-UnderRoot $destinationMeta $uiRoot
            Move-Item -LiteralPath $stagedItems[$record.MetaRel] -Destination $destinationMeta
        }

        if (-not [string]::IsNullOrWhiteSpace($record.PlannedJsonRel) -and $stagedItems.ContainsKey($record.SelectedJsonRel)) {
            $destinationJson = Join-Path $uiRoot $record.PlannedJsonRel
            Assert-UnderRoot $destinationJson $uiRoot
            Move-Item -LiteralPath $stagedItems[$record.SelectedJsonRel] -Destination $destinationJson
        }
    }

    foreach ($dir in @(Get-ChildItem -LiteralPath $stagingRoot -Directory -Recurse | Sort-Object FullName -Descending)) {
        if (@(Get-ChildItem -LiteralPath $dir.FullName -Force).Count -eq 0) {
            Assert-UnderRoot $dir.FullName $uiRoot
            Remove-Item -LiteralPath $dir.FullName -Force
        }
    }

    $leftover = @(Get-ChildItem -LiteralPath $stagingRoot -Force -Recurse)
    if ($leftover.Count -gt 0) {
        throw "Staging directory contains leftover files after moves: $stagingRoot"
    }
    Remove-Item -LiteralPath $stagingRoot -Force
}
catch {
    Write-Error $_
    throw
}

$emptyDirs = @(Get-ChildItem -LiteralPath $uiRoot -Directory -Recurse | Sort-Object FullName -Descending)
foreach ($dir in $emptyDirs) {
    if ($dir.FullName -eq $stagingRoot) {
        continue
    }
    if (@(Get-ChildItem -LiteralPath $dir.FullName -Force).Count -eq 0) {
        Assert-UnderRoot $dir.FullName $uiRoot
        Remove-Item -LiteralPath $dir.FullName -Force
    }
}

$changedUiFiles = Update-TextReferences $oldPngToNewPng $oldGuidToNewGuid
$registryUpdates = @(
    Update-AssetRegistryFile (Join-Path $assetDataRoot "AssetRegistry.json") $oldPngToNewPng $oldGuidToNewGuid
    Update-AssetRegistryFile (Join-Path $assetDataRoot "AssetRegistry.cache.json") $oldPngToNewPng $oldGuidToNewGuid
)

$textureRefs = Test-TextureReferences
$pngsAfter = @(Get-ChildItem -LiteralPath $uiRoot -File -Recurse -Filter *.png)
$missingMeta = @()
foreach ($png in $pngsAfter) {
    if (-not (Test-Path -LiteralPath ($png.FullName + ".meta"))) {
        $missingMeta += $png.FullName.Substring($uiRoot.Length + 1)
    }
}

$duplicatePngGroups = @($pngsAfter | ForEach-Object {
    [pscustomobject]@{
        RelativePath = Normalize-Rel $_.FullName.Substring($uiRoot.Length + 1)
        Hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
    }
} | Group-Object Hash | Where-Object { $_.Count -gt 1 })

$originTextRefs = 0
foreach ($path in @((Join-Path $resourceRoot "UI"), (Join-Path $assetDataRoot "AssetRegistry.json"), (Join-Path $assetDataRoot "AssetRegistry.cache.json"))) {
    if (Test-Path -LiteralPath $path -PathType Container) {
        $originTextRefs += @(Get-ChildItem -LiteralPath $path -File -Recurse -Include *.json | Select-String -Pattern "Textures/UI/origin|Textures\\\\UI\\\\origin").Count
    }
    elseif (Test-Path -LiteralPath $path -PathType Leaf) {
        $originTextRefs += @(Select-String -LiteralPath $path -Pattern "Textures/UI/origin|Textures\\\\UI\\\\origin").Count
    }
}

$removedGuidRefs = 0
foreach ($guid in ($removedDuplicateGuids | Sort-Object -Unique)) {
    if ([string]::IsNullOrWhiteSpace($guid)) {
        continue
    }
    $removedGuidRefs += @(Get-ChildItem -LiteralPath (Join-Path $resourceRoot "UI") -File -Recurse -Include *.json | Select-String -SimpleMatch $guid).Count
}

$topFolders = @(Get-ChildItem -LiteralPath $uiRoot -Directory | Select-Object -ExpandProperty Name | Sort-Object)
$invalidTopFolders = @($topFolders | Where-Object { $allowedTopFolders -notcontains $_ })
$registrySummaries = @(
    Get-RegistryUiSummary (Join-Path $assetDataRoot "AssetRegistry.json")
    Get-RegistryUiSummary (Join-Path $assetDataRoot "AssetRegistry.cache.json")
)

$finalManifest = [pscustomobject]@{
    generatedAt = (Get-Date).ToString("o")
    uiRoot = $uiRoot
    counts = [pscustomobject]@{
        pngBefore = $records.Count
        pngAfter = $pngsAfter.Count
        keepers = @($records | Where-Object IsKeeper).Count
        deduped = @($records | Where-Object { -not $_.IsKeeper }).Count
        changedUiReferenceFiles = $changedUiFiles.Count
    }
    topFolders = $topFolders
    changedUiReferenceFiles = @($changedUiFiles)
    registryUpdates = @($registryUpdates)
    validation = [pscustomobject]@{
        originDirectoryExists = Test-Path -LiteralPath (Join-Path $uiRoot "origin")
        originReferenceCount = $originTextRefs
        missingTextureReferenceCount = $textureRefs.MissingCount
        missingTextureReferences = @($textureRefs.Missing)
        missingMetaCount = $missingMeta.Count
        missingMeta = @($missingMeta)
        removedDuplicateGuidReferenceCount = $removedGuidRefs
        duplicatePngGroups = $duplicatePngGroups.Count
        invalidTopFolders = @($invalidTopFolders)
        registrySummaries = @($registrySummaries)
    }
    operations = @($operations)
}

$finalManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath

Write-Host "Dry-run manifest written: $dryRunManifestPath"
Write-Host "Final manifest written: $manifestPath"
Write-Host "PNG before: $($records.Count)"
Write-Host "PNG after: $($pngsAfter.Count)"
Write-Host "Deduped PNGs: $(@($records | Where-Object { -not $_.IsKeeper }).Count)"
Write-Host "Changed UI reference files: $($changedUiFiles.Count)"
Write-Host "Origin directory exists: $(Test-Path -LiteralPath (Join-Path $uiRoot "origin"))"
Write-Host "Origin references: $originTextRefs"
Write-Host "Missing texture references: $($textureRefs.MissingCount)"
Write-Host "Missing PNG metas: $($missingMeta.Count)"
Write-Host "Duplicate PNG groups: $($duplicatePngGroups.Count)"
Write-Host "Invalid top folders: $($invalidTopFolders -join ', ')"
