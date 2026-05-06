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
    └── PanelAssetBrowser ✅ 폴더 탭(SaveJson/OriginJson) + 더블클릭 로드/임포트
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

### MEUe5Importer ✅

```
MEUe5Importer
└── Import(filePath, SceneContext&, append)
    — InstancedStaticMeshComponent → PerInstanceSMData 순회
    — StaticMeshComponent → RelativeLocation/Rotation/Scale3D 파싱
    — 좌표계 변환 (UE5 → DX) 적용
    — EditorObject 생성 → SceneContext 배치
```

### MEStaticMeshActor ✅

```
MEStaticMeshActor                         — 에디터 뷰포트 시각화 전용
├── _model   (Shared<ModelCom>)
├── _shader  (Shared<ShaderCom>)
└── Render(const Matrix& worldMatrix)
    — 텍스처 있으면 pass 0 (DefaultPass), 없으면 pass 2 (NoTexturePass, 흰색)
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
11. ✅ UE5 FModel JSON 임포터 (`MEUe5Importer`)
    - InstancedStaticMeshComponent / StaticMeshComponent 파싱
    - UE5 → DX 좌표계 변환
    - Origin Maps 탭 더블클릭으로 씬 배치
12. ✅ 정적 메시 뷰포트 렌더링
    - ✅ `MEStaticMeshActor` (Model + Shader_VtxMesh 직접 렌더)
    - ✅ `EditorObject.renderHandle` 연결
    - ✅ `MapEditorApp` RT 내 씬 렌더 루프
    - ✅ 버그 수정: `Free()` 리소스 해제 순서 (`LIVE_BLENDSTATE` / `LIVE_BUFFER`)
    - ✅ 버그 수정: 이름 없는 모델이 `.model` 경로로 로드 시도하던 문제
    - ✅ 버그 수정: `InstancedStaticMeshComponent` 키 대소문자 불일치 (`perInstanceSMData` → `PerInstanceSMData`)
    - ✅ `MEModelManager` — 동일 모델 중복 로드 방지 (캐싱 + Clone 재사용)
    - ✅ `EditorObject._isInstanced` — Instanced 오브젝트는 렌더 핸들 생성 스킵
    - ✅ 모델 버텍스 스케일 보정: `preLocalTransformMatrix = Matrix::CreateScale(0.01f)` (UE5 cm → 엔진 단위)
    - ✅ 텍스처 없는 모델 흰색 렌더링 — `NoTexturePass` (pass 2) 추가
13. ✅ 라이트 배치 및 속성 편집
14. ✅ Inspector Focus 버튼 — 선택 오브젝트 위치로 카메라 즉시 이동 (`MECamera::FocusOn`)

### Phase 3 — 볼륨 / 존 시스템 ← **진행 중**
14. ✅ 타입 시스템 도입 (`EObjectType`, `EZoneShape`) — `MapEditorDefines.h`
    - magic_enum 활용, `ObjectTypeLabel` / `ObjectTypeName` / `ObjectTypeFromName` 헬퍼
15. ✅ `EditorObject` 타입별 파라미터 필드 추가
    - Zone Shape (Box/Sphere), EncounterZone, TriggerVolume, CameraZone, PostProcessVolume 파라미터
16. ✅ `SceneContext` Save/Load 멀티타입 지원
    - JSON 배열 분리: `StaticMeshes` / `SpawnPoints` / `EncounterZones` / `TriggerVolumes` / `CameraZones` / `PostProcessVolumes`
17. ✅ `PanelHierarchy` 타입 선택 팝업 + `[SM]`/`[SP]` 등 타입 라벨 표시
18. ✅ `PanelInspector` 타입별 커스텀 UI (Zone Shape / 타입 파라미터 섹션)
19. ✅ 뷰포트 디버그 드로우 — 존/볼륨 와이어프레임 박스·구 시각화
20. Ambient Sound Zone
21. Water 배치, 파티클 배치 (파티클 시스템 완료 후)
22. 지형 시스템 (방식 미확정)

### Phase 3.5 — PhysX 엔진 통합 ✅
- ✅ `Engine` 측에 `PhysX_Manager` 생성
  - PhysX 5.5.0 초기화 / 씬 생성 / Simulate / FetchResults 루프
  - `PxFoundation`, `PxPhysics`, `PxScene`, `PxDefaultCpuDispatcher` 관리
  - `GameInstance`에서 `Get_PhysXPhysics()` / `Get_PhysXScene()` 접근 가능
- 콜라이더 컴포넌트 연동 → 다른 파트 담당
- 맵 에디터에서 콜라이더 편집 UI는 Phase 5에서 구현

### Phase 4 — 내비 / AI
20. 웨이포인트, 포털 / 존 전환
21. Recast 통합 → NavMesh 베이크 및 시각화

### Phase 5 — 물리 / 최적화
22. PhysX 통합 → 콜라이더 편집, 지형 충돌
23. 인스턴싱 그룹, Occlusion Volume

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

### 렌더링 / 배치

| 기능 | 상태 |
|------|------|
| 정적 메시 배치 | ✅ |
| 라이트 배치 | ✅ |
| 스카이 (SkySphere) | 구현 예정 — PNG 파노라마 텍스처 사용, VIBuffer_Sphere 구현 필요 |
| 물(Water) 배치 | 구현 예정 |
| 파티클 이펙트 배치 | 파티클 시스템 대기 |
| 지형 (Terrain) | 방식 미확정 |

### 데이터 입출력

| 기능 | 상태 |
|------|------|
| JSON 저장 | ✅ |
| JSON 로드 | ✅ |
| UE5 FModel JSON 임포터 | ✅ |
| MainEditor 연동 | 구현 예정 |

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
| 포털 / 존 전환 | 구현 예정 |

### 물리 / 내비게이션

| 기능 | 상태 |
|------|------|
| 콜라이더 편집 | 구현 예정 |
| PhysX 지형 충돌 | 구현 예정 |
| NavMesh 베이크/시각화 | 엔진 Recast 통합 후 |
| 웨이포인트 | 구현 예정 |
| 스폰 포인트 | 구현 예정 |

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
Asset Browser → Origin Maps 탭 → 더블클릭으로 임포트.

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
지형 처리 방식은 Phase 3에서 결정.
