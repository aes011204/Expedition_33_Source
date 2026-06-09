# 맵 머티리얼 작업 가이드

## 작업 개요

`SmallLevel_MF_Zone_01` 맵(및 이후 다른 맵들)의 모델 머티리얼을 1개씩 검수하고 수정하는 작업.
모델에 잘못 매핑된 텍스처, 누락된 ORM 데이터 등을 잡는 것이 목표.

---

## 관련 디렉토리 구조

```
Client/Bin/Resources/
├── Map/
│   ├── SmallLevel_MF_Zone_01/          ← 모델 JSON + .model 파일
│   ├── SmallLevel_MF_Zone_01_MI/       ← MI JSON (머티리얼 인스턴스 파라미터)
│   ├── NoMatJson/
│   │   └── SmallLevel_MF_Zone_01_report.txt   ← 문제 리포트
│   └── NeedModelJson/
│       └── SmallLevel_MF_Zone_01_models.json  ← 전체 모델 목록
└── Materials/
    └── SmallLevel_MF_Zone_01.material.json    ← 실제 적용되는 머티리얼 번들
```

---

## 워크플로우 (BatchTool/RunMapBuild.bat)

| 스텝 | 스크립트 | 역할 |
|------|----------|------|
| Step 2 | `CollectMapMI.py --level-dir [LevelName]` | Sandfall 익스포트에서 MI JSON 수집 |
| Step 3 | `GenerateMapMaterials.py --level-dir [LevelName]` | `.material.json` 번들 생성 |
| Step 4 | `CollectMapTextures.py --level-dir [LevelName]` | 텍스처 파일 복사 |

---

## MI JSON → material.json 매핑 규칙

### 텍스처 슬롯 매핑

| MI JSON 키 | material.json 슬롯 |
|-----------|-------------------|
| `Base Color Map`, `PM_Diffuse`, `Albedo` | `baseColor` |
| `Normal Map`, `PM_Normals` | `normal` |
| `ORM Map`, `PM_SpecularMasks` | `orm` |
| `Opacity` | `opacityMask` |

### 텍스처 경로 변환

```
/Game/Characters/.../T_Foo_BaseColor.T_Foo_BaseColor
→ Map/[LevelName]/Textures/T_Foo_BaseColor.png
(Normal은 Textures/Normal/ 서브폴더)
```

### renderState / template

| 조건 | template | blendMode | twoSided |
|------|----------|-----------|----------|
| 일반 오파크 | 0 | 0 | false |
| `BLEND_Masked` | 1 | 1 | true 가능 |

---

## 채널 상수 (Engine_Shader_Defines.hlsli)

```
CONST0=0, CONST1=1, R=2, G=3, B=4, A=5
```

---

## NMAo 텍스처 처리 방법

`NMAo` = Normal(RG) + Metallic(B) + AO(A) 가 한 텍스처에 packed.

### material.json 설정

```json
"overrides": { "normal": true, "orm": true },
"textures": {
  "normal": "Textures/Normal/T_Foo_NMAo.png",
  "orm":    "Textures/T_Foo_NMAo.png"
},
"cb": {
  "ormAoSource":        5,
  "ormRoughnessSource": 1,
  "ormMetalnessSource": 4
}
```

- `ormAoSource: 5` → A채널 = AO
- `ormRoughnessSource: 1` → CONST1(1.0) × roughnessScale = 고정 roughness
- `ormMetalnessSource: 4` → B채널 = Metallic

---

## AR + NMAo 분리 케이스 (ORM2 슬롯)

Roughness가 AR 텍스처에, Metallic+AO가 NMAo 텍스처에 분리된 경우.

### material.json 설정

```json
"overrides": { "normal": true, "orm": true, "orm2": true },
"textures": {
  "normal": "Textures/Normal/T_Foo_NMAo.png",
  "orm":    "Textures/T_Foo_NMAo.png",
  "orm2":   "Textures/T_Foo_AR.png"
},
"cb": {
  "ormAoSourceSlot": 0,        "ormAoSource": 5,
  "ormRoughnessSourceSlot": 1, "ormRoughnessSource": 5,
  "ormMetalnessSourceSlot": 0, "ormMetalnessSource": 4
}
```

- `ormXxxSourceSlot: 0` = ORM 슬롯에서 읽기
- `ormXxxSourceSlot: 1` = ORM2 슬롯에서 읽기

### ORM2 슬롯 추가를 위해 수정된 파일 목록

| 파일 | 수정 내용 |
|------|-----------|
| `Engine/Bin/ShaderFiles/Engine_Shader_Defines.hlsli` | `MATERIAL_SLOT_ORM2=9`, `END=10`, `FLAG` 추가 |
| `Engine/Public/Material_Types.h` | `ORM2` enum 추가 |
| `Engine/Public/Shader_CBuffer_Define.h` | `ormAoSourceSlot`, `ormRoughnessSourceSlot`, `ormMetalnessSourceSlot` + `_pad1` 조정 |
| `Engine/Public/MaterialCB_OverrideFields.inl` | 3개 UInt 등록 |
| `Engine/Public/MaterialBuilder.h` | `orm2Override`, `orm2Path` 추가 |
| `Engine/Private/MaterialBuilder.cpp` | CB/overrides/textures 파싱 추가 |
| `Engine/Public/Material_Define.h` | `orm2` TextureOverride 추가 |
| `Engine/Private/ModelCom.cpp` | ORM2 슬롯 바인딩 3곳 추가 |
| `Engine/Bin/ShaderFiles/Engine_Shader_ConstantBuffer.hlsl` (Engine+Client) | MaterialBuffer에 3개 변수 추가 |
| `Engine/Bin/ShaderFiles/Engine_Shader_Material.hlsl` (Engine+Client) | `SampleOrm` 수정 |

---

## 검수 체크리스트 (모델 1개 기준)

1. 모델 JSON (`SmallLevel_MF_Zone_01/[Model].json`) 열어서 `StaticMaterials` 확인
2. 각 슬롯의 MI 이름 추출
3. `SmallLevel_MF_Zone_01_MI/[MI].json` 열어서 텍스처 목록 확인
4. `SmallLevel_MF_Zone_01.material.json`에 해당 `assetName` 항목 존재 여부 확인
5. 텍스처 파일이 `SmallLevel_MF_Zone_01/Textures/` 에 실제로 존재하는지 확인
6. NMAo 여부 확인 → orm 슬롯 및 채널 소스 설정
7. AR+NMAo 분리 여부 확인 → orm2 슬롯 설정

---

## 현재 진행 상태

- [x] 엔진 ORM2 슬롯 추가 완료
- [ ] SmallLevel_MF_Zone_01 모델 머티리얼 검수 진행 중
  - [x] Contort1 — 완료 (이상 없음)
  - [x] FR_1 — 완료 (NMAo orm 처리 수정)
  - [ ] 나머지 모델 검수 필요
