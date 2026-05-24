# Terrain UE 작업 TODO

## 현재 상태
- `VIBuffer_TerrainUE` (Engine): PNG 하이트맵 읽기 + 버텍스/인덱스 버퍼 생성 완료
- `METerrainActor` (MapEditor): 렌더링 확인 완료 (흰색 더미 텍스처 임시 사용 중)
- `Terrain_UE` (Client): 클래스 작성 완료, 미등록 상태

---

## 남은 작업

### 1. 높이 정규화 수정
- **문제**: `scaleY ≈ 1131m` 로 너무 커서 지형이 뾰족하게 나옴
- **원인**: 하이트맵이 0~65535 전체 범위를 사용하지 않음
- **방법 A (빠름)**: `scaleY` 값을 수동으로 줄여서 시각적으로 맞춤
- **방법 B (정확)**: 타일 전체 픽셀값의 실제 min/max를 구해 정규화
  ```
  normalizedH = (h - globalMin) / (globalMax - globalMin)
  ```

### 2. 타일 이음새(Seam) 수정
- **문제**: 인접 타일 경계에서 높이값 불일치로 검정 절벽 발생
- **원인 후보 A**: `Heightmap_0~15` → gridX/gridY 매핑 순서가 FModel 실제 출력과 다를 수 있음
- **원인 후보 B**: FModel 내보내기 시 경계 픽셀이 인접 타일과 공유되지 않음
- **확인 방법**: 타일 1개씩 렌더링해서 각 타일의 모양이 자연스러운지 확인 후 붙여보기

### 3. TerrainDesc 하드코딩 → JSON 로드로 전환
현재 `MapEditorApp::Ready_Terrain()` 에 아래 값들이 하드코딩되어 있음:

| 항목 | 현재 값 | 출처 |
|------|---------|------|
| `scaleXZ` | `189.39394 * 0.01` | Level JSON Scale.X |
| `scaleY` | `221.00925 * (65535/128) * 0.01` | Level JSON Scale.Z |
| `worldOffsetX` | `-73831.09 * 0.01` | Level JSON Location.X |
| `worldOffsetY` | `-51864.97 * 0.01` | Level JSON Location.Z |
| `worldOffsetZ` | `-31991.992 * 0.01` | Level JSON Location.Y |
| 타일 경로 16개 | 직접 입력 | FModel 내보내기 폴더 |
| gridX/gridY | row-major 추정 | 미검증 |

- **목표**: MapEditor에서 맵 파일 로드 시 TerrainDesc도 함께 구성
- **위치**: `SceneContext::LoadFromFile` 또는 별도 terrain config JSON

### 4. 법선(Normal) 계산 개선
- 현재 모든 버텍스 법선이 `(0, 1, 0)` (수직) 고정
- 실제 지형 조명을 위해 주변 높이값 기반 법선 계산 필요
- `Build_Tile` 완료 후 후처리로 계산 가능

### 5. 텍스처 연결
- 현재 `METerrainActor`에서 1×1 흰색 더미 SRV 임시 사용 중
- 실제 WeightMap + 레이어 텍스처 연결 필요 (별도 작업)

### 6. Client 등록
- `DT_GameObject.xlsx` 에 `Terrain_UE` / `GamePlay` 행 추가 후 변환
- `Level_GamePlay.cpp`에 `Ready_Layer_Terrain()` 함수 추가
- `TerrainDesc` 값은 MapEditor 검증 완료 후 동일하게 사용

---

## 참고 경로
- 하이트맵 PNG: `Client/Bin/Resources/Map/OriginJson/Level_SpringMeadows_Main_V2/_Generated_/Heightmap_0~15.png`
- 원본 JSON: `Client/Bin/Resources/Map/OriginJson/Level_SpringMeadows_Main_V2.json`
- Landscape 위치/스케일: JSON 내 `Location=(-73831.09, -31991.992, -51864.97)` / `Scale=(189.39, 189.39, 221.01)`