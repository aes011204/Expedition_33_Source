# 맵 최적화 계획 - Level_SpringMeadows_Main_V2

## 현황 분석

| 항목 | 수치 |
|------|------|
| SaveJson 크기 | 252MB |
| StaticMesh 오브젝트 수 | 6,955개 |
| InstancedMesh 모델 수 | 49개 |
| 총 인스턴스 수 | ~373,000개 |
| FBX 파일 수 | 499개 (총 3.7GB) |
| 최대 나무 FBX | 275MB (SM_EuropeanBeech_Field_01_PP.fbx) |

### 주요 인스턴스 (상위)
- SM_ground_foliage_03: 125,915개
- S_Tundra_Grass 계열: ~117,000개
- SM_Knautia_01: 48,413개
- SM_DeadLeaves_Flat: 21,176개

### 기존 구현 현황
- GPU 프러스텀 컬링 (Compute Shader) — 구현됨
- Octree 기반 StaticMesh 컬링 — 에디터에 구현됨
- 프레임 예산 기반 비동기 로딩 (Process_LoadSession) — 구현됨

---

## 문제 목록

### 문제 1 — FBX 메시 크기 (Nanite 메시)
나무 FBX가 80~275MB로 UE5 Nanite 전용 고폴리 메시.
커스텀 DX11 엔진에서 그대로 변환 시 VRAM 초과.

### 문제 2 — SaveJson 252MB (로딩 시간)
373k 인스턴스의 위치/회전/스케일이 텍스트 JSON으로 저장.
파싱만 수십 초~수 분 소요.

### 문제 3 — GPU readback stall (런타임 버그)
`MapInstancedMesh::DispatchCull()`에서 매 프레임 `CopyStructureCount` + `D3D11_MAP_READ`로 GPU→CPU 카운트 읽기.
49개 인스턴스드 메시 × 매 프레임 GPU stall 발생.

### 문제 4 — 텍스처 VRAM
499개 모델의 텍스처가 전부 VRAM에 올라가면 내장 그래픽 한계 초과.
Decimate로 메시를 줄여도 텍스처 VRAM 압박은 별개.

### 문제 5 — 지형 미처리
원본 맵에 UE5 Nanite Landscape 존재 (`bEnableNanite: true`).
현재 SaveJson에 지형 데이터 없음 (StaticMesh/InstancedMesh만 저장 중).

---

## 작업 로드맵

### 1단계 — 실행 가능하게 (필수)

#### 1-1. 나무 FBX Decimate
- Blender Python 배치 스크립트로 자동화
- 대상: 80MB 이상 FBX 파일 (주로 나무 계열 ~20개)
- 목표 폴리곤: 원본의 5~15% 유지 (게임 카메라 거리에서 티 안 남)
- 주의: Decimate는 메시 버퍼 VRAM만 해결, 텍스처는 별도

#### 1-2. 나무 텍스처 다운스케일
- ImageMagick 배치 스크립트로 자동화
- 원본 파일은 절대 수정하지 않음
- 에디터 로드 시 `maxsize` 파라미터로 메모리 단에서만 처리
- `Texture::Initialize_Prototype`에 maxsize 파라미터 추가
  ```cpp
  // DDS, WIC 둘 다 maxsize 파라미터 전달
  size_t maxsize = isEditorMode ? 512 : 0;
  CreateWICTextureFromFile(_device.Get(), path, nullptr, srv.GetAddressOf(), maxsize);
  CreateDDSTextureFromFile(_device.Get(), path, nullptr, srv.GetAddressOf(), maxsize);
  ```

#### 1-3. 지형 처리
- `OriginJson/Level_SpringMeadows_Main_V2/_Generated_/` 내 `.pskx` 파일 10개
- Blender PSK 임포터 플러그인(`io_scene_actorx`) 으로 임포트
- 이미 저용량(~1000KB/타일)이라 Decimate 불필요
- FBX 익스포트 → 팀원 변환 툴로 `.model` 변환
- 텍스처는 기존 풀 텍스처 단일 타일링으로 임시 적용
- SaveJson에 StaticMesh로 추가

#### 1-4. 풀/잔디 Distance Culling
- 거리 초과 시 렌더링 제외
- 권장 기준:
  - 풀/잔디: 80m 이상 렌더링 X
  - 꽃/낙엽: 120m 이상 X
  - 나무/바위: 500m 이상 X
- 실질적 렌더링 인스턴스 수: 373k → 수만 개 수준으로 감소

---

### 2단계 — 로딩/런타임 최적화

#### 2-1. SaveJson → 바이너리
- 252MB JSON → ~15MB 바이너리 (50배 감소)
- 인스턴스 데이터: position(3) + rotation(4) + scale(3) = 10 float = 40 byte/인스턴스
- Python 변환 스크립트 (`BatchTool/` 에 추가)
- C++ MapLoader에 바이너리 포맷 읽기 추가 (기존 JSON 로드와 병행)

#### 2-2. GPU stall 제거
- `MapInstancedMesh::DispatchCull()` 수정
- `CopyStructureCount` + `D3D11_MAP_READ` (CPU 읽기) 제거
- `DrawIndexedInstancedIndirect`로 카운트를 GPU에서 바로 사용
- 관련 파일: `Client/Private/MapInstancedMesh.cpp`

#### 2-3. 텍스처 maxsize (에디터 전용)
- `Engine/Private/Texture.cpp`의 `Initialize_Prototype` 수정
- 에디터 모드 시 512 또는 1024로 제한
- 게임 모드 시 0 (제한 없음)

---

### 3단계 — 에디터 편의

#### 3-1. 인스턴스 밀도 샘플링
- 에디터 로드 시 카테고리별 인스턴스 비율만 렌더링
  - 풀/잔디류: 10% 샘플링 (125k → 12.5k)
  - 꽃/낙엽류: 25% 샘플링
  - 나무/바위류: 100%
- ImGui 슬라이더로 밀도 조절 가능하게
- 배치 분포 파악은 가능, 게임 실행 시 전체 렌더링

#### 3-2. 레이어 토글
- ImGui 패널에 카테고리별 표시/숨김 토글 추가
- 풀/잔디/낙엽 레이어 끄면 에디터 성능 대폭 향상

---

### 4단계 — 비주얼/성능 추가

#### 4-1. Imposter (빌보드 트리)
- 일정 거리 이상 나무를 텍스처 한 장짜리 사각형으로 교체
- 드로우콜 극적으로 감소, 멀리서 시각적 차이 거의 없음

#### 4-2. 그림자 거리 제한
- 주인공 주변 일정 반경 안에서만 그림자 계산

#### 4-3. StaticMesh 자동 인스턴싱
- 로드 시점에 같은 모델 StaticMesh를 자동으로 그룹화
- InstancedMesh로 전환하여 드로우콜 감소

---

### 5단계 — 필요 시

#### 5-1. 공간 청킹 + 스트리밍
- 전제조건: 2단계 바이너리 포맷 완료 후에야 효과 있음
- 맵을 격자로 분할, 플레이어 위치 기반 로드/언로드
- RAM 부족이 실제로 확인됐을 때 진행

#### 5-2. 다단계 LOD 시스템
- 거리별 메시 자동 교체 (LOD0 → LOD1 → LOD2 → Imposter)
- 현재는 단일 Decimate 메시로 대체

#### 5-3. Weightmap 기반 지형 셰이더
- UE5 Landscape 레이어 블렌딩 재현
- `_Generated_/Weightmap_*.png` 활용
- 현재는 단일 타일 텍스처로 임시 처리

---

## 예상 작업 기간

| 단계 | 기간 |
|------|------|
| 1단계 (필수) | 3~5일 |
| 2단계 | 1~1.5주 |
| 3단계 | 3~5일 |
| 4단계 | 2~3주 |
| 5단계 | 1~2달 |
| **1~2단계 합계 (최소)** | **2~3주** |
| **1~3단계 합계** | **약 1달** |

---

## 내장 그래픽 한계

- 게임 플레이: 모든 최적화 후에도 내장 그래픽으로 원활한 플레이 어려움
- 맵 에디터: 1~3단계 완료 시 내장 그래픽에서 작업 가능한 수준
- 게임 테스트: 외장 GPU 환경에서 진행 필요
- 색상/텍스처 확인: 에디터에서 maxsize 100%로 잠깐 올려서 확인 가능

---

## 관련 파일 경로

| 파일 | 설명 |
|------|------|
| `Client/Private/MapLoader.cpp` | 맵 로딩 (JSON 파싱, 모델 로드) |
| `Client/Private/MapInstancedMesh.cpp` | 인스턴스 렌더링 + GPU 컬링 |
| `Engine/Private/Texture.cpp` | 텍스처 로드 (maxsize 수정 대상) |
| `MapEditor/Private/MapEditorApp.cpp` | 에디터 렌더링 루프 |
| `BatchTool/` | Python 배치 스크립트 위치 |
| `Client/Bin/Resources/Map/SaveJson/Level_SpringMeadows_Main_V2.json` | 252MB 맵 배치 데이터 |
| `Client/Bin/Resources/Map/OriginJson/Level_SpringMeadows_Main_V2/_Generated_/` | 지형 타일 (.pskx, Heightmap) |
| `C:\down\Temporary_fbxStorage\Level_SpringMeadows_Main_V2\fbx\` | 원본 FBX 파일들 (3.7GB) |