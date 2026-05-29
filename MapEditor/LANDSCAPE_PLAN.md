# Landscape 기능 구현 계획

## 배경

fmodel 및 ninja ripper로 기존 맵 데이터를 추출·복원하는 방식에 문제가 많아,
MapEditor에 UE Landscape와 유사한 지형 생성/편집 기능을 직접 구현하기로 결정.

---

## 지형 구조 설계 결론

### 문제: 오버행/동굴 구조

게임 맵에 "아래 지형 → 계단 → 위 지형" 구조가 있으며,
위 지형이 아래 지형의 천장 역할을 하는 구간이 존재함 (X,Z 좌표 겹침).

### 결론: Heightmap의 근본적 한계

Heightmap 기반 Terrain(UE Landscape 포함)은 **모든 X,Z 좌표에 높이값이 하나뿐**이므로,
동일 X,Z 범위에서 오버행/동굴 구조를 표현할 수 없음.

### 채택한 구조

```
[상층 Terrain]     ← 위에서 걸어다니는 땅 (Terrain Actor)
      |
 [절벽/측면]       ← Static Mesh (cliff, rock wall)
 [동굴 천장]       ← Static Mesh (cave ceiling)
      |
[하층 Terrain]     ← 아래에서 걸어다니는 땅 (Terrain Actor)
```

- **Terrain Actor 여러 개** : Y 오프셋을 지정해 상층/하층 지형을 독립적으로 배치
- **StaticMesh Actor** : 이미 MapEditor에 있음. 절벽 측면·동굴 천장 등 오버행 구간을 Static Mesh로 채움
- UE 공식 맵 제작 방식과 동일한 접근

---

## 구현할 기능 목록

### 1단계 - 필수 (Core)

| 기능 | 상태 | 설명 |
|------|------|------|
| Terrain 생성 다이얼로그 | ✅ 완료 | 크기(X/Z), 해상도, 세계 위치 설정 후 생성 |
| 동적 Heightmap VIBuffer | ✅ 완료 | `VIBuffer_Landscape` - float[] 높이 배열, D3D11_USAGE_DYNAMIC |
| Sculpt - Raise / Lower | ✅ 완료 | 좌클릭 홀드, 거리 기반 falloff |
| Sculpt - Smooth | ✅ 완료 | 브러시 범위 내 인접 정점 평균화 |
| Sculpt - Flatten | ✅ 완료 | 목표 높이값으로 수렴 |
| Terrain 다중 배치 | ✅ 완료 | 생성·선택·삭제, 독립 transform |
| Heightmap 저장 / 불러오기 | ✅ 완료 | .hmap 바이너리 (헤더 + float[] heights), Windows 파일 다이얼로그 |
| 브러시 시각화 | ✅ 완료 | 깊이 버퍼 픽셀 리드백으로 지형 위 원형 오버레이 표시 |

### 2단계 - 권장 (Quality of Life)

| 기능 | 상태 | 설명 |
|------|------|------|
| Sculpt - Erosion | ✅ 완료 | 열 침식(Thermal Erosion), 반복 횟수·talusAngle·erosionRate 파라미터, 조기 종료 |
| Noise 생성 | ✅ 완료 | Perlin 노이즈, octave·scale·amplitude·seed 파라미터, 덮어쓰기/누적 선택 (`NoiseUtil.h`) |
| LOD | ✅ 완료 | 청크 기반 LOD + Dynamic IB + LOD clamping(이웃 차이 ≤1) + 중심점 기반 경계 스티칭으로 균열 제거 |
| 텍스처 레이어 Paint | ⬜ 미구현 | 레이어별 weight map 편집 + 블렌딩 |

---

## 현재 코드베이스 현황

- **엔진**: C++ / DirectX 11 / ImGui
- **기존 Terrain 관련 클래스**
  - `VIBuffer_TerrainUE` : PNG 16타일 기반 높이맵 로드 (파일 의존적)
  - `METerrainActor` : 지형 렌더링 Actor
  - `TerrainConfig` : heightmapFolder, worldOffset, scale, tiles 구조체

---

## 구현 방향

1. `VIBuffer_TerrainUE` 를 대체하는 **`VIBuffer_Landscape`** 신규 클래스 작성
   - 파일 의존 없이 `float[]` 높이 배열로 동작
   - Sculpt 시 변경된 정점만 GPU 버퍼에 업데이트
2. `METerrainActor` 를 대체하는 **`MELandscapeActor`** 작성
   - 다중 인스턴스 지원 (상층/하층 각각 독립)
   - JSON 직렬화·역직렬화
3. ImGui **Landscape 전용 패널** 또는 Inspector 확장
   - Sculpt 모드 토글, 브러시 크기·강도 슬라이더
   - 생성/저장/불러오기 버튼
4. **브러시 데칼 렌더러** : GPU picking 좌표 기반으로 원형 오버레이 렌더링
