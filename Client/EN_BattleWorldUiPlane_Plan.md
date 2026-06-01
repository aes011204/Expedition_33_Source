# BattleWorldUiPlane Detailed Plan

## External Review Prompt For GPT/Gemini

Use the following prompt when asking GPT or Gemini to review this revised plan:

```text
I am working on a fan-made study / clone-style project inspired by the battle UI direction of Clair Obscur: Expedition 33.

The goal is not to convert the entire existing 2D UI system into 3D. Instead, I want to selectively convert only some battle UI images that need spatial presence into world-space 2D planes. The first target is the background card of the battle Command HUD. Buttons, text, AP display, command highlighting, and input handling should remain in the existing 2D UI system.

You cannot read my local project, so I included the relevant project context, current update/rendering flow, proposed class design, fallback policy, and implementation order below.

This revised plan also incorporates prior feedback about:
- post-camera synchronization to prevent 2D/3D UI desync,
- verifying actual DSV binding during Render_Blend before trusting DepthRead,
- avoiding per-frame visibility reset from Open(),
- strengthening fallback conditions,
- postponing Blend sorting until multiple transparent world UI planes exist.

Please review whether this updated plan is technically sound. I especially want feedback on hidden risks, update order, render state assumptions, and whether the v1 scope is still safe.

Please answer in Korean.
```

---

## 0. Purpose

This document describes a `2.5D battle UI` experiment for the `Expedition_158` project, which references the battle UI presentation style of `Clair Obscur: Expedition 33`.

The goal is not to make the entire 2D UI system 3D. The first goal is to move only the battle `Command HUD` background card image into a `World-Space 2D Plane`.

The following should remain in the existing 2D UI system:

- command buttons,
- command text,
- AP number and AP segments,
- command highlight state,
- input handling,
- HUD open/close state.

The new `BattleWorldUiPlane` is only a visual world-space background card.

---

## 1. Current Project Context

Project root:

```text
C:\DATA\Documents\GitHub\Expedition_158
```

Relevant files:

```text
Client/Public/UI_BattleCommandHUD.h
Client/Private/UI_BattleCommandHUD.cpp
Client/Private/GameplayUI_Battle.cpp
Client/Bin/Resources/UI/UI_Json/BattleSystem/UI_BattleSystem_HUD.json
Client/Bin/Resources/Data/json/DT_Shader.json
Client/Bin/Resources/Data/json/DT_StaticLevel.json
Client/Bin/ShaderFiles/Shader_UI.hlsl
Client/Default/Client.vcxproj
Client/Default/Client.vcxproj.filters

Engine/Public/Engine_Enum.h
Engine/Public/GameObject.h
Engine/Public/TransformCom.h
Engine/Public/VIBuffer_Rect.h
Engine/Private/Renderer.cpp
Engine/Private/GameInstance.cpp
Engine/Private/Target_Manager.cpp
```

`UI_BattleCommandHUD` inherits from `UI_RuntimePanelBase` and loads its 2D UI from JSON through `UI_RuntimeLoader`.

Currently, `UI_BattleCommandHUD::Update()` projects `_targetWorldPos` to screen space and moves the 2D root `Panel_BattleCommand` every frame.

`GameplayUI_Battle::Update_BattleCommandHUD()` currently runs every frame during `BattleTurnState::ChooseAction` and calls:

```text
Open
Set_TargetWorldPosition
Set_CurrentHeroId
Set_CurrentPlayerAP
Refresh_ForState
Update
```

The current world anchor for the Command HUD is:

```cpp
outWorldPos = actor.actor->Get_Transform()->Get_WorldPosition();
outWorldPos.y += 0.8f;
```

This should be reused as the first anchor for `BattleWorldUiPlane`.

---

## 2. Current Command HUD JSON

Relevant JSON:

```text
Client/Bin/Resources/UI/UI_Json/BattleSystem/UI_BattleSystem_HUD.json
```

Relevant nodes:

```text
Panel_BattleCommand
Panel_BattleCommand_BG
Image_BattleCommand_BG
```

Existing background texture path:

```text
../Bin/Resources/Textures/UI/Battle/HUD/T_HUD_BattleSystem_BackGround.png
```

Recommended runtime path:

```text
Textures/UI/Battle/HUD/T_HUD_BattleSystem_BackGround.png
```

The UI JSON schema should not be changed in v1. Existing `Panel_BattleCommand_BG` remains as fallback.

---

## 3. Core Render Decisions

### 3.1 RenderGroup

`BattleWorldUiPlane` should submit itself to `RenderGroup::Blend`.

Reason:

- It is a world-space `GameObject`, not screen-space UI.
- It must use camera View/Projection.
- It needs PNG alpha blending.
- It should feel like a card placed inside the battle scene.

Do not use `RenderGroup::UI` for this feature.

### 3.2 Shader Passes

`Shader_BattleWorldUiPlane` should use the `VTXTEX` input layout.

Default pass:

```text
DSS_DepthRead + BS_AlphaBlend
```

Debug/comparison pass:

```text
DSS_None + BS_AlphaBlend
```

Confirmed current render-state assumptions:

- `DSS_DepthRead` has `DepthWriteMask = zero`, so it does not write depth.
- `BS_AlphaBlend` uses `SrcAlpha / InvSrcAlpha`, which matches straight-alpha PNGs.
- `RS_CullNone` should be used for the first pass to reduce culling/axis-direction issues.

### 3.3 DSV Verification Rule

Current `Renderer::Render_Blend()` calls:

```cpp
GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV)
```

However, current `Target_Manager::Begin_MRT()` appears to fallback to `_originalDSV`:

```cpp
dsv = dsv ? dsv : _originalDSV.Get();
```

So even though the call says `NotUseDSV`, the original DSV may still be bound during Blend rendering. This should be treated as a current implementation detail, not a guaranteed contract.

Decision:

- v1 default is `UseDepthRead = true`.
- The first runtime test must verify whether an actual DSV is bound during `Render_Blend`.
- If no DSV is bound, set `UseDepthRead = false` for the v1 Command HUD plane and postpone Blend-depth cleanup to v2.

### 3.4 Blend Sorting

Current `Render_Blend()` renders objects in registration order. There is no camera-distance back-to-front sorting.

Decision:

- v1 uses only one Command HUD background plane, so Blend sorting is postponed.
- Before adding multiple transparent world UI planes in v2, add or verify camera-distance back-to-front sorting for Blend objects.

---

## 4. Frame Synchronization Rule

The biggest visual risk is desync between the 2D buttons/text and the 3D background plane.

In the current engine update order, `GameplayUI_Battle::Update()` runs inside `ClientInstance::Update_Client()` before the active battle camera finishes updating its final View/Projection matrices.

If final 2D projection happens in `UI_BattleCommandHUD::Update()` before the camera update, the 2D UI may use the previous frame camera. Once the 3D plane is added, the background and text/buttons may visibly slip apart.

Decision:

```text
Final 2D HUD projection and 3D plane transform update must happen after the camera update, using the same final View/Proj matrices in the same frame.
```

Implementation direction:

1. `GameplayUI_Battle::Update_BattleCommandHUD()` should update state only.
   - open/close intent,
   - target world position,
   - current hero id,
   - AP values,
   - command highlight state.
   - It should not finalize 2D projection or submit the world plane.

2. Add a post-camera presentation sync phase.
   - Example: `GameplayUI::Sync_PresentationAfterCamera(float TimeDelta)`
   - Example: `GameplayUI_Battle::Sync_PresentationAfterCamera(float TimeDelta)`
   - Example: `UI_BattleCommandHUD::Sync_PresentationAfterCamera(float TimeDelta)`

3. Call this presentation sync after camera update and before `UI_Manager::Update()`.

Recommended order:

```text
_cameraManager->Update(...)
CLIENT presentation sync
_uiManager->Update(...)
Draw
```

This makes the 2D text/buttons and the 3D background plane sample the same final camera state.

---

## 5. New Runtime Object Design

New files:

```text
Client/Public/BattleWorldUiPlane.h
Client/Private/BattleWorldUiPlane.cpp
```

Class:

```cpp
class BattleWorldUiPlane final : public GameObject
```

Components:

```text
TransformCom
VIBuffer_Rect
Shader_BattleWorldUiPlane
```

### 5.1 Billboard Mode

```cpp
enum class EBattleWorldUiBillboardMode
{
    FaceCamera,
    FixedYaw,
    FixedRotation,
};
```

v1 default is `FaceCamera`.

### 5.2 Desc

```cpp
struct BattleWorldUiPlaneDesc : public GameObject::GAMEOBJECT_DESC
{
    wstring TexturePath{};
    Vec3 WorldPosition{ 0.f, 0.f, 0.f };
    Vec2 WorldSize{ 1.f, 1.f };
    Vec4 TintColor{ 1.f, 1.f, 1.f, 1.f };
    EBattleWorldUiBillboardMode BillboardMode{ EBattleWorldUiBillboardMode::FaceCamera };
    bool UseDepthRead{ true };
    bool Visible{ true };
};
```

### 5.3 Main Methods

```cpp
HRESULT Initialize(const BattleWorldUiPlaneDesc& Desc);
void Set_WorldPosition(const Vec3& Position);
void Set_WorldSize(const Vec2& Size);
void Set_TintColor(const Vec4& Color);
void Set_BillboardMode(EBattleWorldUiBillboardMode Mode);
void Set_Visible(bool Visible);
bool Is_RenderReady() const;
void Submit_Render();
HRESULT Render() override;
```

`Submit_Render()` should enqueue the object into `RenderGroup::Blend` only when `_renderReady && _visible`.

---

## 6. UI_BattleCommandHUD Integration

New members:

```cpp
Shared<UI_Base> _screenBackgroundPanel{};
Shared<BattleWorldUiPlane> _worldBackgroundPlane{};
bool _useWorldBackgroundPlane = false;
```

New methods:

```cpp
HRESULT Ready_WorldBackgroundPlane();
void Sync_PresentationAfterCamera(float TimeDelta);
void Set_ScreenBackgroundVisible(bool Visible);
Vec3 Resolve_WorldBackgroundPosition() const;
```

### 6.1 Stronger Fallback Condition

Hide the existing 2D background only when all conditions are true:

```cpp
_useWorldBackgroundPlane =
    _worldBackgroundPlane != nullptr &&
    _worldBackgroundPlane->Is_RenderReady() &&
    _screenBackgroundPanel != nullptr;
```

Reason:

- If the plane succeeds but the 2D background panel cannot be found, the old 2D background may remain visible together with the 3D plane.
- v1 is intended to replace the old background, so panel binding success is required.

### 6.2 Safe Open Behavior

`GameplayUI_Battle::Update_BattleCommandHUD()` currently calls `Open()` every frame during `ChooseAction`.

Therefore, `Open()` must not reset the plane visibility to false every frame.

Recommended implementation:

```cpp
void UI_BattleCommandHUD::Open()
{
    if (_isOpen)
        return;

    UI_RuntimePanelBase::Open();

    if (_useWorldBackgroundPlane)
        Set_ScreenBackgroundVisible(false);

    if (_worldBackgroundPlane)
        _worldBackgroundPlane->Set_Visible(false);
}
```

Alternative:

- Do not control plane visibility in `Open()`.
- Control it only in `Close()` and `Sync_PresentationAfterCamera()`.

### 6.3 Close Behavior

`Close()` must hide the world plane and restore the 2D fallback background.

```cpp
if (_worldBackgroundPlane)
    _worldBackgroundPlane->Set_Visible(false);

Set_ScreenBackgroundVisible(true);
```

---

## 7. Positioning

Base position:

```cpp
_targetWorldPos = actor position + Vec3(0.f, 0.8f, 0.f);
```

Initial values:

```cpp
WorldSize = Vec2(1.55f, 1.80f);
TintColor = Vec4(1.f, 1.f, 1.f, 0.92f);
BillboardMode = FaceCamera;
UseDepthRead = true;
```

Existing hero screen offsets can be approximated into world space using camera right/up vectors.

```cpp
position += cameraRight * (screenOffset.x * kWorldOffsetPerScreenPixel);
position += cameraUp    * (-screenOffset.y * kWorldOffsetPerScreenPixel);
```

This is not exact pixel-to-world matching. v1 only needs to validate spatial feel.

If precision becomes necessary later:

```cpp
worldPerPixel = 2.f * distance * tan(fovY * 0.5f) / viewportHeight;
```

---

## 8. Implementation Order

1. Add `Shader_BattleWorldUiPlane.hlsl`
   - `VTXTEX`
   - `DSS_DepthRead + BS_AlphaBlend`
   - `DSS_None + BS_AlphaBlend`
   - `RS_CullNone`

2. Register `Shader_BattleWorldUiPlane` in `DT_Shader.json`

3. Add `BattleWorldUiPlane`
   - component clone,
   - texture loading,
   - billboard transform,
   - `RenderGroup::Blend` submission.

4. Test the plane standalone before HUD integration
   - spawn one plane at a fixed world position,
   - test `UseDepthRead=false` first,
   - then compare `UseDepthRead=true`.

5. Integrate with `UI_BattleCommandHUD`
   - keep existing 2D buttons/text/AP logic,
   - bind existing background panel,
   - hide old background only when plane and panel are both ready.

6. Add post-camera presentation sync
   - separate state update from presentation update,
   - update 2D projection and 3D plane transform together after camera update.

7. Run regression tests and tune values.

---

## 9. Test Plan

### 9.1 Build

```text
Client/Default/Client.vcxproj
Debug | x64
```

Check:

- Client builds.
- Engine builds.
- New HLSL compiles.
- `DT_Shader.json` registration is valid.

### 9.2 Runtime

Battle state:

```text
BattleTurnState::ChooseAction
```

Check:

- The Command HUD background plane appears near the current character.
- Existing 2D buttons/text/AP/input remain functional.
- The old 2D background and the 3D plane are not visible at the same time.
- Camera movement does not make the 3D background slip away from the 2D text/buttons.
- Repeated `Open()` calls do not cause flicker.
- `Close()` or projection failure hides the plane.
- Verify actual DSV binding during `Render_Blend`.
- Compare `UseDepthRead=true` and `UseDepthRead=false`.

### 9.3 Regression

These should keep working:

- Player HP Bar,
- Monster HP Bar,
- Damage Font,
- Heal Font,
- TurnOrder,
- Crosshair,
- CancelAim,
- Victory UI,
- BattleSkillPanel,
- BattleItemPanel.

### 9.4 Git Checks

```text
git diff --check
git status --short
```

Unexpected changes must not appear in:

- `AssetRegistry.json`,
- build outputs,
- unnecessary `.meta` files,
- existing UI JSON.

---

## 10. Future Work After v1

Only after the single Command HUD background plane is stable:

- add Blend back-to-front sorting,
- add camera FOV/distance based world-per-pixel correction,
- extend to Skill Wheel background,
- extend to QTE ring,
- extend to Crosshair decoration,
- extend to character-near HP/AP mini HUD,
- add UI Editor preview support.

These are not part of v1.

