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

### 1단계 - 필수 (Core) — 최우선 구현

| 기능 | 설명 |
|------|------|
| Terrain 생성 다이얼로그 | 크기(X/Z), 해상도(섹션당 쿼드 수), 세계 위치(Y 오프셋 포함) 설정 후 생성 |
| 동적 Heightmap VIBuffer | PNG 없이 메모리 상의 높이값 배열로 메시 실시간 재생성 |
| Sculpt - Raise / Lower | 브러시 범위 내 높이 올리기/내리기 (좌클릭 / Shift+좌클릭) |
| Sculpt - Smooth | 브러시 범위 내 인접 정점 평균화 |
| Sculpt - Flatten | 특정 높이값으로 고정 |
| 브러시 시각화 | 뷰포트에서 커서 위치와 브러시 반경을 원형으로 표시 |
| Heightmap 저장 / 불러오기 | 편집된 높이값을 PNG(16-bit) 또는 RAW로 저장·불러오기 |
| Terrain 다중 배치 | MapEditor에서 Terrain Actor를 여러 개 생성·선택·삭제 가능 |

### 2단계 - 권장 (Quality of Life)

| 기능 | 설명 |
|------|------|
| Sculpt - Erosion | 간단한 열 침식으로 자연스러운 지형 생성 |
| Noise 생성 | Perlin/Simplex 노이즈 기반 초기 지형 자동 생성 |
| 텍스처 레이어 Paint | 레이어별 weight map 편집 + 블렌딩 (grass, rock, dirt 등) |
| LOD | 카메라 거리에 따른 폴리곤 수 감소 |

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
