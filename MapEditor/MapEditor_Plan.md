# MapEditor 기획 문서

## 개요

Unity 에디터를 참고하여 맵 관련 기능을 전담하는 독립 에디터.  
Clair Obscur: Expedition 33 모작 기준으로 필요한 맵 데이터를 편집하고,  
완성된 데이터는 JSON으로 직렬화하여 MainEditor에서 로드해 사용한다.

---

## 팀 구성 (8인)

| 담당자 | 역할 | 맵 에디터와의 접점 |
|--------|------|-------------------|
| mainFramework | 엔진 기반, GameInstance, 렌더링 파이프라인 | 엔진 API 전반 사용, 로드 파이프라인 포맷 협의 |
| 맵 (본인) | 맵 에디터, 맵 데이터 직렬화 | — |
| 이펙트 | 파티클 및 VFX 시스템 구현 | 맵에 이펙트 배치 시 EffectId 참조, 파라미터 편집 |
| 셰이더 | 셰이더 기본 구현 (Water, Fog, Post-processing 등) | 셰이더 파라미터를 구역별로 수치 조정하는 UI만 담당 |
| 플레이어 | 플레이어 캐릭터 시스템 | 플레이어 스폰 포인트 위치 지정 |
| 몬스터 & AI | 몬스터 시스템, AI 행동 | 인카운터 존, 적 스폰 포인트, AI 웨이포인트/순찰 경로 |
| UI | UI 시스템 | 특정 구역 진입 시 UI 트리거 존 설치 |
| 카메라 | 카메라 시스템 기본 구현 | 카메라 존 파라미터(FOV, 고정 앵글, 전환 방식) 편집만 담당 |

> 맵 에디터는 각 시스템을 직접 구현하지 않고, **각 팀이 정의한 ID와 파라미터를 맵 위에 배치하고 JSON으로 직렬화**하는 것이 역할.  
> 따라서 EffectId, EnemyGroupId, 셰이더 파라미터 키 등은 각 담당자와 **데이터 포맷 협의** 필요.

---

## 아키텍처 원칙

- **완전 독립 프로젝트**: MainEditor와 코드 공유 없음
- **데이터만 공유**: JSON 포맷으로 MainEditor에 맵 데이터 전달
- **엔진 기반**: `Engine_Define.h`, `GameInstance`, `Engine::Base` 활용
- **ImGui 기반 UI**: Unity처럼 실시간 수치 조정 및 씬 확인
- **진입점**: `MapEditor/Default/MapEditor.cpp`
- **앱 중심**: `MapEditor/Private/MapEditorApp.cpp`
- **경로 기준**: VS 실행 시 작업 디렉토리 = `MapEditor/Default/`, 리소스 경로는 `../../` 기준

---

## 클래스 구조

```
MapEditorApp          — 엔진 초기화, 메인 루프 (Update/Render), SceneContext 소유
├── MEImGuiManager    — ImGui 생명주기 (NewFrame / Render / Shutdown)
├── MECamera          — 에디터 자유시점 카메라 (우클릭+WASD, 뷰/프로젝션 행렬 → GAME)
├── RenderTarget      — 씬을 RT에 렌더, PanelViewport에 SRV 전달
├── SceneContext      — 씬 오브젝트 목록 + 선택 상태 + JSON 저장/로드
└── MEManager         — DockSpace + 패널 컨테이너 + File 메뉴
    ├── PanelViewport     ✅ RT ImGui::Image, 카메라 입력, ImGuizmo 오버레이
    ├── PanelHierarchy    ✅ 오브젝트 목록, 선택/추가/삭제/복제/이름변경
    ├── PanelInspector    ✅ Transform 실시간 편집, Gizmo 양방향 연동
    └── PanelAssetBrowser ✅ 폴더 탭(SaveJson) + 더블클릭 로드
```

**베이스 클래스**: `MEWindow` (이름, 열림 상태, Update/Render 인터페이스)

### EditorObject ✅

```
EditorObject
├── name          (wstring)   — Hierarchy 표시 이름
├── modelPath     (wstring)   — 모델 경로 (JSON 저장 시 사용)
├── position      (Vec3)
├── rotation      (Quat)      — 쿼터니언 저장, Inspector는 오일러(도) 변환 표시
├── scale         (Vec3)
├── worldMatrix   (Matrix)    — Gizmo / 렌더링에 사용
└── renderHandle  (Shared<MEStaticMeshActor>) — 뷰포트 시각화 전용, JSON 직렬화 대상 아님
```

### SceneContext ✅

```
SceneContext
├── objects       (vector<Shared<EditorObject>>)
├── selected      (Shared<EditorObject>)
├── mapName       (wstring)
├── SaveToFile(const string& path)
└── LoadFromFile(const string& path)
```

### scale_map.py ✅ (외부 변환 도구)

```
Client/Bin/Resources/Map/scale_map.py
└── OriginJson/*.json (UE5 FModel 포맷) 읽기 → SaveJson/*.json (MapEditor 포맷) 저장
    — StaticMeshComponent / InstancedStaticMeshComponent 파싱
    — 좌표계 변환 (UE5 → DX): Translation /100 + factor 보정, Y↔Z 스왑
    — Scale은 UE 원본값 유지 (factor 미적용)
    — --factor 옵션으로 맵 배율 조정 (기본 0.001, 코드 수정 불필요)
    — OriginJson 원본 파일 수정 없음
```

> `scale_map.bat` 으로 실행. MEUe5Importer(C++)는 에디터에서 제거됨.

### MEStaticMeshActor ✅

```
MEStaticMeshActor                         — 에디터 뷰포트 시각화 전용
├── _model   (Shared<ModelCom>)
├── _shader  (Shared<ShaderCom>)
├── Render(const Matrix& worldMatrix)
│   — 텍스처 있으면 pass 0 (DefaultPass), 없으면 pass 1 (NoTexturePass, 회색)
│   — GAME->Get_LightDesc(0)로 Directional Light 바인딩 (Forward 라이팅)
└── GetModel() — 컬링용 AABB 접근에 사용
```

### MEModelManager ✅

```
MEModelManager                            — 모델 로드/캐싱 전담
├── map<wstring, Shared<ModelCom>> _cache
├── GetOrLoad(device, context, path)      — 캐시 조회 or 로드 후 Clone 반환
├── Unload()                              — 씬 전체 초기화 시 캐시 clear
└── ~MEModelManager()                     — Unload() 호출
```

> 오브젝트 1개 삭제 시에는 `EditorObject::_renderHandle.reset()`만 하면 됨.  
> `Unload()`는 New Map / 다른 맵 로드 시 전체 캐시 비울 때 사용.

---

## 개발 단계 (Phase)

### Phase 1 — 에디터 기반 ✅
1. ✅ `MapEditor.cpp` 진입점 (엔진 초기화, 60FPS 루프, ImGui WndProc)
2. ✅ `MapEditorApp` (`Engine::Base` 상속, Initialize/Update/Render/Free)
3. ✅ ImGui 도킹 레이아웃 (`MEImGuiManager`, `MEManager`, DockBuilder)
4. ✅ 기본 패널 4종 — Viewport / Hierarchy / Inspector / Asset Browser
5. ✅ 에디터 카메라 + RT 렌더링 (`MECamera`, `RenderTarget` → ImGui::Image)
6. ✅ 기즈모 Translate/Rotate/Scale — ImGuizmo, W/E/R 단축키
    - ✅ Snapping — Ctrl 홀드 활성, `PanelSnap`으로 단위 편집 (Translate/Rotate/Scale)

### Phase 2 — 핵심 오브젝트 편집 ✅
7. ✅ `EditorObject` / `SceneContext`
8. ✅ Hierarchy 패널 (목록, 선택/추가/삭제/복제/이름변경, Gizmo 연동)
9. ✅ Inspector 패널 (Transform 편집, Gizmo 양방향 연동)
10. ✅ JSON 저장/로드 + PanelAssetBrowser + File 메뉴 다이얼로그
11. ✅ UE5 FModel JSON 변환 (`scale_map.py` 외부 도구)
    - InstancedStaticMeshComponent / StaticMeshComponent 파싱
    - UE5 → DX 좌표계 변환, Position에만 배율 보정 (--factor)
    - OriginJson → SaveJson 변환 후 에디터에서 로드
    - `MEUe5Importer`(C++) 및 Origin Maps 탭은 에디터에서 제거
12. ✅ 정적 메시 뷰포트 렌더링
    - ✅ `MEStaticMeshActor` (Model + Shader_VtxMesh 직접 렌더)
    - ✅ `EditorObject.renderHandle` 연결
    - ✅ `MapEditorApp` RT 내 씬 렌더 루프
    - ✅ 버그 수정: `Free()` 리소스 해제 순서 (`LIVE_BLENDSTATE` / `LIVE_BUFFER`)
    - ✅ 버그 수정: 이름 없는 모델이 `.model` 경로로 로드 시도하던 문제
    - ✅ 버그 수정: `InstancedStaticMeshComponent` 키 대소문자 불일치 (`perInstanceSMData` → `PerInstanceSMData`)
    - ✅ `MEModelManager` — 동일 모델 중복 로드 방지 (캐싱 + Clone 재사용)
    - ✅ `EditorObject._isInstanced` — Instanced 오브젝트는 렌더 핸들 생성 스킵
    - ✅ 모델 버텍스 스케일 보정: `preLocalTransformMatrix = Matrix::CreateScale(0.001f)` (UE5 cm → 엔진 단위)
    - ✅ 텍스처 없는 모델 회색 렌더링 — `NoTexturePass` (pass 1) 추가
    - ✅ Forward 라이팅 — `Shader_VtxMesh` Forward 방식으로 교체 (Lambert + Specular + Ambient, 단일 RT 출력), `MapEditorApp::Initialize()`에서 기본 Directional Light 등록, `MEStaticMeshActor::Render()`에서 `GAME->Get_LightDesc(0)` 바인딩
13. ✅ 라이트 배치 및 속성 편집
14. ✅ Inspector Focus 버튼 — 선택 오브젝트 위치로 카메라 즉시 이동 (`MECamera::FocusOn`)

### Phase 3 — 볼륨 / 존 시스템 ✅
15. ✅ 타입 시스템 도입 (`EObjectType`, `EZoneShape`) — `MapEditorDefines.h`
    - magic_enum 활용, `ObjectTypeLabel` / `ObjectTypeName` / `ObjectTypeFromName` 헬퍼
16. ✅ `EditorObject` 타입별 파라미터 필드 추가
    - Zone Shape (Box/Sphere), EncounterZone, TriggerVolume, CameraZone, PostProcessVolume 파라미터
17. ✅ `SceneContext` Save/Load 멀티타입 지원
    - JSON 배열 분리: `StaticMeshes` / `SpawnPoints` / `EncounterZones` / `TriggerVolumes` / `CameraZones` / `PostProcessVolumes`
18. ✅ `PanelHierarchy` 타입 선택 팝업 + `[SM]`/`[SP]` 등 타입 라벨 표시
19. ✅ `PanelInspector` 타입별 커스텀 UI (Zone Shape / 타입 파라미터 섹션)
20. ✅ 뷰포트 디버그 드로우 — 존/볼륨 와이어프레임 박스·구 시각화

### Phase 3.5 — 엔진 확장 ✅
- ✅ **PhysX 5.5.0 통합** (`PhysX_Manager`, `GameInstance` 등록)
  - PhysX 초기화 / 씬 생성 / Simulate / FetchResults 루프
  - `GameInstance`에서 `Get_PhysXPhysics()` / `Get_PhysXScene()` 접근 가능
  - 콜라이더 컴포넌트 연동 → 다른 파트 담당
- ✅ **절두체 컬링 (Frustum Culling)** (`Culling_Manager`)
  - `UpdateFrustum(view, proj)` — `BoundingFrustum::CreateFromMatrix` + `XMMatrixInverse` 기반
  - `IsVisible(worldAABB)` — `BoundingFrustum::Intersects` 판정
  - `GameInstance`에 등록, `ModelCom::Get_PreLocalTransformMatrix()` getter 추가
  - `MapStaticMesh::Late_Update()` — `preLocalTransformMatrix * worldMatrix`로 worldAABB 구성 후 컬링
  - 버그: `FreeCamera` / `TargetCamera` 둘 다 UpdateFrustum 호출 → 절두체 덮어쓰기 → 각자 호출로 분리
  - MapEditor: `MECamera::Apply()`에 UpdateFrustum 추가, `DrawSceneDebug()` StaticMesh 케이스에 IsVisible 체크

### Phase 3.6 — 뷰포트 마우스 피킹 + Inspector Delete ✅
#### 설계 결정
- **레이캐스트 순회 방식 기각**: 오브젝트 수 많을 때 O(n) 성능 문제 확인
- **GPU Object ID Buffer 방식 채택**: 클릭 시점에만 렌더 1회, O(1) 조회, 픽셀 정확도

#### 구현 내용
- ✅ **GPU ID Buffer 피킹** (`MapEditorApp` + `Shader_VtxMesh.hlsl` + `MEStaticMeshActor`)
    - `Shader_VtxMesh.hlsl`에 `g_iObjectID` 변수 + `PS_MAIN_ID` + `IDPass` (pass index 2) 추가
    - `MEStaticMeshActor::RenderID(worldMatrix, id)` — pass 2로 ID 색상 렌더
    - `MapEditorApp`에 `_idRT` (RGBA8, 뷰포트와 동일 크기) + `_idStagingTex` (1×1 Staging) 추가
    - `DoPickAt()`: ID RT 렌더 → `CopySubresourceRegion` 1픽셀 → Map/Unmap → ID 디코딩 → `_selected` 갱신
    - **ID 인코딩**: index+1 (1-based, 0=배경), R + G×256 + B×65536
    - **빈 공간 클릭** → ID=0 → `_selected = nullptr` (선택 해제)
    - **StaticMesh만 피킹**: Zone/SpawnPoint/Light는 메시 없어 제외 → 추후 콜라이더 편집 시 추가 예정
    - > **TODO**: 선택 오브젝트를 마우스 피킹 위치로 이동 — 깊이 버퍼 월드 포지션 복원 방식, 별도 구현 예정
- ✅ **뷰포트 좌클릭 입력 감지** (`PanelViewport`)
    - `ImGui::IsItemClicked(Left)` + `!ImGuizmo::IsOver()` + `!ImGuizmo::IsUsing()` 조건
    - `ConsumePick(outMousePos, outVpPos, outVpSize)` — 소비 후 플래그 해제
- ✅ **Inspector Delete 버튼** — `_ctx->Remove(_ctx->_selected)` 호출

### Phase 3.7 — JSON ModelFolder 분리 ✅

- `SceneContext`에 `_modelFolder` 필드 추가, SaveToFile/LoadFromFile에서 `ModelFolder + "/" + ModelPath`로 경로 구성
- `scale_map.py` — `ModelFolder` 필드 출력, `ModelPath`를 파일명만 저장

---

### Phase 4 — MainEditor 레벨 구성 ← **다음 작업**

#### 목표
- Client에서 실제 플레이 가능한 레벨 구조 잡기
- MapEditor에서 저장한 JSON을 MainEditor/Client에서 로드하는 파이프라인 완성

#### 작업 내용
1. **레벨 추가**
    - `Level_GamePlay` 외 `Level_TestMap` (기능 테스트용) + 실제 스테이지 레벨 몇 개 추가
    - 레벨별 리소스 로딩 (`Loader` 연동) 구조 정리

2. **맵 JSON 로드 파이프라인**
    - `MapLoader` — MapEditor JSON 포맷 파싱, `MapStaticMesh` / 존 오브젝트 스폰
    - 현재 `MapStaticMesh`는 있으나 레벨 진입 시 자동 로드 흐름 미완성
    - 로드 시 `ModelCom` 공유(`sharedModel`) 활용으로 중복 로드 방지
    - SpawnPoint / EncounterZone / TriggerVolume 등 존 오브젝트도 스폰 대상에 포함

3. **레벨 전환**
    - 포털/트리거 존 진입 시 다음 레벨로 전환하는 기본 흐름 구현

> Phase 3.7 (ModelFolder 분리)과 동시에 진행 시 Client 로드 코드를 `ModelFolder + "/" + ModelPath` 포맷으로 맞춰야 함.

---

### Phase 5 — 내비 / AI
- 웨이포인트, 포털 / 존 전환
- Recast 통합 → NavMesh 베이크 및 시각화

### Phase 6 — 물리 / 최적화
- PhysX 콜라이더 편집 UI (맵 에디터)
- PhysX 지형 충돌
- Octree 기반 공간 분할 컬링 (현재 O(n) → O(log n))
- 인스턴싱 그룹, Occlusion Volume

---

## 목표 기능 목록

### 편집 도구

| 기능 | 상태 |
|------|------|
| 뷰포트 | ✅ |
| 기즈모 (W/E/R) | ✅ |
| Hierarchy | ✅ |
| Inspector | ✅ |
| Snapping | ✅ (Ctrl 홀드, PanelSnap으로 수치 편집) |
| Focus (Inspector) | ✅ |
| 마우스 피킹 (GPU ID Buffer) | ✅ |

### 렌더링 / 배치

| 기능 | 상태 |
|------|------|
| 정적 메시 배치 | ✅ |
| 라이트 배치 | ✅ |
| 절두체 컬링 (Client/MainEditor) | ✅ |
| 절두체 컬링 (MapEditor 뷰포트) | ✅ |
| Octree 공간 분할 컬링 | 구현 예정 (Phase 6) |
| 스카이 (SkySphere) | 구현 예정 — PNG 파노라마 텍스처, VIBuffer_Sphere 필요 |
| 물(Water) 배치 | 구현 예정 |
| 파티클 이펙트 배치 | 파티클 시스템 대기 |
| 지형 (Terrain) | 방식 미확정 |

### 데이터 입출력

| 기능 | 상태 |
|------|------|
| JSON 저장 | ✅ |
| JSON 로드 | ✅ |
| UE5 FModel JSON 변환 (scale_map.py) | ✅ |
| ModelFolder 분리 (저장/로드) | ✅ |
| MainEditor 맵 JSON 로드 파이프라인 | 구현 예정 (Phase 4) |
| 레벨 추가 및 전환 | 구현 예정 (Phase 4) |

### 볼륨 / 존

| 기능 | 상태 |
|------|------|
| 트리거 볼륨 | ✅ (편집·직렬화) |
| 인카운터 존 | ✅ (편집·직렬화) |
| Post-processing Volume | ✅ (편집·직렬화) |
| Volumetric Fog 존 | ✅ (PostProcess에 통합) |
| Camera Zone | ✅ (편집·직렬화) |
| 스폰 포인트 | ✅ (편집·직렬화) |
| 볼륨 뷰포트 시각화 | ✅ |
| Ambient Sound Zone | 구현 예정 |
| 포털 / 존 전환 | 구현 예정 (Phase 4) |

### 물리 / 내비게이션

| 기능 | 상태 |
|------|------|
| 콜라이더 편집 | 구현 예정 (Phase 6) |
| PhysX 지형 충돌 | 구현 예정 (Phase 6) |
| NavMesh 베이크/시각화 | 엔진 Recast 통합 후 (Phase 5) |
| 웨이포인트 | 구현 예정 (Phase 5) |

---

## 라이브러리 현황

| 라이브러리 | 용도 | 현황 |
|------------|------|------|
| DirectX 11 | 렌더링 | 통합 완료 |
| DirectXTK | 유틸리티 | 통합 완료 |
| Assimp | 메시 임포트 | 통합 완료, 엔진 리팩토링 대기 |
| nlohmann/json | 데이터 직렬화 | 통합 완료 |
| ImGui | UI | 통합 완료 |
| PhysX 5.5.0 | 물리 충돌 | 통합 완료 (PhysX_Manager, GameInstance 등록) |
| Recast/Detour | 내비게이션 메시 | 미설치, 엔진 통합 필요 |

---

## JSON 출력 포맷

MainEditor에서 추가 파싱 없이 바로 로드 가능하도록 설계.  
Rotation은 쿼터니언 [X, Y, Z, W]으로 저장. Inspector는 오일러(도) 변환 표시.

```json
{
    "MapData": {
        "MapName": "Stage_01",
        "StaticMeshes": [
            {
                "Id": "SM_Rock_01",
                "ModelPath": "Data/Models/Rock_01.bin",
                "Position": [125.93, -123.91, -103.11],
                "Rotation": [0.0, 0.0, 0.98215955, -0.17882906],
                "Scale": [0.91263, 0.91263, 0.91263],
                "ColliderType": "Mesh"
            }
        ],
        "Lights": [],
        "PostProcessVolumes": [],
        "EncounterZones": [],
        "CameraZones": [],
        "SpawnPoints": [],
        "Waypoints": [],
        "Triggers": [],
        "NavMeshAreas": []
    }
}
```

---

## UE5 레벨 데이터 임포트 ✅

FModel로 추출한 레벨 JSON을 MapEditor 오브젝트로 자동 변환.  
파일: `Client/Bin/Resources/Map/OriginJson/`  
`scale_map.py --factor <배율>` 실행 → `SaveJson/` 에 저장 → Asset Browser에서 로드.

### 파싱 대상

| Type | 트랜스폼 위치 | 비고 |
|------|-------------|------|
| `InstancedStaticMeshComponent` | `PerInstanceSMData[].TransformData` | Quat + Translation + Scale |
| `StaticMeshComponent` | `Properties.RelativeLocation/Rotation/Scale3D` | Euler(Pitch/Yaw/Roll) |

### 좌표계 변환 (UE5 → DirectX)

```cpp
// Translation (cm → 엔진 단위, Z↑ → Y↑)
DX.x =  UE.x / 100.f;
DX.y =  UE.z / 100.f;
DX.z =  UE.y / 100.f;

// Quaternion (축 스왑 + W 반전)
DX_Quat = Quat(UE.x, UE.z, UE.y, -UE.w);
```

### 지형 데이터

UE5 Landscape는 FModel 레벨 JSON에 포함되지 않음 (별도 스트리밍 에셋).  
지형 처리 방식은 추후 결정.
