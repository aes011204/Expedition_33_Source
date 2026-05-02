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

## 구현된 클래스 구조 (Phase 1)

```
MapEditorApp          — 엔진 초기화, 메인 루프 (Update/Render)
├── MEImGuiManager    — ImGui 생명주기 (NewFrame / Render / Shutdown)
├── MECamera          — 에디터 자유시점 카메라 (우클릭+WASD, 뷰/프로젝션 행렬 → GAME)
├── RenderTarget      — 씬을 RT에 렌더, PanelViewport에 SRV 전달
└── MEManager         — DockSpace + 패널 컨테이너
    ├── PanelViewport     — RT를 ImGui::Image로 표시, 호버 시 카메라 입력 처리
    ├── PanelHierarchy    — 씬 오브젝트 트리
    ├── PanelInspector    — 선택 오브젝트 속성 편집
    └── PanelAssetBrowser — 맵 에셋 브라우저
```

**베이스 클래스**: `MEWindow` (이름, 열림 상태, Update/Render 인터페이스)

---

## 목표 기능 목록

### 렌더링 / 배치

| 기능 | 설명 | 상태 |
|------|------|------|
| 정적 메시 배치 | 씬에 Static Mesh 오브젝트 추가/삭제/복제 | 구현 예정 |
| 라이트 배치 | Directional / Point / Spot Light 설치 및 실시간 속성 편집 | 구현 예정 |
| 물(Water) 배치 | 수면 볼륨 설치 및 셰이더 파라미터 조정 | 구현 예정 |
| 파티클 이펙트 배치 | 주변 환경 파티클(먼지, 빛 입자 등) 배치 및 파라미터 설정 | 구현 예정 |
| 지형 (Terrain) | 하이트맵 기반(`VIBuffer_Terrain`) 또는 정적 메시, 모작 데이터 포맷 확인 후 결정 | 방식 미확정 |

### 편집 도구

| 기능 | 설명 | 상태 |
|------|------|------|
| 기즈모 | Translate / Rotate / Scale 핸들, 실시간 수치 반영 | 구현 예정 |
| Inspector | 선택 오브젝트의 Transform 및 컴포넌트 속성 실시간 편집 | 구현 예정 |
| Hierarchy | 씬 내 오브젝트 목록 트리, 선택/삭제/이름 변경 | 구현 예정 |
| 뷰포트 | 씬 3D 뷰, 에디터 전용 카메라 이동/회전 | 구현 예정 |
| Snapping | 그리드 스냅, 각도 스냅 | 구현 예정 |

### 볼륨 / 존 시스템

| 기능 | 설명 | 상태 |
|------|------|------|
| 트리거 볼륨 | 이벤트 트리거 영역 설치 및 속성 설정 | 구현 예정 |
| 인카운터 존 | 전투 진입 트리거 구역 설치 및 적 그룹 지정 | 구현 예정 |
| Post-processing Volume | 구역별 bloom, fog density, color grading 수치 조정 (셰이더 기본 구현은 팀원) | 구현 예정 |
| Volumetric Fog 존 | 특정 구역에만 안개 효과 범위 및 농도 설정 | 구현 예정 |
| Camera Zone | 구역 진입 시 카메라 FOV, 고정 앵글, 전환 방식 파라미터 편집 (카메라 기본 구현은 팀원) | 구현 예정 |
| Ambient Sound Zone | 구역별 배경음/효과음 범위 및 음량 설정 | 구현 예정 |
| 포털 / 존 전환 | 맵과 맵을 연결하는 진입점 설치 | 구현 예정 |

### 물리 / 충돌

| 기능 | 설명 | 상태 |
|------|------|------|
| 콜라이더 편집 | Box / Sphere / Capsule / Mesh Collider 시각적 편집 | 구현 예정 |
| PhysX 지형 충돌 | PhysX HeightField 기반 지형 충돌체 생성 | **엔진 PhysX 통합 후 작업** |

### 내비게이션

| 기능 | 설명 | 상태 |
|------|------|------|
| NavMesh 베이크 | Recast 기반, 원하는 영역에만 NavMesh 생성 | **엔진 Recast 통합 후 작업** |
| NavMesh 시각화 | 생성된 NavMesh 오버레이 렌더링 | **엔진 Recast 통합 후 작업** |
| NavMesh 수치 조정 | Agent Radius / Height / Step Height 등 실시간 조정 | **엔진 Recast 통합 후 작업** |
| 웨이포인트 | AI 순찰 경로 및 이동 경로 지정 | 구현 예정 |
| 스폰 포인트 | 적 및 플레이어 시작 위치 지정 | 구현 예정 |

### 최적화 보조 도구

| 기능 | 설명 | 상태 |
|------|------|------|
| 인스턴싱 그룹 | 나무, 풀 등 대량 반복 오브젝트 그룹 배치 | 구현 예정 |
| Occlusion Volume | 시야 차단 구역 설정 (컬링용) | 구현 예정 |

### 데이터 입출력

| 기능 | 설명 | 상태 |
|------|------|------|
| JSON 저장 | 씬 전체 맵 데이터를 JSON으로 직렬화 | 구현 예정 |
| JSON 로드 | 저장된 맵 JSON 파일 불러오기 | 구현 예정 |
| MainEditor 연동 | 출력 JSON을 MainEditor 로드 파이프라인 포맷에 맞춰 출력 | 구현 예정 |
| UE JSON 임포터 | 원본 게임 레벨 데이터(UE5 추출 JSON)를 읽어 맵 에디터 오브젝트로 자동 변환 | 구현 예정 |

---

## 개발 단계 (Phase)

### Phase 1 — 에디터 기반
1. ✅ `MapEditor.cpp` 진입점 교체 (엔진 초기화, 60FPS 게임루프, ImGui WndProc, DPI 처리)
2. ✅ `MapEditorApp` 클래스 (`Engine::Base` 상속, Initialize / Update / Render / Free)
3. ✅ ImGui 초기화 및 도킹 레이아웃 (`MEImGuiManager`, `MEManager`, DockBuilder 초기 배치)
4. ✅ 기본 패널 4종 표시 — Viewport / Hierarchy / Inspector / Asset Browser
5. ✅ 에디터 전용 카메라 및 뷰포트 렌더링 (`MECamera`, `RenderTarget` RT → ImGui::Image)
6. 🔲 기즈모 (Translate / Rotate / Scale)

### Phase 2 — 핵심 오브젝트 편집
6. Hierarchy 패널
7. Inspector 패널 (Transform 실시간 편집)
8. 정적 메시 배치 / 삭제 / 복제
9. 라이트 배치 및 속성 편집
10. JSON 저장 / 로드

### Phase 3 — 볼륨 / 환경
11. 인카운터 존, 트리거 볼륨, 스폰 포인트
12. Post-processing Volume, Volumetric Fog 존
13. Camera Zone 파라미터 편집
14. Ambient Sound Zone
15. Water 배치, 파티클 배치
16. 지형 시스템 (방식 확정 후)

### Phase 4 — 내비 / AI
17. 웨이포인트, 포털 / 존 전환
18. Recast 엔진 통합 → NavMesh 베이크 및 시각화

### Phase 5 — 물리 / 최적화
19. PhysX 엔진 통합 → 콜라이더 편집, 지형 충돌
20. 인스턴싱 그룹, Occlusion Volume

---

## 라이브러리 현황

| 라이브러리 | 용도 | 현황 |
|------------|------|------|
| DirectX 11 | 렌더링 | 통합 완료 |
| DirectXTK | 유틸리티 | 통합 완료 |
| Assimp | 메시 임포트 | 통합 완료 |
| nlohmann/json | 데이터 직렬화 | 통합 완료 |
| ImGui | UI | **통합 완료** |
| PhysX 5.5.0 | 물리 충돌 / BVH | vcpkg 설치 완료, **엔진 통합 필요** |
| Recast/Detour | 내비게이션 메시 | **미설치 — 엔진 통합 필요** |

---

## JSON 출력 포맷 방향

기존 엔진의 `DT_StaticLevel.json` / `DT_GameObject.json` 포맷을 따라  
MainEditor에서 추가 파싱 없이 바로 로드 가능하도록 설계.

**Rotation은 쿼터니언 [X, Y, Z, W] 으로 저장.**  
Inspector UI에서는 오일러 각도로 변환해서 표시하고, 편집 후 다시 쿼터니언으로 저장. (Unity와 동일 방식)

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
        "Lights": [
            {
                "Type": "Directional",
                "Direction": [0.0, -1.0, 0.0],
                "Diffuse": [1.0, 1.0, 1.0, 1.0],
                "Intensity": 1.0
            }
        ],
        "PostProcessVolumes": [
            {
                "Bounds": { "Min": [-10, -10, -10], "Max": [10, 10, 10] },
                "BloomIntensity": 1.5,
                "FogDensity": 0.02,
                "ColorGrading": { "Saturation": 1.2, "Contrast": 1.1 }
            }
        ],
        "EncounterZones": [
            {
                "Id": "Encounter_01",
                "Bounds": { "Min": [0, 0, 0], "Max": [20, 5, 20] },
                "EnemyGroupId": "EnemyGroup_Forest_01"
            }
        ],
        "CameraZones": [
            {
                "Id": "CamZone_01",
                "Bounds": { "Min": [0, 0, 0], "Max": [10, 5, 10] },
                "FOV": 60.0,
                "FixedAngle": false,
                "TransitionTime": 0.5
            }
        ],
        "SpawnPoints": [],
        "Waypoints": [],
        "Triggers": [],
        "NavMeshAreas": []
    }
}
```

---

## UE5 레벨 데이터 임포트

원본 게임(Expedition 33)의 레벨 데이터를 UE5 에셋 추출 툴로 JSON으로 뽑는 데 성공.  
이 데이터를 기반으로 맵 배치를 자동 임포트하는 파이프라인을 구축한다.

### 추출된 데이터 구조

```json
{
  "Type": "StaticMeshActor",
  "ActorLabel": "SM_Rock_01",
  "TransformData": {
    "Rotation": { "X": 0.0, "Y": 0.0, "Z": 0.98215955, "W": -0.17882906 },
    "Translation": { "X": 12593.907, "Y": -10311.829, "Z": -12391.668 },
    "Scale3D": { "X": 0.91263163, "Y": 0.91263163, "Z": 0.91263163 }
  }
}
```

### 좌표계 변환 (UE5 → DirectX)

UE5와 엔진의 좌표계가 달라 변환이 필요하다.

| 항목 | UE5 | DirectX (엔진) |
|------|-----|----------------|
| 핸드니스 | 왼손 | 왼손 |
| Up 축 | Z | Y |
| Forward 축 | X | Z |
| 단위 | 센티미터 (cm) | 미터 (m) 또는 엔진 단위 |

```cpp
// Translation 변환 (cm → 엔진 단위, 축 스왑)
DX.x =  UE.x / 100.f;
DX.y =  UE.z / 100.f;   // Z → Y (Up)
DX.z =  UE.y / 100.f;   // Y → Z (Forward)

// Quaternion 변환 (축 스왑 + 핸드니스 반전)
DX_Quat.x =  UE_Quat.x;
DX_Quat.y =  UE_Quat.z;
DX_Quat.z =  UE_Quat.y;
DX_Quat.w = -UE_Quat.w;
```

### 임포터 파이프라인

```
UE JSON 파일 로드
    ↓
Actor Type 분류 (StaticMesh / Light / AmbientSound / Trigger 등)
    ↓
TransformData 파싱 + 좌표계 변환
    ↓
에셋 경로 매핑 테이블 적용 (UE "/Game/..." 경로 → 로컬 경로)
    ↓
맵 에디터 내부 오브젝트로 생성 및 씬 배치
    ↓
확인 후 내부 JSON 포맷으로 저장
```

### 에셋 경로 매핑

UE 경로를 그대로 쓸 수 없으므로 매핑 테이블이 필요하다.

```json
{
    "/Game/Meshes/Environment/Rock_01": "Data/Models/Rock_01.bin",
    "/Game/Meshes/Environment/Tree_01": "Data/Models/Tree_01.bin"
}
```

---

## JSON 출력 포맷 방향

- MainEditor 구조: `MainEditor/Default/MainEditor.cpp`, `MainEditor/Public/MainEditorApp.h`
- 엔진 정의: `Engine/Public/Engine_Define.h`
- 기존 데이터 포맷: `Client` 프로젝트의 `DT_StaticLevel.json`, `DT_GameObject.json`
- 모작 대상: Clair Obscur: Expedition 33 (Sandfall Interactive)
