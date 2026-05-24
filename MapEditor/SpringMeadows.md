# SpringMeadows 지형 형상 미리보기 플랜

## 요약
목표는 머티리얼과 충돌을 빼고, `SpringMeadows` 지형의 “모양”을 맵 에디터에서 최대한 원본 게임 배치 그대로 확인하는 것이다. 현재 export 기준으로 실제 지형 형상은 `Heightmap_*`가 아니라 `Level_SpringMeadows_Main_V2/_Generated_/<hash>/LandscapeNaniteMesh_0.pskx` 쪽에 있으므로, 이 파일들을 우리 엔진의 `.model`로 변환한 뒤 16개 `LandscapeStreamingProxy` 위치에 배치하는 전용 preview map json을 만든다.

## 핵심 방향
- `Heightmap` 복원은 v1 목표에서 제외한다.
- `LandscapeNaniteMesh_0.pskx`를 지형 형상의 기준 데이터로 사용한다.
- 머티리얼, weightmap, RVT, landscape layer는 이번 단계에서 제외한다.
- 충돌도 이번 단계에서 제외하고, 눈으로 지형 연결과 형태를 확인하는 데 집중한다.
- 첫 검증 경로는 `MapEditor`다. `MainEditor` 런타임 로딩은 캐시, 충돌, `.bin` 우선 로딩 변수가 있어서 v1 확인용으로는 뒤로 미룬다.

## 구현 계획
- `ModelEditor::ModelConverter`가 `.pskx`를 받을 수 있게 확장한다.
- 기존 변환기는 `.fbx` 중심이므로, 파일 스캔/드래그드롭/선택 변환 조건에 `.pskx`를 추가한다.
- Assimp가 `.pskx` 확장자를 직접 못 읽으면, 임시 `.psk` alias 경로로 재시도한다. 파일 헤더가 `ACTRHEAD`, `PNTS0000` ActorX 계열이라 이 우회가 가장 작다.
- 변환 대상은 아래 12개 unique hash 폴더의 `LandscapeNaniteMesh_0.pskx`다.
```text
0F4L9OFLRA65NRXF6JQYYOHAR
2WUASXANX11YPRZZRGGQQU9L2
5ZISZJ0O437MAFXF5MAOW5UBY
8RVL8KPMOV8AH3ULA6R38SP6L
ABDVZUFU1PXJVKO9XH4MRNXR5
AO7YL4I19HLULFIG5L5RMJRDT
B5MJC6MG5E1947EL4LA3GIUOE
BYMVCAFHARJJW7RU8IJGYJ1Q3
D4I0EHPGFGTMHHU4WRZ0YTJV0
DFP2VFSLHRGXTVAGNDOLYLD5B
DLJM43O4MC2ZL9PBRCH0HBQM1
EZLDUKD9TRITI0QFV5ZGYGW8I
```
- 변환 결과는 `Client/Bin/Resources/Map/Level_SpringMeadows_Main_V2_Landscape/`에 `<hash>.model`로 저장한다.
- `Client/Bin/Resources/Map/SaveJson/SpringMeadows_LandscapePreview.json`을 만든다.
- preview json의 `MapData.ModelFolder`는 `Level_SpringMeadows_Main_V2_Landscape`로 둔다.
- preview json의 `StaticMeshes`에는 16개 proxy를 넣고, `ModelPath`는 `<hash>.model`로 둔다.

## 배치 기준
현재 `MapEditor`의 `MEUe5Importer` 변환 규칙을 그대로 따른다.

```cpp
UE Location X,Y,Z -> Engine Position X,Z,Y / 100
UE Scale X,Y,Z    -> Engine Scale X,Z,Y
```

지형 루트 기준:
```text
UE Location = (-73831.09, -31991.992, -51864.97)
UE Scale    = (189.39394, 189.39394, 221.00925)
```

PSKX 로컬 bounds 확인 결과:
```text
각 타일은 대략 X=-1..125, Y=-125..1
타일 간격 = 126 * 189.39394 / 100 = 238.636
```

preview 배치값:
```text
Scale = (189.39394, 221.00925, 189.39394)
Rotation = (0, 0, 0, 1)
```

16개 배치표:
```text
(-2,-2) EZLDUKD9TRITI0QFV5ZGYGW8I  Pos=(-1215.584, -518.650, -797.193)
(-1,-2) DLJM43O4MC2ZL9PBRCH0HBQM1  Pos=( -976.947, -518.650, -797.193)
( 0,-2) AO7YL4I19HLULFIG5L5RMJRDT  Pos=( -738.311, -518.650, -797.193)
( 1,-2) 2WUASXANX11YPRZZRGGQQU9L2  Pos=( -499.675, -518.650, -797.193)

(-2,-1) B5MJC6MG5E1947EL4LA3GIUOE  Pos=(-1215.584, -518.650, -558.556)
(-1,-1) 8RVL8KPMOV8AH3ULA6R38SP6L  Pos=( -976.947, -518.650, -558.556)
( 0,-1) BYMVCAFHARJJW7RU8IJGYJ1Q3  Pos=( -738.311, -518.650, -558.556)
( 1,-1) 0F4L9OFLRA65NRXF6JQYYOHAR  Pos=( -499.675, -518.650, -558.556)

(-2, 0) B5MJC6MG5E1947EL4LA3GIUOE  Pos=(-1215.584, -518.650, -319.920)
(-1, 0) 8RVL8KPMOV8AH3ULA6R38SP6L  Pos=( -976.947, -518.650, -319.920)
( 0, 0) BYMVCAFHARJJW7RU8IJGYJ1Q3  Pos=( -738.311, -518.650, -319.920)
( 1, 0) 0F4L9OFLRA65NRXF6JQYYOHAR  Pos=( -499.675, -518.650, -319.920)

(-2, 1) D4I0EHPGFGTMHHU4WRZ0YTJV0  Pos=(-1215.584, -518.650,  -81.284)
(-1, 1) DFP2VFSLHRGXTVAGNDOLYLD5B  Pos=( -976.947, -518.650,  -81.284)
( 0, 1) 5ZISZJ0O437MAFXF5MAOW5UBY  Pos=( -738.311, -518.650,  -81.284)
( 1, 1) ABDVZUFU1PXJVKO9XH4MRNXR5  Pos=( -499.675, -518.650,  -81.284)
```

## 검증 계획
- `.pskx -> .model` 변환 후 12개 `.model`이 모두 생성되는지 확인한다.
- `MapEditor`에서 `SpringMeadows_LandscapePreview.json`을 로드한다.
- hierarchy에 landscape static mesh 16개가 들어오는지 확인한다.
- viewport에서 4x4 지형이 끊기지 않고 이어지는지 확인한다.
- 먼저 확인할 seam은 `(0,0)-(1,0)`, `(0,0)-(0,1)`, `(-1,-1)-(0,-1)`이다.
- 지형이 뒤집히거나 행/열이 반대로 보이면 `.model` 변환 결과는 건드리지 않고 preview json의 배치 수식만 수정한다.
- 지형이 너무 멀어서 안 보이면 `MapEditorApp`의 camera far 값을 `1000.f`에서 `3000.f` 이상으로 올린다.

## 확정된 기본값
- v1은 시각 확인 전용이다.
- 머티리얼 적용은 하지 않는다.
- 충돌 생성은 하지 않는다.
- `.bin` 생성은 하지 않는다.
- 기존 SpringMeadows chunk json은 수정하지 않는다.
- 새 preview 리소스는 `Level_SpringMeadows_Main_V2_Landscape` 폴더와 `SpringMeadows_LandscapePreview.json`으로 분리한다.
