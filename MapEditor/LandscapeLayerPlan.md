# Landscape Layer 시스템 개선 계획

## 목표

`MI_Landscape_General_Multiface` 머테리얼 분석 결과를 기반으로  
지형 레이어를 4개 → 7개로 확장하고, 레이어당 Normal/Roughness 텍스처 및  
PBR 파라미터(Tiling, GroundTint, NormalIntensity, RoughnessMultiply)를 지원한다.

---

## 완료된 작업

| 파일 | 변경 내용 |
|---|---|
| `Engine/Public/Engine_Static.h` | `MAX_LAYERS` 4 → 7 |
| `MapEditor/Public/MELandscapeActor.h` | `layerNames[]`, `weightNames[]` 7개로 확장 |
| `Client/Public/MapLandscape.h` | 동일 |
| `Client/Bin/ShaderFiles/Shader_VtxNorTex.hlsl` | `g_Layer4/5/6`, `g_Weight4/5/6` 선언 추가, `PS_Layer` 7개 블렌딩으로 수정 |

---

## 남은 작업 목록

### 1. Shader_VtxNorTex.hlsl — PBR 텍스처 슬롯 및 cbuffer 추가

```hlsl
// 추가할 텍스처 슬롯
Texture2D g_Normal0, g_Normal1, g_Normal2, g_Normal3, g_Normal4, g_Normal5, g_Normal6;
Texture2D g_Roughness0, g_Roughness1, g_Roughness2, g_Roughness3, g_Roughness4, g_Roughness5, g_Roughness6;

// 추가할 cbuffer
cbuffer LayerParams : register(b1)
{
    float4 g_Tiling[7];        // x = tiling 배율
    float4 g_GroundTint[7];    // rgb = tint 색상
    float4 g_LayerControl[7];  // x = NormalIntensity, y = RoughnessMultiply, z = Specular
};
```

`PS_Layer`에서:
- 레이어별 `tiledUV = In.Texcoord * g_Tiling[i].x` 적용
- Albedo에 `g_GroundTint` 곱하기
- Normal map을 `ddx/ddy`로 계산한 TBN으로 변환 후 가중 혼합
- Roughness를 `g_LayerControl[i].y`로 곱한 뒤 가중 혼합
- Normal이 없는 레이어는 버텍스 노멀 사용

---

### 2. MELandscapeActor.h — 멤버 추가

```cpp
// 텍스처 SRV
ComPtr<ID3D11ShaderResourceView> _normalSRVs[MAX_LAYERS];
ComPtr<ID3D11ShaderResourceView> _roughnessSRVs[MAX_LAYERS];

// 파일 경로
wstring _normalPaths[MAX_LAYERS];
wstring _roughnessPaths[MAX_LAYERS];

// 레이어 파라미터
float  _tiling[MAX_LAYERS]           = { 3.f, 3.f, 3.14f, 3.f, 10.f, 7.f, 3.68f };
Vec4   _groundTint[MAX_LAYERS];
Vec4   _layerControl[MAX_LAYERS];    // x=NormalIntensity, y=RoughnessMultiply, z=Specular
```

함수 시그니처 변경:
```cpp
// 기존
HRESULT Add_LayerTexture(const wstring& albedoPath);

// 변경 후
HRESULT Add_LayerTexture(const wstring& albedoPath,
                         const wstring& normalPath = L"",
                         const wstring& roughnessPath = L"");
```

추가 Getter:
```cpp
const wstring& Get_NormalPath(uint32 i) const { return _normalPaths[i]; }
const wstring& Get_RoughnessPath(uint32 i) const { return _roughnessPaths[i]; }
```

---

### 3. MELandscapeActor.cpp — 함수 수정

- `Add_LayerTexture` : normal/roughness 경로가 있으면 SRV 로드, 없으면 nullptr 유지
- `Bind_ShaderResources` : `g_Normal*`, `g_Roughness*` 바인딩 추가 (없으면 dummy SRV), cbuffer 바인딩
- `Clear_Layers` : `_normalSRVs`, `_roughnessSRVs`, `_normalPaths`, `_roughnessPaths` 초기화 추가
- `Remove_LayerTexture` : normal/roughness도 동일하게 shift
- `Set_BaseLayer` : normal/roughness도 동일하게 swap

---

### 4. MELandscapeManager.cpp — Save/Load 포맷 변경

**전략: Save 먼저 수정 → 에디터에서 기존 파일 재저장 → Load 수정**

새 Save 포맷 (레이어 1개 단위):
```
[albedoNameLen (uint32)]
[albedoName   (wchar_t * len)]
[normalNameLen (uint32)]          ← 신규 (없으면 0)
[normalName   (wchar_t * len)]    ← 신규 (len=0이면 생략)
[roughnessNameLen (uint32)]       ← 신규 (없으면 0)
[roughnessName (wchar_t * len)]   ← 신규 (len=0이면 생략)
[tiling (float)]                  ← 신규
[groundTint (float4)]             ← 신규
[layerControl (float4)]           ← 신규
[weights (float * vertCount)]
```

Load 코드는 `normalNameLen == 0`이면 normal/roughness 경로 스킵.

> **작업 순서 주의**  
> 1. Save 코드 수정 → 빌드  
> 2. 에디터 실행 → 기존 `.bin` 파일 Load (구 Load 코드) → 그대로 Save  
> 3. Load 코드 수정 → 재빌드  
> 4. 새 포맷으로 Load 확인  

현재 대상 파일:
- `SmallLevel_MF_Zone_01_Mk2.bin`
- `SpringMeadows_LandscapePreview.bin`

---

### 5. Client/Private/MapLandscape.cpp — 클라이언트 사이드 동일 적용

- `_normalSRVs`, `_roughnessSRVs` 멤버 추가 (MapLandscape.h)
- Load 코드에서 normal/roughness 경로 읽고 SRV 생성
- `Bind_ShaderResources`에서 바인딩

---

### 6. PanelLandscape.cpp — UI 변경

#### 레이어 추가 팝업 (방법 A)

`+ 레이어 추가` 클릭 시 별도 다이얼로그:

```
┌─────────────────────────────────────┐
│  레이어 추가                          │
│                                     │
│  Albedo    [ Albedo_2K_vec... ] [선택] │  ← 필수
│  Normal    [ Normal_2K_vec... ] [선택] │  ← 선택사항
│  Roughness [ -             ] [선택] │  ← 선택사항
│                                     │
│          [추가]      [취소]           │
└─────────────────────────────────────┘
```

#### 레이어별 파라미터 편집

레이어 목록에서 각 항목을 펼치면:

```
▶ Layer 0  Albedo_2K_vecneiulw.dds   x  !
  Tiling        [3.00 ]
  Ground Tint   [R:0.91 G:0.02 B:0.00]
  Normal Intensity [2.00]
  Roughness x   [1.69 ]
```

#### 기타

- `레이어 (%u / 4)` → `레이어 (%u / 7)` 로 수정 (line 411)
- `if (layerCount < 4)` → `if (layerCount < MAX_LAYERS)` 로 수정 (line 455)

---

## 레이어 텍스처 매핑 (MD 파일 기준)

| 슬롯 | Albedo | Normal | Roughness |
|---|---|---|---|
| Layer 0 | `Albedo_2K_vecneiulw.dds` | `Normal_2K_vecneiulw.dds` | — |
| Layer 1 | `T_Dry_Mud_Ground_uhpmbido_4K_D.dds` | `T_Dry_Mud_Ground_uhpmbido_4K_N.dds` | — |
| Layer 2 | `Albedo_2K_tcglbihg.dds` | `Normal_2K_tcglbihg.dds` | — |
| Layer 3 | `T_Powdered_Coal_And_Soil_vbtmajon_4K_D.dds` | `T_Powdered_Coal_And_Soil_vbtmajon_4K_N.dds` | — |
| Layer 4 | `boudin_boudin_BaseColor.dds` | `boudin_boudin_Normal.dds` | `boudin_boudin_Roughness.dds` |
| Layer 5 | `T_ground_wet_dirt_04_BC_H.dds` | `T_ground_wet_dirt_04_N.dds` | `T_ground_wet_dirt_04_Roughness.dds` |
| Layer 6 | `Albedo_2K_vecneiulw.dds` (재사용) | `Normal_2K_vecneiulw.dds` | `Roughness_2K_vecneiulw.dds` |

---

## 레이어별 기본 파라미터 (MD 파일 기준)

| 슬롯 | 이름 | Tiling | NormalIntensity | RoughnessMultiply | Specular | GroundTint |
|---|---|---|---|---|---|---|
| 0 | Moss Ground | 3.0 | 2.0 | 1.692 | 0.4 | #F62600 |
| 1 | Dry Mud Ground | 3.0 | 3.0 | 0.936 | — | #FFFFFF |
| 2 | Icelandic Gravel Rock | 3.142 | 1.919 | 1.239 | — | #F8E8EB |
| 3 | Powdered Coal And Soil | 3.0 | 4.0 | 2.825 | — | — |
| 4 | MultiFace (boudin) | 10.0 | 3.0 | 0.8 | 1.0 | #FFE1C2 |
| 5 | Wet Dirt | 7.0 | 1.2 | 2.0 | — | #FF764E |
| 6 | Moss Ground (재사용) | 3.677 | 3.0 | 0.4 | — | #FFFFFF(HDR) |

---

## 구현 순서 요약

```
[1] Shader  — g_Normal/g_Roughness 슬롯 + cbuffer LayerParams + PS_Layer PBR 로직
[2] MELandscapeActor.h  — SRV/경로/파라미터 멤버 추가, Add_LayerTexture 시그니처 변경
[3] MELandscapeActor.cpp  — Add_LayerTexture / Bind_ShaderResources / Clear / Remove / Swap 수정
[4] MELandscapeManager.cpp  — Save 포맷 수정 → 에디터에서 기존 파일 재저장
[5] MELandscapeManager.cpp  — Load 포맷 수정
[6] MapLandscape.h/.cpp  — 클라이언트 사이드 동기화
[7] PanelLandscape.cpp  — 레이어 추가 팝업 + 파라미터 편집 UI
```
