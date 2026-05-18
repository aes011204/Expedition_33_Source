# Per-Model Material Pass Override 시스템 설계

## 배경 및 문제

### 문제 1: 맵 로드 전 드래그앤드랍 스폰 시 모델이 안 보임

**증상**
- MapEditor에서 맵 로드 없이 `.model` 파일을 드래그앤드랍하면 모델이 보이지 않음
- 맵 로드 후 드래그앤드랍하면 정상 표시

**원인** (`MapEditorApp.cpp:226`)
```cpp
if (!_scene._modelFolder.empty())  // 맵 로드 전엔 항상 false
{
    // 머테리얼 바인딩 전체 스킵됨
    model->Apply_MaterialByAssetName(i, bundleKey, miName);
}
```
- `_modelFolder`는 `SceneContext::LoadFromFile()`(SceneContext.cpp:168)에서 맵 JSON의 `"ModelFolder"` 값을 읽어 세팅
- 맵 로드 전에는 비어있으므로 머테리얼 번들을 못 찾아 머테리얼 없는 상태로 렌더링됨

**수정 방향**
- 모델 경로가 항상 `Resources/Map/{ModelFolder}/` 구조임을 이용해 경로에서 폴더명을 파싱
- `_modelFolder`가 비어있을 때 `obj->_modelPath`에서 자동 유도하여 `bundleKey` 생성
- `UpdateRenderHandles()` 내부에서 처리
- 드래그앤드랍 fix 적용 후 맵 로드 없이 모델만 스폰한 상태에서도 머테리얼 설정 가능

---

### 문제 2: 특수 머테리얼 처리가 필요한 오브젝트

**요구사항**
- 칼 외곽선, 발광 등 특수 렌더링이 필요한 오브젝트들이 존재
- MapEditor에서 설정 → 저장 → Client에서 동일하게 적용되어야 함
- 기존 오브젝트(수정 없음)는 기존 머테리얼 흐름 그대로 유지

---

## 설계: Per-Model Pass Override

### 적용 단위: 모델 단위 (Model-Unit)

오브젝트 단위가 아닌 **모델 경로 단위**로 패스를 관리한다.

**이유**
- `ModelCom`이 경로 기준으로 캐시 공유되므로, 특정 인스턴스만 패스를 바꾸면 같은 모델을 쓰는 모든 인스턴스가 함께 바뀜 — 오브젝트 단위 독립 적용이 구조적으로 불가
- **InstancedMesh는 GPU 구조상 무조건 모델 단위** — 인스턴싱은 동일 드로우콜로 전체를 렌더링하므로 인스턴스별 패스 분리 원천 불가
- 실제 사용 패턴상 "이 모델은 전부 외곽선" 식의 모델 단위 처리가 자연스러움
- 설정 작업량이 대폭 감소 (같은 모델 수백 개여도 한 줄로 처리)

**예외 케이스 발생 시**
- 같은 메시인데 일부는 일반, 일부는 특수 처리가 필요한 경우 → 모델 파일 자체를 분리하는 것이 올바른 방향

### 핵심 원칙

```
기본 모델:  Apply_MaterialByAssetName() → 번들 자동 적용 → 끝
특수 모델: Apply_MaterialByAssetName() → 번들 자동 적용 → Set_ForcedPassIndex(N) 덮어씀
```

`PassIndex`가 없거나 `-1`이면 기존 흐름 그대로, 지정된 모델만 패스를 오버라이드하므로 하위 호환 완전 보장

### JSON 구조

모델 경로 키로 별도 섹션(`ModelOverrides`)에서 관리:

```json
{
  "MapData": {
    "MapName": "SmallLevel_SimonArea",
    "ModelFolder": "SmallLevel_SimonArea",
    "ModelOverrides": {
      "SM_Sword.model":      { "PassIndex": 3 },
      "SM_GlowingRock.model": { "PassIndex": 4 }
    },
    "StaticMeshes":   [ ... ],
    "InstancedMeshes": [ ... ]
  }
}
```

- 같은 모델이 맵에 수백 개 있어도 `ModelOverrides`에서 한 줄로 관리
- 나중에 진짜 오브젝트 단위 예외가 필요하면 각 오브젝트 항목에 `"PassIndex"` 추가하는 방식으로 확장 가능 (오브젝트 > 모델 우선순위)

### 데이터 흐름

```
MapEditor Properties 패널 (드롭다운으로 패스 선택)
        ↓
SceneContext._modelOverrides (map<string, int32>)
        ↓ SaveToFile
Map JSON: "ModelOverrides": { "SM_Sword.model": { "PassIndex": 3 } }
        ↓ LoadFromFile / Client MapLoader
모델 로드 후 Set_ForcedPassIndex(N) 호출
        ↓
Shader가 올바른 패스로 렌더링
```

### Model Override 패널 UI

별도 패널(`PanelModelOverride`)에서 모델 단위로 관리:

```
┌─ Model Override ─────────────────────────────────────┐
│  [+ 선택 오브젝트 추가]                                │
│                                                      │
│  ▼ SM_Sword.model              Pass: [Outline ▼] [X] │
│    └─ Slot 0: MI_Sword_Blade                         │
│         normalStrength    [ 1.00 ]                   │
│         roughnessScale    [ 0.50 ]                   │
│         emissiveIntensity [ 2.00 ]                   │
│         emissiveColor     [■ R  G  B ]               │
│         baseColorTint     [■ R  G  B ]               │
│                                                      │
│  ► SM_Rock.model               Pass: [Default ▼] [X] │
└──────────────────────────────────────────────────────┘
```

- **[+ 선택 오브젝트 추가]**: 뷰포트/Hierarchy에서 선택된 오브젝트의 모델 경로를 목록에 추가
- **Pass 드롭다운**: 셰이더 패스 선택, 변경 즉시 뷰포트에 반영
- **CB 수치 슬라이더**: normalStrength / roughnessScale / emissiveIntensity / emissiveColor / baseColorTint 등 실시간 조정
- **[X]**: 해당 모델의 오버라이드 제거 → 기본 머테리얼로 복귀
- **▼/►**: 펼치기/접기로 슬롯 상세 표시

### 저장 위치 분리

| 데이터 | 저장 위치 | 이유 |
|--------|-----------|------|
| PassIndex | 맵 JSON `ModelOverrides` | 패스는 번들과 무관한 맵 단위 설정 |
| CB 수치 (roughness 등) | `.material.json` 직접 수정 | Client가 이미 이 파일을 읽으므로 별도 파이프라인 불필요, 번들이 이미 맵 이름 단위로 분리되어 있음 |

### 변경 위치

| 파일 | 변경 내용 |
|------|-----------|
| `MapEditor/Public/SceneContext.h` | `map<string, int32> _modelOverrides` 필드 추가 |
| `MapEditor/Private/SceneContext.cpp` SaveToFile | `"ModelOverrides"` 섹션 저장 |
| `MapEditor/Private/SceneContext.cpp` LoadFromFile | `"ModelOverrides"` 읽어서 `_modelOverrides` 세팅 |
| `MapEditor/Private/MapEditorApp.cpp` UpdateRenderHandles | ① 드래그앤드랍 fix (경로에서 modelFolder 파싱) ② 머테리얼 바인딩 후 `_modelOverrides`에서 해당 모델 패스 조회 → `Set_ForcedPassIndex()` 호출 |
| `MapEditor/Private/PanelModelOverride.h/.cpp` (신규) | Model Override 전용 패널 — 패스 드롭다운 + CB 수치 편집 UI |
| `Client/Private/MapLoader.cpp` | `"ModelOverrides"` 읽어서 모델 로드 후 패스 적용 |

---

## 미구현 항목: Outline 패스

**현재 상태**
- 칼 외곽선(Outline) 같은 특수 효과는 엔진에 패스로 아직 없음
- 현재 `VtxMeshPassIndex`에 정의된 패스:
  ```cpp
  DefaultPass       = 0
  ShadowPass        = 1
  NoTexturePass     = 2
  MaskedPass        = 3
  TwoSidedPass      = 4
  MaskedTwoSidedPass = 5
  ```

**구현 시 할 일**
- 엔진 셰이더에 Outline 패스 추가 (스텐실 or 스케일 기반)
- `VtxMeshPassIndex`에 `OutlinePass = N` 상수 추가
- `ShaderPassFinder`에 매핑 추가
- 위 Per-Model Pass Override 시스템과 연동

---

## 관련 파일 경로

| 역할 | 경로 |
|------|------|
| 머테리얼 번들 | `Client/Bin/Resources/Materials/{MapName}.material.json` |
| 모델 폴더 | `Client/Bin/Resources/Map/{MapName}/` |
| 머테리얼 인스턴스 | `Client/Bin/Resources/Map/{MapName}_MI/` |
| 맵 저장 JSON | `Client/Bin/Resources/Map/SaveJson/{MapName}.json` |
| Pass 정의 | `Engine/Public/ShaderPassFinder.h` |
| 머테리얼 적용 | `Engine/Private/ModelCom.cpp:916` (Apply_MaterialByAssetName) |
| 런타임 렌더 (Static) | `Client/Private/MapStaticMesh.cpp:93` |
| 런타임 렌더 (Instanced) | `Client/Private/MapInstancedMesh.cpp:116` |
