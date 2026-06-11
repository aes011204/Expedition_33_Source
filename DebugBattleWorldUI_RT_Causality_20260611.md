# Debug BattleWorldUI Render-To-Texture Review Prompt

Date: 2026-06-11  
Project: Expedition_158  
Area: `Client` battle UI, runtime UI capture, world-space UI presentation

## Paste-Ready Prompt For GPT/Gemini

I am working on a DirectX 11 C++ game UI system. Please review the following architecture and problem description, then suggest the most likely cause and the smallest diagnostic steps before making another fix.

We have a normal 2D battle UI authored in JSON and rendered by our runtime UI system. I am experimenting with converting part of the battle selection UI into a world-space card by capturing the existing 2D UI into render targets and then rendering those render targets on a world-space quad near the active player.

The current problem is:

- Skill and Item world cards are visible, although Skill is very small and Item still needs tuning.
- Item main list correctly disappears when entering the item-use tooltip/targeting state.
- CommandHUD world card is not visible at all. It may exist somewhere, but visually we cannot find even tiny text.
- The original 2D selection panel is now hidden after the world card is submitted each frame.

Please analyze whether this is more likely to be an empty capture RT, render group filtering issue, UI open/alpha issue, world placement/scale issue, or depth/foreground/background split issue.

## Runtime UI Architecture

The project has a runtime UI system built around UI objects loaded from JSON:

- `UI_Base`
- `UI_Image`
- `UI_Text`
- `UI_Panel`
- `UI_RuntimePanelBase`

Runtime UI documents are loaded through `UI_RuntimeLoader`.

Each runtime battle UI panel owns or references a root `UI_Base`. UI visibility is controlled through `Open()` / `Close()`. Rendering checks effective visibility through parent hierarchy, so if a root is closed, its children should not render even if the child itself is open.

Important behavior:

- `UI_Base::Open()` sets `_isOpen = true`.
- `UI_Base::Close()` sets `_isOpen = false`.
- `UI_Base::Is_EffectivelyOpen()` checks itself and parent chain.

The selection panels currently involved are:

- `UI_BattleCommandHUD`
- `UI_BattleSkillPanel`
- `UI_BattleItemPanel`

They are updated from `GameplayUI_Battle::Update()`.

## Battle State Routing

`GameplayUI_Battle` chooses which panel to capture based on `BattleTurnState`:

- `ChooseAction` / `AttackTargeting` -> `UI_BattleCommandHUD`
- `ChooseSkill` / `SkillTargeting` -> `UI_BattleSkillPanel`
- `ChooseItem` / `ItemTargeting` -> `UI_BattleItemPanel`

The resolver is private to `GameplayUI_Battle`:

```cpp
UI_RuntimePanelBase* Resolve_DebugBattleSelectionPanel() const;
```

There is also a per-state placement resolver:

```cpp
DebugBattleWorldUIPlacement Resolve_DebugBattleWorldUIPlacement() const;
```

Initial placement values are intentionally identical:

```cpp
Vec2 worldSize{ 1.75f, 1.0f };
Vec3 worldOffset{ 0.65f, 0.9f, 0.f };
Vec3 worldRotation{ 0.f, 0.f, 0.f };
```

Command, Skill, and Item have separate slots so they can be tuned independently later.

## Render-To-Texture Capture Setup

The capture system creates two render targets:

```cpp
RT_DebugBattleCommandCapture_Background
RT_DebugBattleCommandCapture_Foreground
```

Both are currently:

```cpp
2048 x 1024
DXGI_FORMAT_R8G8B8A8_UNORM
clear color = Color(0.f, 0.f, 0.f, 0.f)
```

There are two corresponding MRT tags:

```cpp
MRT_DebugBattleCommandCapture_Background
MRT_DebugBattleCommandCapture_Foreground
```

The capture code renders the selected runtime panel twice:

```cpp
capturePanel->Render_DebugCapture(UI_RenderGroup::Background);
capturePanel->Render_DebugCapture(UI_RenderGroup::Foreground);
```

`Render_DebugCapture()` filters UI entries by `UI_RenderGroup`.

The intended layer model is:

- Background RT contains large brush/deco/background images.
- Foreground RT contains cards, key icons, readable text, and anything that must not be hidden behind the player.

## World-Space Presentation

The world-space card object is `BattleWorldUI`.

There are two `BattleWorldUI` objects:

```cpp
_debugBattleWorldUIBackground
_debugBattleWorldUIForeground
```

The background card uses depth:

```cpp
useDepthRead = true;
```

The foreground card ignores depth:

```cpp
useDepthRead = false;
```

This is intended to make the brush/background feel like it is in the scene while keeping actual buttons/text readable on top of the player.

Each frame, after capture, the code submits both cards:

```cpp
worldPos += placement.worldOffset;

_debugBattleWorldUIBackground->Set_WorldPosition(worldPos);
_debugBattleWorldUIBackground->Set_WorldSize(placement.worldSize);
_debugBattleWorldUIBackground->Set_WorldRotation(placement.worldRotation);
_debugBattleWorldUIBackground->Submit_Render();

_debugBattleWorldUIForeground->Set_WorldPosition(worldPos);
_debugBattleWorldUIForeground->Set_WorldSize(placement.worldSize);
_debugBattleWorldUIForeground->Set_WorldRotation(placement.worldRotation);
_debugBattleWorldUIForeground->Submit_Render();
```

## Frame Order

The relevant current order in `GameplayUI_Battle::Update()` is:

```cpp
Update_BattleItemPanel(timeDelta);
Update_BattleSkillPanel(timeDelta);
Capture_DebugBattleCommandHUD();
Update_DebugBattleWorldUI(timeDelta);
Suppress_DebugBattleScreenHUD();
```

This order is intentional:

1. The active selection panel is opened/updated normally.
2. If capture is dirty, it captures the selected panel into RTs.
3. It submits the world-space cards.
4. It hides the original 2D selection panel root so the world card is the visible presentation.

Because the original 2D panel is suppressed every frame, the capture function reopens the root before capturing if needed:

```cpp
if (!root->Is_Open() && kDebugBattleCommandHideScreenHUD)
    root->Open();
```

Then after capture and world submission, suppression closes the original root again:

```cpp
Suppress_DebugBattleScreenHUD();
```

## Dirty Capture Policy

The capture is not supposed to update every frame.

It skips capture when all of these match the last capture:

- current `BattleTurnState`
- selected capture panel pointer
- panel runtime/editor revision

The revision is exposed through:

```cpp
Get_EditorRuntimeTuningRevision()
```

It changes when editor tuning changes, or when certain panel internal view changes occur.

The Item panel was updated to mark dirty when:

- Main item list opens.
- Item tooltip opens.
- Tooltip item type changes.
- Tooltip returns to main list.

This fixed the issue where old Item list text remained during item-use/targeting.

## Confirmed Results

### Confirmed Fixed

The old large black rectangle was caused by RT preview:

```cpp
kDebugBattleCommandCaptureShowPreview = true;
```

It is now:

```cpp
kDebugBattleCommandCaptureShowPreview = false;
```

The black rectangle disappeared.

### Confirmed Working

Item path:

- Item world card is visible.
- Item main list disappears when entering item-use tooltip/targeting.
- This suggests the capture path, world shader, and Item dirty policy are at least partly working.

Skill path:

- Skill world card is visible but extremely small near the character's right side.
- This suggests the world-card path is alive but placement/scale needs tuning.

### Remaining Broken

CommandHUD path:

- Command world card is not visibly found at all.
- It does not even show tiny text like Skill.
- The original 2D Command panel is now suppressed, so if the world card is empty, nothing appears.

## Important Current Screenshots

Observed at runtime:

- Skill panel: tiny text can be seen near the character's right side.
- Item panel: visible and confirms application.
- CommandHUD: no visible world card.

Local screenshot paths:

```text
C:/DATA/Videos/Desktop/Desktop Screenshot 2026.06.11 - 13.31.35.11.png
C:/DATA/Videos/Desktop/Desktop Screenshot 2026.06.11 - 13.31.46.66.png
```

## Main Hypotheses

### Hypothesis 1 - CommandHUD capture RT is empty or transparent

The selected Command panel may be open at the panel level, but its actual command option children may be closed, alpha-zero, animation-hidden, or not effectively open at capture time.

Why this seems plausible:

- Skill and Item show something.
- Command shows nothing.
- If Command RT is empty, world presentation would still submit but draw transparent pixels.

Minimal diagnostic:

- Add debug-only render counts in `UI_RuntimePanelBase::Render_DebugCapture()`.
- Log how many `UI_Image` and `UI_Text` objects were rendered for:
  - panel type
  - Background group
  - Foreground group
- If Command counts are zero, the issue is capture/open/filter/alpha, not world placement.

### Hypothesis 2 - CommandHUD uses a render group not captured by the current filter

The capture only renders:

```cpp
UI_RenderGroup::Background
UI_RenderGroup::Foreground
```

If Command cards/text are authored as a different group in JSON, they are skipped.

Minimal diagnostic:

- Inspect the CommandHUD JSON.
- Compare its `renderGroup` values to Skill and Item JSON entries that are visible.

### Hypothesis 3 - CommandHUD is rendered but too small or off-screen/in a bad world position

Skill is already visible but very small. Command may be similarly small or placed in a bad spot.

Why this is less certain:

- User could find tiny Skill text, but cannot find Command text at all.

Minimal diagnostic:

- Temporarily use MainEditor placement controls for Command only:
  - Increase world size strongly, for example `4.0 x 2.0`.
  - Move offset to a clearly visible area.
  - Try rotation reset.
- If Command appears, this is placement/scale.
- If Command still does not appear, suspect empty RT/filter/alpha.

### Hypothesis 4 - CommandHUD is captured into Background and hidden by depth

Background card uses depth read. If Command content is mostly in the Background RT, it may be hidden behind the player/world.

Minimal diagnostic:

- Check Command JSON render groups.
- Temporarily force Command capture into Foreground only, or temporarily disable depth on the Background card for Command diagnosis.

### Hypothesis 5 - CommandHUD root/open semantics differ from Skill/Item

Skill and Item explicitly manage main-panel and tooltip roots.

CommandHUD may have different internal child panels, animation alpha, or root behavior.

Minimal diagnostic:

- Inspect `UI_BattleCommandHUD.cpp/.h`.
- Confirm:
  - Which root is returned by `Get_RootUI()`.
  - Whether `Open()` opens the same root and children.
  - Whether option children are effectively open during capture.
  - Whether alpha multiplier is zero during capture.

## Questions For Review

1. Given that Skill and Item world cards are visible but CommandHUD is not visible at all, which hypothesis is most likely?
2. What is the smallest instrumentation to prove whether Command RT contains pixels?
3. Is the frame order sound?

```cpp
Capture_DebugBattleCommandHUD();
Update_DebugBattleWorldUI(timeDelta);
Suppress_DebugBattleScreenHUD();
```

4. Is reopening the root only before dirty capture safe when the screen-space panel is suppressed every frame?
5. Should suppression close only the root, or should it use a separate runtime visibility flag so capture/open state and screen presentation state are decoupled?
6. Could CommandHUD be hidden by an intro animation or alpha state at the moment of dirty capture?
7. Should the next step be:
   - RT pixel/count logging,
   - Command JSON render group inspection,
   - placement tuning,
   - or depth isolation?

## My Current Preferred Next Step

Before tuning placement, I think we should prove whether the Command RT has any rendered entries/pixels.

The smallest next diagnostic seems to be:

1. Add temporary debug-only render counts in `Render_DebugCapture()`.
2. Inspect CommandHUD JSON `renderGroup` values.
3. Only if Command draw counts are nonzero, test placement/depth.

Please review this reasoning and suggest the most reliable next step.
