# MapEditor 임포트 분석 문서

## 개요

`scale_map.py`를 통한 UE5 FModel JSON → MapEditor SaveJson 변환 파이프라인의 현황 및 미처리 항목 정리.

---

## 1. InstancedStaticMesh 처리 현황

### `_isInstanced` 플래그

- `EditorObject._isInstanced = true` 는 임포트 시 일시적으로 설정됨
- 목적: `UpdateRenderHandles()`에서 수천 개 인스턴스에 대해 렌더 핸들 생성을 스킵하기 위함
- **문제**: `SceneContext::SaveToFile` / `LoadFromFile`에서 이 플래그를 저장/복원하지 않음
  - 저장 후 재로드 시 `_isInstanced = false`로 초기화됨
  - 재로드된 오브젝트는 렌더 핸들이 요청 시 생성됨 → 성능상 허용 가능한 수준

### 관련 코드 위치

| 파일 | 라인 | 내용 |
|------|------|------|
| `MapEditor/Public/EditorObject.h` | 43 | `bool _isInstanced = false;` 선언 |
| `MapEditor/Private/MapEditorApp.cpp` | 235 | `_isInstanced == true`이면 렌더 핸들 생성 스킵 |
| `MapEditor/Private/MEUe5Importer.cpp` | 168 | `ParseInstanced()` 후 플래그 설정 (현재 제거됨, scale_map.py로 대체) |
| `MapEditor/Private/SceneContext.cpp` | — | `_isInstanced` 저장/복원 없음 |

---

## 2. OriginJson 컴포넌트 타입 전수 조사

파일: `Client/Bin/Resources/Map/OriginJson/SmallLevel_SimonArea.json`

### 전체 타입 목록

| Type | 수량 | 처리 여부 |
|------|------|-----------|
| `StaticMeshComponent` | 286 | ✅ 파싱 (메시 있는 것 245개, 없는 것 41개) |
| `StaticMeshActor` | 245 | — 메인 JSON에서는 자식 StaticMeshComponent를 통해 간접 처리됨 |
| `InstancedStaticMeshComponent` | 12 (총 인스턴스 458개) | ✅ 파싱 |
| `SpotLightComponent` | 29 | ❌ 미처리 |
| `NiagaraComponent` | 33 | ❌ 미처리 (스킵 의도) |
| `SpotLight` | 27 | ❌ 미처리 |
| `BoxComponent` | 23 | ❌ 미처리 |
| `DecalComponent` | 8 | ❌ 미처리 |
| `PointLightComponent` | 3 | ❌ 미처리 |
| `DirectionalLightComponent` | 4 | ❌ 미처리 |
| `SF_GeneratedInvisibleWallBoxComponent` | 3 | ❌ 미처리 (스킵 의도) |
| 기타 (WorldPartition, BP_*, Audio 등) | — | ❌ 미처리 (스킵 의도) |

### 검증 결과

- `StaticMeshComponent` 286개 중 메시 있는 것: **245개**
- `StaticMeshComponent` 없는 것 41개 예시: `EDITOR_CheckCylinder`, `OverviewFocusPreviz`, `TargetingAllyPreviz` 등 에디터 전용 컴포넌트
  - `ModelPath`가 빈 문자열 → `LoadFromFile`에서 `continue`로 스킵 (무해)
- 245 StaticMeshActor ↔ 245 StaticMeshComponent(메시 있음) 1:1 대응 확인 → 누락 없음

**결론: 메인 JSON에서 필요한 StaticMesh 데이터는 빠짐없이 처리되고 있음**

---

## 3. _Generated_ 폴더 (World Partition 스트리밍 셀) 미처리 문제

### 배경

UE5 World Partition은 레벨을 공간 단위로 분할하여 스트리밍 셀 파일을 생성한다.
- 메인 파일: `SmallLevel_SimonArea.json` → 현재 처리 중
- 추가 파일: `SmallLevel_SimonArea/_Generated_/*.json` (약 35개) → **현재 미처리**

이 파일들을 임포트하지 않아 **맵이 비어 보이는 현상** 발생.

### _Generated_ 파일 구조

`StaticMeshActor` 데이터가 2단계 구조로 나뉨 (메인 JSON과 다름):

```
Step 1: StaticMeshActor → 컴포넌트 참조
{
  "Type": "StaticMeshActor",
  "Name": "StaticMeshActor_XXX",
  "Properties": {
    "StaticMeshComponent": { "ObjectName": "StaticMeshComponent0'...'" }
  }
}

Step 2: StaticMeshComponent0 → 실제 메시 + 트랜스폼
{
  "Type": "StaticMeshComponent",
  "Name": "StaticMeshComponent0",
  "Outer": { "ObjectName": "StaticMeshActor'...'" },
  "Properties": {
    "StaticMesh": { "ObjectPath": "/Game/Meshes/.../FR_1.2" },
    "RelativeLocation": { "X": 15511.783, "Y": -717.7485, "Z": 88.54772 },
    "RelativeRotation": { ... },
    "RelativeScale3D": { ... }
  }
}
```

메인 JSON의 `StaticMeshComponent`는 독립적으로 트랜스폼을 가지지만,  
`_Generated_` 파일의 `StaticMeshComponent`는 `StaticMeshActor`의 자식으로 `Outer`를 통해 연결된다.

### _Generated_ 파일에서 발견된 타입별 처리 방침

| Type | 처리 방침 |
|------|-----------|
| `StaticMeshActor` | ✅ **임포트 대상** — 2단계 조회로 메시 + 트랜스폼 추출 |
| `SF_GeneratedInvisibleWallBoxComponent` | ❌ 스킵 — 보이지 않는 벽, 시각적 오브젝트 아님 |
| `BP_WallCorner_C` | ❌ 스킵 — 블루프린트 액터 |
| `AmbientSound` | ❌ 스킵 |
| `Emitter` / `ParticleSystemComponent` | ❌ 스킵 |

---

## 4. scale_map.py 현황 및 필요한 업데이트

### 현재 처리 흐름

```
OriginJson/<filename>.json
  └─ StaticMeshComponent       → parse_single()
  └─ InstancedStaticMeshComponent → parse_instanced() × N
→ SaveJson/<filename>.json
```

### 필요한 업데이트: _Generated_ 폴더 지원

```
OriginJson/<filename>.json           → 기존 처리 (유지)
OriginJson/<mapname>/_Generated_/*.json → 추가 처리 필요
  └─ StaticMeshActor 목록 수집
  └─ 각 Actor에 대해 Outer 매칭으로 StaticMeshComponent 조회 (2단계 조회)
  └─ RelativeLocation / RelativeRotation / RelativeScale3D + StaticMesh 경로 추출
→ 기존 StaticMeshes 배열에 병합 후 SaveJson 저장
```

### 좌표 변환

_Generated_ 파일의 `StaticMeshComponent`는 기존 `parse_single()`과 동일한 변환 적용:
- `RelativeLocation` → `convert_translation(x, y, z, factor)`
- `RelativeRotation` (Pitch/Yaw/Roll) → `euler_to_quat()`
- `RelativeScale3D` → `convert_scale(x, y, z, factor)`

---

## 5. SaveJson ↔ LoadFromFile 필드 매핑 검증

`scale_map.py` 출력 vs `SceneContext::LoadFromFile` 읽기 필드 비교:

| 필드 | scale_map.py 출력 | LoadFromFile 읽기 | 일치 여부 |
|------|-------------------|-------------------|-----------|
| `Id` | ✅ | `obj->_name` | ✅ |
| `ModelPath` | ✅ (파일명만) | `_modelFolder + "/" + ModelPath`로 전체 경로 구성 | ✅ |
| `Position` | ✅ [x, y, z] | `obj->_position` | ✅ |
| `Rotation` | ✅ [x, y, z, w] | `obj->_rotation` | ✅ |
| `Scale` | ✅ [x, y, z] | `obj->_scale` | ✅ |

**결론: 필드 매핑 완전 일치. 포맷 이슈 없음.**

---

## 6. 미해결 / 향후 작업

| 항목 | 우선순위 | 내용 |
|------|----------|------|
| `_Generated_` 폴더 파싱 추가 | 높음 | `scale_map.py` 업데이트, 2단계 StaticMeshActor 조회 구현 |
| 조명 임포트 | 낮음 | SpotLight 29개, PointLight 3개, DirectionalLight 4개 미처리 |
| `_isInstanced` 저장/복원 | 낮음 | 재로드 후 렌더 핸들 생성 허용으로 현재 허용 가능 |
