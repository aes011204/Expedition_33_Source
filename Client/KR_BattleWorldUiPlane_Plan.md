# BattleWorldUiPlane 상세 계획서

## 0. 목적

이 문서는 `Clair Obscur: Expedition 33`의 전투 UI 연출을 레퍼런스로 삼는 `Expedition_158` 프로젝트에서, 전투 중 일부 2D UI 이미지를 월드 공간에 띄우는 `2.5D UI` 실험을 정리한 한국어 계획서다.

목표는 기존 2D UI 시스템 전체를 3D화하는 것이 아니다. 1차 목표는 전투 `Command HUD`의 배경 카드 이미지만 `World-Space 2D Plane`으로 분리하는 것이다.

다음 요소는 기존 2D UI 시스템에 남긴다.

- 커맨드 버튼
- 커맨드 텍스트
- AP 숫자와 AP segment
- 선택 하이라이트
- 입력 처리
- HUD 열림/닫힘 상태 관리

새 `BattleWorldUiPlane`은 배경 카드 이미지 한 장을 3D 전투 공간에 배치하는 시각 요소로만 사용한다.

---

## 1. 현재 프로젝트 맥락

프로젝트 루트:

```text
C:\DATA\Documents\GitHub\Expedition_158
```

주요 관련 파일:

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

현재 `UI_BattleCommandHUD`는 `UI_RuntimePanelBase`를 상속하며, `UI_RuntimeLoader`로 JSON을 읽어 2D UI를 구성한다.

현재 `UI_BattleCommandHUD::Update()`는 `_targetWorldPos`를 화면 좌표로 투영하고, 루트 `Panel_BattleCommand`의 2D 위치를 매 프레임 갱신한다.

현재 `GameplayUI_Battle::Update_BattleCommandHUD()`는 전투 상태가 `BattleTurnState::ChooseAction`일 때 매 프레임 다음 흐름을 실행한다.

```text
Open
Set_TargetWorldPosition
Set_CurrentHeroId
Set_CurrentPlayerAP
Refresh_ForState
Update
```

Command HUD 기준 월드 위치는 현재 턴 캐릭터의 월드 위치에 Y축 0.8을 더한 값이다.

```cpp
outWorldPos = actor.actor->Get_Transform()->Get_WorldPosition();
outWorldPos.y += 0.8f;
```

이 값을 1차 `BattleWorldUiPlane`의 기준 위치로 사용한다.

---

## 2. 현재 Command HUD JSON 구조

관련 JSON:

```text
Client/Bin/Resources/UI/UI_Json/BattleSystem/UI_BattleSystem_HUD.json
```

관련 노드:

```text
Panel_BattleCommand
Panel_BattleCommand_BG
Image_BattleCommand_BG
```

기존 배경 텍스처:

```text
../Bin/Resources/Textures/UI/Battle/HUD/T_HUD_BattleSystem_BackGround.png
```

코드에서 사용할 권장 경로:

```text
Textures/UI/Battle/HUD/T_HUD_BattleSystem_BackGround.png
```

1차 구현에서는 UI JSON 스키마를 변경하지 않는다. 기존 `Panel_BattleCommand_BG`는 fallback용으로 유지한다.

---

## 3. 핵심 설계 결정

### 3.1 RenderGroup

`BattleWorldUiPlane`은 `RenderGroup::Blend`에 제출한다.

이유:

- 화면 공간 UI가 아니라 월드 공간 `GameObject`다.
- 카메라 View/Projection을 타야 한다.
- PNG 알파 블렌딩이 필요하다.
- 전투 공간 안에 있는 카드처럼 보여야 한다.

`RenderGroup::UI`는 사용하지 않는다.

### 3.2 셰이더 기본 패스

`Shader_BattleWorldUiPlane`은 `VTXTEX` 입력 레이아웃을 사용한다.

기본 패스:

```text
DSS_DepthRead + BS_AlphaBlend
```

디버그/비교 패스:

```text
DSS_None + BS_AlphaBlend
```

현재 확인된 렌더 상태:

- `DSS_DepthRead`는 `DepthWriteMask = zero`이므로 depth write를 하지 않는다.
- `BS_AlphaBlend`는 `SrcBlend = Src_Alpha`, `DestBlend = Inv_Src_Alpha`이므로 일반 PNG straight alpha에 적합하다.
- `RS_CullNone`을 사용해 plane 방향/축 문제로 첫 테스트에서 안 보이는 리스크를 줄인다.

### 3.3 DSV 확인 규칙

현재 `Renderer::Render_Blend()`는 다음처럼 호출된다.

```cpp
GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV)
```

하지만 현재 `Target_Manager::Begin_MRT()`는 내부에서 DSV가 없으면 `_originalDSV`를 fallback으로 잡는 코드가 있다.

```cpp
dsv = dsv ? dsv : _originalDSV.Get();
```

따라서 이름상 `NotUseDSV`라도 실제로는 원본 DSV가 바인딩될 가능성이 있다. 다만 이 동작은 명시적 설계라기보다 현재 구현 세부에 가깝다.

결정:

- v1 기본값은 `UseDepthRead = true`로 둔다.
- 단, 첫 런타임 테스트에서 `Render_Blend` 시점의 실제 OM DSV 바인딩을 확인한다.
- DSV가 실제로 바인딩되지 않는다면 v1에서는 `UseDepthRead = false`로 바꾸고, Blend 패스 DSV 정리는 v2 작업으로 분리한다.

### 3.4 Blend 정렬

현재 `Render_Blend()`는 등록 순서대로 렌더한다. 카메라 거리 기반 back-to-front 정렬은 없다.

결정:

- v1은 Command HUD 배경 Plane 1장만 사용하므로 Blend sorting 구현은 보류한다.
- QTE 링, Skill Wheel, HP 미니 HUD 등으로 Plane이 여러 장이 되는 v2 전에 Blend 그룹의 투명 오브젝트 정렬을 확인하거나 추가한다.

---

## 4. 프레임 동기화 규칙

Gemini가 지적한 가장 큰 리스크는 2D UI와 3D 배경 Plane의 desync다.

현재 엔진 업데이트 순서상 `ClientInstance::Update_Client()` 안에서 `GameplayUI_Battle::Update()`가 먼저 실행되고, 그 뒤 오브젝트 업데이트 단계에서 `BattleTurnCamera` 또는 `TargetCamera`가 최종 View/Projection을 갱신한다.

즉 기존처럼 `UI_BattleCommandHUD::Update()`에서 바로 화면 투영을 끝내면, 2D HUD 위치가 이전 카메라 기준으로 계산될 수 있다. 3D Plane까지 섞이면 배경과 텍스트/버튼이 미끄러져 보일 수 있다.

결정:

```text
2D HUD 위치 투영과 3D Plane transform 갱신은 카메라 갱신 이후 같은 프레임의 최종 View/Proj 기준으로 함께 처리한다.
```

구현 방향:

1. `GameplayUI_Battle::Update_BattleCommandHUD()`는 상태 갱신만 담당한다.
   - HUD를 열지/닫을지 결정
   - target world position 저장
   - current hero id 저장
   - AP 값 저장
   - command highlight state 저장
   - 최종 2D 투영과 3D Plane 제출은 하지 않음

2. 카메라 갱신 이후 호출되는 presentation sync 단계를 추가한다.
   - 예: `GameplayUI::Sync_PresentationAfterCamera(float TimeDelta)`
   - 예: `GameplayUI_Battle::Sync_PresentationAfterCamera(float TimeDelta)`
   - 예: `UI_BattleCommandHUD::Sync_PresentationAfterCamera(float TimeDelta)`

3. 엔진 업데이트 루프에는 카메라 업데이트 이후, `UI_Manager::Update()` 이전에 클라이언트 presentation sync callback을 넣는다.

권장 위치:

```text
_cameraManager->Update(...)
CLIENT presentation sync
_uiManager->Update(...)
Draw
```

이렇게 하면 2D 버튼/텍스트와 3D 배경 Plane이 같은 최종 카메라 행렬을 기준으로 갱신된다.

---

## 5. 새 클래스 설계

추가 파일:

```text
Client/Public/BattleWorldUiPlane.h
Client/Private/BattleWorldUiPlane.cpp
```

클래스:

```cpp
class BattleWorldUiPlane final : public GameObject
```

사용 컴포넌트:

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

v1 기본값은 `FaceCamera`다.

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

### 5.3 주요 메서드

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

`Submit_Render()`는 `_renderReady && _visible`일 때만 `RenderGroup::Blend`에 자신을 제출한다.

---

## 6. UI_BattleCommandHUD 통합 설계

추가 멤버:

```cpp
Shared<UI_Base> _screenBackgroundPanel{};
Shared<BattleWorldUiPlane> _worldBackgroundPlane{};
bool _useWorldBackgroundPlane = false;
```

추가 메서드:

```cpp
HRESULT Ready_WorldBackgroundPlane();
void Sync_PresentationAfterCamera(float TimeDelta);
void Set_ScreenBackgroundVisible(bool Visible);
Vec3 Resolve_WorldBackgroundPosition() const;
```

### 6.1 fallback 조건 강화

기존 2D 배경을 숨기는 조건은 다음을 모두 만족할 때만 true로 한다.

```cpp
_useWorldBackgroundPlane =
    _worldBackgroundPlane != nullptr &&
    _worldBackgroundPlane->Is_RenderReady() &&
    _screenBackgroundPanel != nullptr;
```

이유:

- Plane은 성공했지만 기존 2D 배경 패널을 찾지 못하면 2D 배경과 3D Plane이 동시에 보일 수 있다.
- v1에서는 “기존 배경 대체”가 목적이므로 패널 바인딩 성공까지 요구한다.

### 6.2 Open 반복 호출 안전화

현재 `GameplayUI_Battle::Update_BattleCommandHUD()`는 `ChooseAction` 동안 매 프레임 `Open()`을 호출한다.

따라서 새 통합에서는 `Open()`이 매 프레임 Plane visibility를 false로 리셋하면 안 된다.

권장 방식:

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

또는 Plane visibility 제어를 `Close()`와 `Sync_PresentationAfterCamera()`로만 제한한다.

### 6.3 Close 처리

`Close()`에서는 반드시 Plane을 숨기고, 기존 2D 배경을 다시 열어 fallback 상태를 복원한다.

```cpp
if (_worldBackgroundPlane)
    _worldBackgroundPlane->Set_Visible(false);

Set_ScreenBackgroundVisible(true);
```

---

## 7. 위치 계산

기준 위치:

```cpp
_targetWorldPos = actor position + Vec3(0.f, 0.8f, 0.f);
```

초기 튜닝값:

```cpp
WorldSize = Vec2(1.55f, 1.80f);
TintColor = Vec4(1.f, 1.f, 1.f, 0.92f);
BillboardMode = FaceCamera;
UseDepthRead = true;
```

기존 hero별 screen offset은 1차에서 카메라 Right/Up 방향으로 근사 변환한다.

```cpp
position += cameraRight * (screenOffset.x * kWorldOffsetPerScreenPixel);
position += cameraUp    * (-screenOffset.y * kWorldOffsetPerScreenPixel);
```

이 방식은 정확한 pixel-to-world matching이 아니다. v1 목적은 공간감 확인이다.

정밀 보정이 필요하면 후속으로 다음 공식을 사용한다.

```cpp
worldPerPixel = 2.f * distance * tan(fovY * 0.5f) / viewportHeight;
```

---

## 8. 구현 순서

1. `Shader_BattleWorldUiPlane.hlsl` 추가
   - `VTXTEX`
   - `DSS_DepthRead + BS_AlphaBlend`
   - `DSS_None + BS_AlphaBlend`
   - `RS_CullNone`

2. `DT_Shader.json`에 `Shader_BattleWorldUiPlane` 등록

3. `BattleWorldUiPlane` 클래스 추가
   - component clone
   - texture load
   - billboard transform
   - `RenderGroup::Blend` submit

4. 단독 렌더 테스트
   - HUD에 연결하기 전에 고정 월드 위치에 Plane 1장을 강제로 띄운다.
   - 먼저 `UseDepthRead=false`로 보이는지 확인한다.
   - 이후 `UseDepthRead=true`로 DSV/depth 반응을 비교한다.

5. `UI_BattleCommandHUD`에 연결
   - 기존 2D 버튼/텍스트/AP 로직 유지
   - 기존 2D 배경 패널 바인딩
   - Plane 준비 성공 및 배경 패널 바인딩 성공 시에만 기존 배경 숨김

6. post-camera presentation sync 추가
   - 상태 갱신과 표시 갱신 분리
   - 카메라 갱신 이후 2D HUD 투영과 3D Plane transform을 함께 갱신

7. 회귀 확인 및 튜닝

---

## 9. 테스트 계획

### 9.1 빌드

```text
Client/Default/Client.vcxproj
Debug | x64
```

확인:

- Client 빌드 성공
- Engine 빌드 성공
- 새 HLSL 컴파일 성공
- `DT_Shader.json` 등록 오류 없음

### 9.2 런타임

전투 상태:

```text
BattleTurnState::ChooseAction
```

확인:

- Command HUD 배경 Plane이 현재 턴 캐릭터 근처에 표시된다.
- 기존 2D 버튼/텍스트/AP/입력은 유지된다.
- 2D 배경과 3D Plane이 동시에 보이지 않는다.
- 카메라 이동 중 배경과 버튼/텍스트가 미끄러지지 않는다.
- `Open()`이 매 프레임 호출되어도 Plane flicker가 없다.
- `Close()` 또는 projection 실패 시 Plane이 확실히 숨겨진다.
- `Render_Blend` 시점에 실제 DSV 바인딩 여부를 확인한다.
- `UseDepthRead=true/false` 결과를 비교한다.

### 9.3 회귀

다음 UI가 기존처럼 동작해야 한다.

- Player HP Bar
- Monster HP Bar
- Damage Font
- Heal Font
- TurnOrder
- Crosshair
- CancelAim
- Victory UI
- BattleSkillPanel
- BattleItemPanel

### 9.4 Git 확인

```text
git diff --check
git status --short
```

의도치 않은 다음 파일 변경이 없어야 한다.

- `AssetRegistry.json`
- 빌드 산출물
- 불필요한 `.meta`
- 기존 UI JSON

---

## 10. v1 이후 과제

v1에서 Command HUD 배경 카드 1장이 안정적으로 동작하면 다음을 검토한다.

- Blend 그룹 back-to-front 정렬
- 카메라 FOV/거리 기반 world-per-pixel 보정
- Skill Wheel 배경
- QTE 링
- Crosshair 보조 장식
- 캐릭터 근처 HP/AP 미니 HUD
- UI Editor 미리보기 지원

v1에서는 위 항목을 구현하지 않는다.

