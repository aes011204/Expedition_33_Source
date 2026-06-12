# FacialClip Builder

Builds runtime facial preview clips from exported UE/FModel facial animation
data. The normal output used by the game is `.facialclipbin`.

The tool combines:

- AnimSequence JSON `CompressedCurveData.FloatCurves` into morph curve tracks
- ActorX PSA `BONENAMES` + `ANIMKEYS` into facial bone tracks
- `.model.meta` bone matching for the current project head
- `.morph` target matching for the current project head

## Normal Workflow

Use the all-hero batch for the current GameMenu facial preview.

```bat
cd /d D:\GitDesktop\Expedition_158\BatchTool\Facial
BuildFacialClip_AllHeroes.bat
```

The batch reads the shared source animation:

```text
Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu.json
Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu.psa
```

Then it binds the source animation to each project head:

```text
Client/Bin/Resources/Models/Heros/Gustave/GustaveHead/GustaveHead.model.meta
Client/Bin/Resources/Models/Heros/Gustave/GustaveHead/GustaveHead.morph
Client/Bin/Resources/Models/Heros/Lune/LuneHead/LuneHead.model.meta
Client/Bin/Resources/Models/Heros/Lune/LuneHead/LuneHead.morph
Client/Bin/Resources/Models/Heros/Maelle/MaelleHead/MaelleHead.model.meta
Client/Bin/Resources/Models/Heros/Maelle/MaelleHead/MaelleHead.morph
```

## Outputs

The all-hero batch creates these files under
`Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements`.

```text
A_MH_RandomMovement_GameMenu_Gustave.facialclip.json
A_MH_RandomMovement_GameMenu_Gustave.facialclip.report.json
A_MH_RandomMovement_GameMenu_Gustave.facialclipbin
A_MH_RandomMovement_GameMenu_Lune.facialclip.json
A_MH_RandomMovement_GameMenu_Lune.facialclip.report.json
A_MH_RandomMovement_GameMenu_Lune.facialclipbin
A_MH_RandomMovement_GameMenu_Maelle.facialclip.json
A_MH_RandomMovement_GameMenu_Maelle.facialclip.report.json
A_MH_RandomMovement_GameMenu_Maelle.facialclipbin
```

Runtime code loads only `.facialclipbin`. The `.facialclip.json` and
`.facialclip.report.json` files are for inspection and debugging.

## What To Check After Running

First inspect the report files.

```powershell
Get-Content -Encoding UTF8 ..\..\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Gustave.facialclip.report.json
Get-Content -Encoding UTF8 ..\..\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Lune.facialclip.report.json
Get-Content -Encoding UTF8 ..\..\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Maelle.facialclip.report.json
```

Current expected report level:

```text
duration: 17.633333s
morph:    182 / 315
bone:     779 / 779
```

If the matched morph or bone count drops sharply, check the hero
`.model.meta` / `.morph` files before testing runtime.

## Runtime Connection

No model-editor step is needed after the batch succeeds. `PW_Gustave`,
`PW_Lune`, and `PW_Maelle` each return their hero-specific `.facialclipbin`
from `Get_GameMenuFacialClipPath()`. `Player_WorldBase` lazy-loads that file
when the Character Status Panel or Character Skill Panel preview updates.

## When To Rebuild Clips

Re-run `BuildFacialClip_AllHeroes.bat` when:

- The source `A_MH_RandomMovement_GameMenu.json` or `.psa` changes.
- A hero head `.model.meta` or `.morph` file is regenerated.
- A report shows bad matching and the binding files were fixed.

You do not need to re-run the batch when only runtime tuning constants in
`FacialAnimationModifier.cpp` change.

## Single-Hero / Custom Run

Use this only for debugging or testing a new path. For the actual project
GameMenu preview, prefer `BuildFacialClip_AllHeroes.bat`.

```bat
py -3 BuildFacialClip.py ^
  --json "..\..\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu.json" ^
  --psa "..\..\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu.psa" ^
  --model-meta "..\..\Client\Bin\Resources\Models\Heros\Gustave\GustaveHead\GustaveHead.model.meta" ^
  --morph "..\..\Client\Bin\Resources\Models\Heros\Gustave\GustaveHead\GustaveHead.morph" ^
  --out "..\..\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Gustave.facialclip.json" ^
  --bin-out "..\..\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Gustave.facialclipbin" ^
  --compact
```

## Troubleshooting

If `py -3` is not recognized, install or repair the Python launcher for
Windows, or run the script with the Python executable available on the machine.

If JSON or PSA input is missing, copy the exported files into
`Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements` first.

If runtime does not change after rebuilding clips, confirm that the hero path in
`Get_GameMenuFacialClipPath()` points to the hero-specific `.facialclipbin` and
that the game was restarted or the asset cache was refreshed.
