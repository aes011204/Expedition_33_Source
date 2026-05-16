# 맵 모델 머테리얼 적용 계획

## 1. 현재 상황

- 맵: `SmallLevel_SimonArea`
- 모델 위치: `Client/Bin/Resources/Map/SmallLevel_SimonArea/` (`.model` 바이너리)
- MI JSON 위치: `Client/Bin/Resources/Map/SmallLevel_SimonArea_MI/` (fmodel로 추출한 UE MaterialInstanceConstant)
- 모델 스폰 방식: 맵 에디터에서 `StaticMesh` / `InstancedStaticMesh` 두 종류
- 텍스처 파일: 아직 미추출 (fmodel로 별도 추출 필요)

---

## 2. 머테리얼 → 셰이더 전체 흐름

```
[.material.json]
      ↓  MaterialBuilder::GetOrLoadBundle()
[BundleAsset]  캐싱
      ↓  ModelCom::Apply_EffectiveMaterial()
[Material]  텍스처 SRV 보유
      ↓  ShaderCom::Bind_MaterialTextures()
[g_MaterialTextures[]]  GPU 텍스처 배열
      ↓  ShaderCom::Begin(passIndex)
[HLSL Pass 실행]
```

### 핵심 함수 정리

| 단계 | 함수 | 파일 | 역할 |
|------|------|------|------|
| 번들 로드 | `MaterialBuilder::GetOrLoadBundle()` | `Engine/Public/MaterialBuilder.h` | `.material.json` 파싱 및 캐싱 |
| 머테리얼 적용 | `ModelCom::Apply_EffectiveMaterial()` | `Engine/Private/ModelCom.cpp:361` | 번들에서 텍스처 경로 읽어 SRV 생성 |
| 마스크 갱신 | `Material::Update_MaterialState()` | `Engine/Private/Material.cpp:148` | 슬롯별 SRV 존재 여부 → `materialMask` 비트 세팅 |
| GPU 전송 | `ShaderCom::Bind_MaterialTextures()` | `Engine/Private/ShaderCom.cpp:286` | SRV 배열 → `g_MaterialTextures[]` |
| CB 전송 + 드로우 | 각 오브젝트 `Render()` | 예: `Client/Private/Head_Player.cpp:71` | CB, 패스 인덱스, DrawIndexed |

### 셰이더 슬롯 정의 (`Engine_Shader_Defines.hlsli`)

```
MATERIAL_SLOT_BASECOLOR    = 0  (bit 0)
MATERIAL_SLOT_NORMAL       = 1  (bit 1)
MATERIAL_SLOT_EMISSIVE     = 2  (bit 2)
MATERIAL_SLOT_OPACITY_MASK = 3  (bit 3)
MATERIAL_SLOT_ORM          = 4  (bit 4)
```

샘플링 함수는 모두 `Engine_Shader_Material.hlsl`에 정의:
`SampleBaseColor()`, `ComputeWorldNormal()`, `SampleOrm()`, `SampleEmissive()`

---

## 3. MI JSON 구조와 엔진 매핑

MI JSON (`SmallLevel_SimonArea_MI/*.json`)에서 읽을 수 있는 정보:

| MI JSON 필드 | 엔진 매핑 |
|---|---|
| `TextureParameterValues[].ParameterValue.ObjectName` | 텍스처 파일 이름 |
| `BasePropertyOverrides.BlendMode` | `renderState.blendMode` + `template` |
| `BasePropertyOverrides.TwoSided` | `renderState.twoSided` |
| `BasePropertyOverrides.OpacityMaskClipValue` | `cb.opacityMaskClipValue` |
| `ScalarParameterValues` | `cb.*` (EmissiveIntensity 등) |
| `VectorParameterValues` | `cb.*` (EmissiveColor 등) |

### BlendMode 변환 규칙

```
BLEND_Opaque  → template: 0 (OpaquePBR),  blendMode: 0
BLEND_Masked  → template: 1 (MaskedPBR),  blendMode: 1
```

---

## 4. Packed Texture 문제 (NREmis)

UE에서 추출한 텍스처는 여러 정보가 채널에 팩킹되어 있음.

### `T_..._AlbedoAlpha` (BaseColor 슬롯)
```
RGB = Albedo 색상
A   = Opacity Mask
```
→ `baseColor` 슬롯에 그대로 사용. `SampleBaseColor()`에서 A < 0.34 시 discard.

### `T_..._NREmis` (Normal 슬롯, 팩킹)
```
R = Normal X
G = Normal Y
B = Roughness  ← 현재 셰이더가 Normal Z로 오해
A = Emissive Mask
```

현재 셰이더 문제:
```hlsl
// 현재 (B채널을 Normal Z로 잘못 읽음)
float3 normalTS = g_MaterialTextures[MATERIAL_SLOT_NORMAL].Sample(...).xyz * 2.f - 1.f;
```

수정 방향 (`Engine_Shader_Material.hlsl:70`):
```hlsl
// XY에서 Z를 수학적으로 복원 (일반 노멀맵도 동일하게 동작)
float2 normalXY = g_MaterialTextures[MATERIAL_SLOT_NORMAL].Sample(...).xy * 2.f - 1.f;
normalXY *= g_NormalStrength;
float3 normalTS = float3(normalXY, sqrt(saturate(1.f - dot(normalXY, normalXY))));
```

---

## 5. Emissive 처리

### 텍스처가 있는 경우
`.material.json`에 `textures.emissive` 경로 지정, `overrides.emissive: true`

### 텍스처 없이 CB 값만으로 발광시키는 경우

현재 `SampleEmissive()`는 텍스처가 없으면 `(0,0,0)` 반환해서 CB 값이 무시됨.

수정 방향 (`Engine_Shader_Material.hlsl`):
```hlsl
float3 SampleEmissive(float2 uv)
{
    if (HasMaterialTexture(MATERIAL_SLOT_EMISSIVE))
    {
        float3 emissiveTex = g_MaterialTextures[MATERIAL_SLOT_EMISSIVE].Sample(LinearWrapSampler, uv).rgb;
        return emissiveTex * g_EmissiveColor.rgb * g_EmissiveIntensity
             + emissiveTex * g_EmissiveMaskMul;
    }

    // 텍스처 없이 단색 발광
    return g_EmissiveColor.rgb * g_EmissiveIntensity;
}
```

`.material.json`에서 모델별로 독립 설정:
```json
// 발광 없음
{ "overrides": { "emissive": false }, "textures": { "emissive": "" }, "cb": { "emissiveIntensity": 0.0 } }

// 단색 발광 (텍스처 없이)
{ "overrides": { "emissive": false }, "textures": { "emissive": "" },
  "cb": { "emissiveColor": [1.0, 0.6, 0.1, 0.0], "emissiveIntensity": 2.5 } }

// 텍스처 기반 발광
{ "overrides": { "emissive": true }, "textures": { "emissive": "Models/Map/.../Emissive.png" },
  "cb": { "emissiveIntensity": 1.0 } }
```

### Emissive + Bloom 전체 구현 (추후)

현재 파이프라인에 Emissive RTV와 Bloom 포스트 프로세싱이 없음. 완전한 구현은 3단계:
1. `Target_Emissive` 렌더 타겟 추가 → `MRT_GameObjects`의 `SV_TARGET3`
2. Bloom 포스트 프로세싱 셰이더 추가 (가우시안 블러)
3. `Render_Combined()`에서 최종 합산

현재는 Emissive를 Diffuse에 더하는 방식으로 임시 처리 가능 (`Shader_VtxMesh.hlsl` 주석 해제).

---

## 6. 작업 순서

### Step 1. 텍스처 추출 (fmodel)
- `SmallLevel_SimonArea_MI/*.json`에서 `TextureParameterValues`의 텍스처 이름 목록 수집
- fmodel로 해당 텍스처 파일 추출
- 저장 위치: `Resources/Models/Map/SmallLevel_SimonArea/[모델명]/`

### Step 2. 셰이더 수정
- `Engine_Shader_Material.hlsl:70` — Normal Z 복원 (XY → Z 수학적 계산)
- `Engine_Shader_Material.hlsl` — `SampleEmissive()` 텍스처 없는 경우 분기 추가
- `Shader_VtxMesh.hlsl:117` — Emissive 주석 해제

### Step 3. BatchTool 스크립트
`SmallLevel_SimonArea_MI/*.json` → `Resources/Materials/Map/SmallLevel_SimonArea/*.material.json` 자동 생성

파라미터 이름 → 슬롯 매핑 규칙:
```
Albedo / BaseColor / Diffuse → baseColor 슬롯
Normal                       → normal 슬롯
NREmis (팩킹)                → normal 슬롯 (Z 복원 셰이더 수정 적용)
ORM / Metallic / Roughness   → orm 슬롯
Emissive                     → emissive 슬롯
Alpha / Opacity / Mask       → opacityMask 슬롯

ScalarParameterValues (EmissiveIntensity 등) → cb 필드
VectorParameterValues (EmissiveColor 등)     → cb 필드
```

---

## 7. 관련 파일 위치 요약

| 역할 | 경로 |
|------|------|
| 머테리얼 샘플링 함수 | `Client/Bin/ShaderFiles/Engine_Shader_Material.hlsl` |
| StaticMesh 셰이더 패스 | `Client/Bin/ShaderFiles/Shader_VtxMesh.hlsl` |
| 슬롯 상수 정의 | `Engine/Bin/ShaderFiles/Engine_Shader_Defines.hlsli` |
| 디퍼드 합성 셰이더 | `Client/Bin/ShaderFiles/Shader_Deferred.hlsl` |
| 렌더 파이프라인 | `Engine/Private/Renderer.cpp` |
| 머테리얼 번들 로더 | `Engine/Public/MaterialBuilder.h` |
| 머테리얼 클래스 | `Engine/Public/Material.h` |
| 모델 컴포넌트 | `Engine/Public/ModelCom.h` |
| 머테리얼 슬롯/타입 정의 | `Engine/Public/Material_Types.h` |
| 머테리얼 구조체 정의 | `Engine/Public/Material_Define.h` |
| MI JSON 원본 | `Client/Bin/Resources/Map/SmallLevel_SimonArea_MI/` |
| 기존 .material.json 예시 | `Client/Bin/Resources/Materials/LuneBody.material.json` |
