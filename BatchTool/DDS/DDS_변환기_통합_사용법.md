# DDS 변환기 사용법

이 문서는 다음 두 배치파일의 사용 방법을 설명합니다.

```text
NvttNormalConverterToDDS.bat
BasicConverterToDDS.bat
```

권장 순서는 다음과 같습니다.

```text
1. NvttNormalConverterToDDS
2. BasicConverterToDDS
```

먼저 NVTT 변환기로 노멀 DDS를 만든 뒤, Basic 변환기를 `Skip` 모드로 실행하면 NVTT 노멀은 유지되고 나머지 텍스처만 생성됩니다.

---

# 1. NvttNormalConverterToDDS

맵 또는 모델의 노멀 텍스처를 찾아 `BC5 DDS` 중간 파일을 만든 뒤, `nvtt_normal.dpf` 프리셋을 적용해 최종 DDS를 생성합니다.

## 사전 준비

다음 파일들이 필요합니다.

```text
NvttNormalConverterToDDS.bat
NvttNormalConverterToDDS.ps1
nvtt_normal.dpf
texconv.exe
```

`nvtt_export.exe`가 다른 폴더에 있다면 PowerShell에서 경로를 등록합니다.

```powershell
$env:NVTT_EXPORT_PATH = "C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe"
```

경로 확인:

```powershell
Test-Path $env:NVTT_EXPORT_PATH
```

`True`가 나오면 정상입니다.

`texconv.exe`가 배치파일과 같은 폴더에 없다면 `TEXCONV_PATH`도 설정해야 합니다.

```powershell
$env:TEXCONV_PATH = "C:\texconv.exe가 있는 경로\texconv.exe"
```

확인:

```powershell
Test-Path $env:TEXCONV_PATH
```

## 실행

PowerShell에서 다음 배치파일을 실행합니다.

```powershell
& "C:\Users\delay\Jusin\Expedition_158\BatchTool\DDS\NvttNormalConverterToDDS.bat"
```

## 메뉴 선택

```text
1. Map
2. Model
3. Effect
4. All
```

### Map

`Resources\Map` 아래의 상대 경로를 입력합니다.

예:

```text
SmallLevel_MF_Zone_01
```

실제 대상:

```text
Client\Bin\Resources\Map\SmallLevel_MF_Zone_01\Textures
```

맵은 다음 순서로 처리됩니다.

```text
노멀 후보 검색
→ 후보 목록 출력
→ 제외할 파일 선택
→ Textures\Normal 폴더로 원본 복사
→ texconv로 BC5 전체 밉맵 중간 DDS 생성
→ nvtt_normal.dpf 프리셋으로 최종 DDS 생성
```

### Model

`Resources\Models` 아래의 상대 경로를 입력합니다. 비워두면 `Resources\Models` 전체를 순회합니다.

예:

```text
Heros\Lune
```

실제 대상:

```text
Client\Bin\Resources\Models\Heros\Lune
```

모델은 파일 이름에 `Normal`이 포함된 이미지를 찾아 원본 파일과 같은 위치에 최종 DDS를 생성합니다.

예:

```text
T_Lune_Body_Normal.png
T_Lune_Body_Normal.dds
```

모델은 별도의 `Textures\Normal` 폴더로 복사하지 않습니다.

### Effect

현재 지원하지 않습니다.

다음 안내가 출력됩니다.

```text
[NOT IMPLEMENTED] Effect NVTT conversion is not implemented. Use BasicConverterToDDS instead.
```

### All

맵 경로와 모델 경로를 각각 입력받아 모두 처리합니다.

Effect는 처리하지 않습니다.

## 기존 DDS 처리

```text
1. Overwrite existing DDS
2. Skip when DDS already exists
```

- `Overwrite`: 기존 DDS가 있어도 NVTT 결과로 다시 생성합니다.
- `Skip`: 같은 이름의 최종 DDS가 있으면 해당 파일을 건너뜁니다.

## 맵 노멀 후보 확인

맵을 선택하면 변환할 노멀 후보 목록이 출력됩니다.

특정 파일을 제외하려면 표시된 파일 이름이나 상대 경로를 입력합니다.

변환을 시작하려면:

```text
go
```

취소하려면:

```text
cancel
```

을 입력합니다.

## 변환 중 출력

```text
[COPY]
```

원본 노멀 이미지를 `Textures\Normal` 폴더로 복사하는 단계입니다.

```text
[BC5]
```

`texconv`로 전체 밉맵이 포함된 BC5 중간 DDS를 생성하는 단계입니다.

```text
[NVTT]
```

`nvtt_normal.dpf` 프리셋을 적용해 최종 DDS를 생성하는 단계입니다.

8K 텍스처는 `[NVTT]` 단계에서 시간이 오래 걸릴 수 있습니다.

작업 관리자에서 `nvtt_export.exe`가 CPU나 GPU를 사용 중이라면 정상적으로 처리 중입니다.

---

# 2. BasicConverterToDDS

맵, 모델, 이펙트 폴더의 일반 텍스처를 찾아 용도에 맞는 DDS 포맷으로 변환합니다.

NVTT가 필요하지 않은 이펙트 노멀도 이 변환기를 사용합니다.

## 사전 준비

다음 파일들이 필요합니다.

```text
BasicConverterToDDS.bat
BasicConverterToDDS.ps1
texconv.exe
```

`texconv.exe`가 배치파일과 같은 폴더에 없다면 PowerShell에서 경로를 등록합니다.

```powershell
$env:TEXCONV_PATH = "C:\texconv.exe가 있는 경로\texconv.exe"
```

확인:

```powershell
Test-Path $env:TEXCONV_PATH
```

`True`가 나오면 정상입니다.

## 실행

PowerShell에서 다음 배치파일을 실행합니다.

```powershell
& "C:\Users\delay\Jusin\Expedition_158\BatchTool\DDS\BasicConverterToDDS.bat"
```

## 메뉴 선택

```text
1. Map
2. Model
3. Effect
4. All
```

### Map

`Resources\Map` 아래의 상대 경로를 입력합니다.

예:

```text
SmallLevel_MF_Zone_01
```

실제 대상:

```text
Client\Bin\Resources\Map\SmallLevel_MF_Zone_01
```

### Model

`Resources\Models` 아래의 상대 경로를 입력합니다.

예:

```text
Heros\Lune
```

실제 대상:

```text
Client\Bin\Resources\Models\Heros\Lune
```

### Effect

`Resources\Effects` 아래의 상대 경로를 입력합니다.

예:

```text
Character\Lune
```

실제 대상:

```text
Client\Bin\Resources\Effects\Character\Lune
```

### All

Map, Model, Effect 경로를 각각 입력받아 모두 처리합니다.

## 기존 DDS 처리

```text
1. Overwrite existing DDS
2. Skip when DDS already exists
```

- `Overwrite`: 기존 DDS가 있어도 다시 생성합니다.
- `Skip`: 같은 이름의 DDS가 있으면 건너뜁니다.

NVTT로 만든 노멀 DDS를 유지하려면 `BasicConverterToDDS`에서는 `Skip`을 선택해야 합니다.

## 변환 규칙

### 컬러 텍스처

예:

```text
BaseColor
Albedo
Diffuse
Color
Emissive
```

변환 설정:

```text
BC7_UNORM_SRGB
전체 밉맵 생성
```

### 순수 노멀 텍스처

예:

```text
Normal
Normals
Norm
NRM
_N
```

변환 설정:

```text
BC5_UNORM
전체 밉맵 생성
```

이 방식은 NVTT가 필요하지 않은 이펙트 노멀 등에 사용합니다.

### 패킹 노멀 및 데이터 텍스처

예:

```text
NMAO
NAO
NME
NRE
Normal_Roughness
Normal_Metallic
Normal_Emissive
Normal_Mask
ORM
Mask
Roughness
Metallic
AO
Height
Cavity
```

변환 설정:

```text
BC7_UNORM
데이터 텍스처용 밉 설정
```

패킹 노멀은 여러 채널에 다른 데이터가 들어 있으므로 BC5로 변환하지 않습니다.

---

# 권장 실행 순서

## Map / Model

```text
1. NvttNormalConverterToDDS 실행
2. 필요에 따라 Overwrite 또는 Skip 선택
3. 노멀 DDS 생성 완료
4. BasicConverterToDDS 실행
5. Skip 선택
6. BaseColor, ORM, Emissive 등 나머지 DDS 생성
```

`BasicConverterToDDS`에서 `Overwrite`를 선택하면 NVTT로 만든 노멀 DDS가 일반 BC5 DDS로 다시 덮어써질 수 있으므로 주의해야 합니다.

## Effect

```text
1. BasicConverterToDDS 실행
2. Effect 선택
3. 대상 경로 입력
4. Overwrite 또는 Skip 선택
```

Effect는 NVTT 변환기를 사용하지 않습니다.

순수 노멀은 Basic 변환기에서 BC5로 처리하고, 패킹 노멀은 BC7 데이터 텍스처로 처리합니다.
