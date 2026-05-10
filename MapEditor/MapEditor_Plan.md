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
    ├── PanelHierarchy    ✅ 오브젝트 목록, 선택/삭제/복제/이름변경 (Add 버튼 제거 — Prefab으로 대체)
    ├── PanelInspector    ✅ Transform 실시간 편집, Gizmo 양방향 연동, Undo 기록
    └── PanelAssetBrowser ✅ 파일 탐색기 방식, 브레드크럼, .bin/.prefab 드래그 앤 드롭 스폰
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
├── _undoStack    (vector<Shared<IEditorCommand>>)  — 최대 50개
├── _redoStack    (vector<Shared<IEditorCommand>>)
├── SaveToFile(const string& path)
├── LoadFromFile(const string& path)               — 로드 시 Undo/Redo 스택 초기화
├── SpawnObject(const wstring& modelPath, const Vec3& pos) → Shared<EditorObject>
├── SpawnFromPrefab(const string& prefabPath, const Vec3& pos) → Shared<EditorObject>
├── PushCommand(Shared<IEditorCommand> cmd)        — Execute 호출 후 undoStack push, redoStack clear
├── UndoCommand()
└── RedoCommand()
```

### Command 시스템 ✅

```
IEditorCommand (abstract)          — Execute / Undo 인터페이스
├── CmdAddObject                   — obj 추가 (Execute: Add+선택, Undo: Remove)
├── CmdDeleteObject                — obj 삭제 (Execute: Remove, Undo: 원래 인덱스에 재삽입+선택)
└── CmdTransformObject             — Transform 변경 (old/new pos·rot·scale 보관, 양방향 복원)
```

> Prefab/메시 스폰, Inspector Delete, Hierarchy Delete, Gizmo/Inspector Transform 편집 모두 Command로 기록.  
> Ctrl+Z / Ctrl+Y 로 Undo/Redo. `WantTextInput` 체크로 텍스트 입력 중 오작동 방지.

---

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
├── Render(const Matrix& worldMatrix)     — G-buffer 출력 (pass 0: 텍스처, pass 1: 회색)
├── RenderID(const Matrix& worldMatrix, uint32 id) — ID 버퍼용 (pass 2)
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
    - ✅ Deferred 라이팅 전환
      - G-buffer MRT 패스 → 라이트 누적 패스 → CombinedSimple 컴바인 패스
      - `Shader_VtxMesh.hlsl`: G-buffer 출력 (Diffuse/Normal/Depth, 3 targets)
      - `Shader_Deferred.hlsl`: CombinedSimple pass (index 4) 추가 — 섀도우 없는 단순 합성
      - `SyncSceneLights()`: 매 프레임 GAME Light_Manager를 SceneContext 기준으로 재동기화
13. ✅ 라이트 배치 및 속성 편집
    - Directional / Point Light 지원, Inspector 실시간 수치 조정
    - 기본 Directional Light를 EditorObject로 관리 (Hierarchy 표시, 삭제 가능)
    - `EditorObject._lightType` / `_lightRange` / `_lightDiffuse` 등 파라미터 편집
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

### Phase 3.8 — Asset Browser 파일 탐색기 + 드래그 앤 드롭 스폰 ✅

#### 목표
- Asset Browser를 Unity Project 창처럼 파일 탐색기 방식으로 재설계
- `.bin` 모델 파일을 뷰포트로 드래그 앤 드롭하면 3D 커서 위치에 오브젝트 스폰

#### 구현 내용

1. **`PanelAssetBrowser` 재설계** ✅
    - 탭 기반 → 파일 탐색기 방식으로 전환 (`Initialize()` 분리, `Create`에서 호출)
    - 루트: `Client/Bin/Resources/Map` (`_mapSavePath` → `fs::canonical`로 절대 경로 변환)
    - 브레드크럼 바: `Map > SaveJson` 형태, 클릭 시 해당 경로로 점프 (루프 내 `NavigateTo` 후 `break`)
    - 폴더 더블클릭 → 내부 탐색 (`_dirs` / `_files` 분리 스캔)
    - `.json` 더블클릭 → `_ctx->LoadFromFile()` (기존 동작 유지)
    - `.bin` / `.model` → `ImGui::BeginDragDropSource` 드래그 소스 (payload = 절대 경로)

2. **`PanelViewport` 수정** ✅
    - `BeginDragDropTarget` → `ConsumeSpawn` 패턴 (ConsumePick과 동일 구조)
    - Shift+좌클릭 → `ConsumeCursorSet` (기존 좌클릭 피킹과 분기)

3. **`MapEditorApp` 수정 — 3D 커서 + 스폰** ✅
    - `SceneContext`에 `_cursorPos` / `_cursorVisible` 추가 (뷰포트 오버레이 공유)
    - `SetCursorAt()`: GPU ID 버퍼로 히트 오브젝트 식별 → 레이 × 해당 오브젝트 AABB 교차로 정확한 표면 위치 계산 (O(1))
    - `DoSpawnAt()`: `_scene._cursorPos` 위치에 `SceneContext::SpawnObject` 호출
    - 빈 공간 Shift+클릭 → 카메라 시선 방향 10 단위 앞 폴백

4. **`SceneContext` 수정** ✅
    - `SpawnObject(const wstring& modelPath, const Vec3& pos)` 추가
    - `EditorObject` 생성 (StaticMesh, 파일명을 이름으로) + `_dirtyRenderHandles = true` → `UpdateRenderHandles`가 다음 프레임에 렌더 핸들 연결

5. **뷰포트 커서 오버레이** ✅
    - `Image` 아래 좌하단 `##CursorOverlay` 윈도우 — 보라색 `Cursor X.XX Y.XX Z.XX` 표시

#### 설계 결정
- **3D 커서 방식 채택**: Blender 스타일, 지면 높이 무관, Shift+클릭으로 위치 지정 후 드롭
- **Ray × Y=0 기각**: 경사 지형에서 부정확
- **CPU AABB 전체 순회 기각**: 오브젝트 수 많을 때 O(n) 성능 문제
- **GPU ID + 단일 AABB 교차 채택**: ID 버퍼로 히트 오브젝트 특정 후 해당 1개만 레이캐스트 → O(1)
- **PhysX 레이캐스트 기각**: Phase 6 이전 콜라이더 미구축
- **동시 실행 충돌 없음**: 피킹(좌클릭)과 드롭은 물리적으로 동시에 발생 불가, 사용 RT도 분리

---

### Phase 3.9 — 편집 UX 개선 ✅

#### 목표
- 오브젝트 배치 흐름을 Prefab 드래그 앤 드롭으로 통일하고, 실수 복구 기능 추가

#### 구현 내용

1. **오브젝트 추가 시 자동 선택** ✅
   - `SpawnObject` / `SpawnFromPrefab` → `Shared<EditorObject>` 반환
   - `DoSpawnAt`에서 `PushCommand(CmdAddObject)` → Execute 내에서 자동 선택
   - Hierarchy `_prevSelected` 비교로 스크롤 자동 동기화

2. **Prefab 시스템 (Asset Browser 통합)** ✅
   - Light / SpawnPoint / TriggerVolume 등 비메시 오브젝트를 `.prefab` JSON 파일로 저장
   - 저장 경로: `Client/Bin/Resources/Map/Prefabs/` (7종 기본 프리팹 제공)
   - `PanelAssetBrowser`: `.prefab` 드래그 소스 추가 (payload 재사용: `"MODEL_PATH"`)
   - `DoSpawnAt`: 확장자 분기 — `.prefab` → `SpawnFromPrefab`, 나머지 → `SpawnObject`
   - `SceneContext::SpawnFromPrefab`: JSON 파싱 → 타입별 파라미터 복원 → `Add`
   - Hierarchy "추가" 버튼(타입 팝업) 제거, `_types` 배열 제거
   - 버그 수정: `DoSpawnAt`에서 `SpawnObject`/`SpawnFromPrefab` 내부 `Add` 후 `Remove` → `PushCommand` 순서로 이중 스폰 방지

3. **Ctrl+Z / Ctrl+Y Undo/Redo** ✅
   - `IEditorCommand` (abstract class) — `Execute` / `Undo` 인터페이스
   - `CmdAddObject` / `CmdDeleteObject` / `CmdTransformObject` — Initialize/Create 패턴
   - `SceneContext`에 `_undoStack` / `_redoStack` (vector, 최대 50개)
   - `PushCommand`: Execute 호출 → undoStack push → redoStack clear
   - `MapEditorApp::Update`: `WantTextInput` 체크 후 Ctrl+Z → `UndoCommand`, Ctrl+Y → `RedoCommand`
   - Inspector Transform: DragFloat3 `IsItemActive` 추적, 드래그 시작 시 before 스냅샷, 드래그 종료 시 `CmdTransformObject` push
     - `frameStartPos/Rot/Scale`로 첫 프레임 수정 전 값 캡처
   - Gizmo: `IsUsing()` 전환 감지, 드래그 시작 시 before 캡처, 드래그 종료 시 `CmdTransformObject` push (`_gizmoWasUsing` 업데이트 필수)
   - Inspector/Hierarchy Delete → `CmdDeleteObject` (삭제 전 인덱스 보존)
   - `LoadFromFile` 시 Undo/Redo 스택 초기화
   - `Free()` 시 스택 clear → D3D11 리소스 누수 방지
   - 버그 수정: `Free()`에서 `_deferredShader` / `_rectBuffer` 누락으로 인한 `LIVE_BLENDSTATE` / `LIVE_BUFFER` 해결

---

### Phase 4 — MainEditor 레벨 구성

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
| Asset Browser 파일 탐색기 | ✅ |
| 3D 커서 스폰 (Shift+클릭 + 드래그 앤 드롭) | ✅ |
| 추가 시 자동 선택 | ✅ |
| Prefab 시스템 (.prefab 드래그 앤 드롭) | ✅ |
| Undo / Redo (Ctrl+Z / Ctrl+Y) | ✅ |

### 렌더링 / 배치

| 기능 | 상태 |
|------|------|
| 정적 메시 배치 | ✅ |
| 라이트 배치 (Directional / Point) | ✅ |
| Deferred 라이팅 (G-buffer + 라이트 누적 + 합성) | ✅ |
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
