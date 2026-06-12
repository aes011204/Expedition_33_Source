# ModelEditor DNA Morph Parsing Plan

## 목적

ModelEditor에서 MetaHuman 계열 DNA 파일을 읽어 얼굴 Morph 데이터를 생성하고, 모델 파일과 같은 경로에 별도 morph 데이터 파일로 저장한다. 이후 Engine의 `ModelCom`은 모델 로드 시 같은 경로의 morph 파일을 찾아 `ModelDeform`에 주입하고, `AnimComputeRuntime`은 해당 데이터를 compute morph pass로 skinning 이전에 적용한다.

이번 계획은 **ModelEditor를 통한 데이터 생성/검증 흐름**을 중심으로 한다. Engine 쪽 morph compute 골격은 이미 일부 준비되어 있으므로, 남은 작업은 DNA 파싱, mesh 매칭, morph 파일 저장, 로드 연결, 뷰어 검증이다.

## 현재 전제

- `ModelCom`은 `ModelDeform`을 통해 `MorphData`, morph weight, dirty 상태를 다룰 수 있다.
- `AnimComputeRuntime`은 morph 데이터를 GPU용 influence buffer로 재배열하는 구조가 잡혀 있다.
- morph compute shader는 아직 실제 등록/연결 전이다.
- `ModelEditor`의 기존 모델 변환 진입점은 `ModelConverter`다.
- `ModelViewerObject`는 모델 뷰어 역할을 하지만 현재 compute morph preview 경로는 아직 없다.
- DNA 파일은 FBX/PSK처럼 단순 vertex mesh가 아니라, 얼굴 rig 정의, blend shape channel, mesh delta, joint/behavior 정보를 담는 별도 리소스로 본다.

## 목표 결과물

모델 변환 후 같은 디렉터리에 다음과 같은 산출물이 생기게 한다.

```text
FaceMesh.model
FaceMesh.morph
```

또는 모델명이 캐릭터 기준이면:

```text
Lune_FaceMesh.model
Lune_FaceMesh.morph
```

`Try_LoadMorphData()`는 나중에 `FaceMesh.model` 로드 시 같은 stem의 `FaceMesh.morph`를 찾아 읽는다.

## 데이터 흐름

```text
DNA 파일 선택
    -> DNA Reader로 mesh / blend shape channel / delta 읽기
    -> 현재 ModelCom/ModelConverter mesh와 이름 또는 vertex count로 매칭
    -> Engine::MorphData 구성
    -> .morph 파일 저장
    -> ModelViewerObject에서 weight slider로 미리보기
    -> Engine 런타임에서 model load 시 .morph 자동 로드
```

## 핵심 설계

### DNA 데이터에서 필요한 항목

최소 1차 목표에서는 아래 정보만 필요하다.

```text
mesh name
vertex count
blend shape channel name
target별 affected vertex index
target별 delta position
target별 delta normal
```

DNA에 normal delta가 없는 경우에는 1차에서 `deltaNormal = Vec3::Zero`로 둔다. 이후 필요하면 normal 재계산 또는 DNA normal delta 대응을 추가한다.

### Engine MorphData 해석

현재 `MorphData`는 다음처럼 해석한다.

```cpp
MorphData::targets[targetIndex]
MorphMeshData::vertexRanges[targetIndex]
MorphMeshData::vertexDeltas[range.start ~ range.start + range.count]
```

즉 `MorphVertexDelta`에는 `targetIndex`를 넣지 않는다. target index는 `vertexRanges`의 배열 index로 판단한다.

```cpp
struct MorphMeshData
{
    uint32 meshIndex{};
    string meshName{};
    uint32 vertexCount{};
    vector<MorphVertexRange> vertexRanges; // targetIndex 기준
    vector<MorphVertexDelta> vertexDeltas; // 모든 target delta를 flat 배열로 저장
};
```

이 구조를 유지하면 `AnimComputeRuntime::Set_MorphVertices()`가 target range를 순회하면서 GPU용 `MorphGpuInfluence`에 targetIndex를 채운다.

## 파일 포맷 계획

### 저장 확장자

1차는 binary `.morph`로 간다.

```text
<model-stem>.morph
```

예:

```text
Lune_FaceMesh.model
Lune_FaceMesh.morph
```

### 헤더

```cpp
struct MorphFileHeader
{
    char magic[4];      // "MORP"
    uint32 version;     // 1
    uint32 targetCount;
    uint32 meshCount;
};
```

문자열은 기존 model export처럼 `uint32 length + bytes` 방식으로 맞춘다.

### 저장 순서

```text
Header
sourceName string

targets:
    targetName string

meshes:
    meshIndex uint32
    meshName string
    vertexCount uint32
    rangeCount uint32
    deltaCount uint32
    MorphVertexRange[rangeCount]
    MorphVertexDelta[deltaCount]
```

`rangeCount`는 원칙적으로 `targetCount`와 같아야 한다. 특정 mesh가 어떤 target의 delta가 없으면 해당 range는 `{ start = currentDeltaOffset, count = 0 }`로 저장한다.

## ModelEditor 작업 범위

### 1. DNA 파서 래퍼 추가

새 클래스 후보:

```text
ModelEditor/Public/DnaMorphParser.h
ModelEditor/Private/DnaMorphParser.cpp
```

역할:

- DNA 파일 열기
- DNA reader 초기화
- mesh 목록 읽기
- blend shape channel 목록 읽기
- mesh별 target delta 읽기
- `Engine::MorphData`로 변환

인터페이스 초안:

```cpp
class DnaMorphParser final
{
public:
    HRESULT Load_DNA(const wstring& dnaFilePath);
    HRESULT Build_MorphData(const vector<ModelMeshData>& modelMeshes, MorphData& outData);
    void Clear();
};
```

`modelMeshes`를 받는 이유는 DNA mesh와 현재 변환된 `.model` mesh를 매칭해야 하기 때문이다.

### 2. DNA mesh와 Model mesh 매칭

우선순위:

1. mesh name 완전 일치
2. sanitize 후 name 일치
3. vertex count 일치
4. name 유사 + vertex count 일치

매칭 실패 시 해당 mesh의 morph는 스킵하고 로그를 남긴다.

로그 예:

```text
[DNA Morph] Mesh match failed. dnaMesh=Lune_FaceMesh, vertexCount=12345
[DNA Morph] Mesh matched. dnaMesh=Lune_FaceMesh -> modelMeshIndex=0
```

주의:

- DNA vertex order와 exported model vertex order가 다르면 delta 적용이 깨진다.
- 따라서 1차 검증에서는 DNA가 기준으로 삼는 face mesh와 ModelConverter가 읽은 face mesh의 vertex count/order가 유지되는지 확인해야 한다.
- Assimp import option 중 `aiProcess_JoinIdenticalVertices`, `aiProcess_OptimizeMeshes`는 vertex order를 바꿀 수 있다. 얼굴 morph 대상 mesh는 별도 옵션으로 vertex order 보존이 필요할 수 있다.

### 3. ModelConverter에 DNA 입력 추가

`ModelConverter` UI에 다음 항목을 추가한다.

```text
[ ] Export Morph From DNA
DNA Path: [               ] [Browse]
```

동작:

- 체크가 꺼져 있으면 기존 FBX 변환만 수행한다.
- 체크가 켜져 있고 DNA path가 있으면 model export 이후 morph export를 수행한다.
- DNA path가 비어 있으면 경고 로그를 남기고 morph export만 스킵한다.

멤버 후보:

```cpp
bool _exportMorphFromDNA{ false };
char _dnaPathBuffer[MAX_PATH]{};
wstring _dnaPath{};
```

### 4. Export_MorphData 추가

`ModelConverter`에 함수 추가 후보:

```cpp
HRESULT Export_MorphData(const wstring& outputDir, const string& modelName);
```

호출 위치:

```text
Convert_Model()
    ReadMeshes()
    ReadMaterial()
    ReadAnimations()
    Export_ModelData()
    Export_AnimationData()
    Export_MorphData()
```

권장 위치는 `Export_ModelData()` 성공 이후다. `.morph`는 `.model` mesh index/vertex count에 의존하므로 model export가 실패했다면 morph export도 하지 않는다.

### 5. Morph binary writer

ModelConverter에 직접 넣어도 되지만, 길어질 가능성이 높으므로 helper로 분리한다.

후보:

```text
ModelEditor/Public/MorphDataExporter.h
ModelEditor/Private/MorphDataExporter.cpp
```

역할:

- `MorphData`를 `.morph` binary로 저장
- debug json 저장 옵션 제공 가능

인터페이스 초안:

```cpp
class MorphDataExporter final
{
public:
    static HRESULT Export_Binary(const wstring& filePath, const MorphData& morphData);
    static HRESULT Export_DebugJson(const wstring& filePath, const MorphData& morphData);
};
```

## Engine 로드 작업 범위

### 1. ModelDeform loader 구현

현재 `ModelDeform::Try_LoadMorphData(const string& modelPath)`는 stub이다.

추후 역할:

- `modelPath`의 확장자를 `.morph`로 교체
- 파일 존재 확인
- binary read
- version/magic 검증
- `MorphData` 구성
- `Set_MorphData(std::move(data))`

### 2. ModelCom에서 로드 호출

모델 로드 완료 후 호출한다.

후보 위치:

```text
ModelCom::Initialize_Prototype(...)
ModelCom::Initialize(...)
ModelCom::Create(...)
```

정확한 위치는 현재 `.model`을 읽고 `_meshes`가 완성된 뒤여야 한다. mesh count/name/vertex count 검증을 해야 하기 때문이다.

### 3. 로드 검증

`.morph`의 mesh 정보와 로드된 model mesh를 비교한다.

검증 항목:

- meshIndex가 model mesh count 범위 안인지
- meshName이 동일하거나 허용 가능한지
- vertexCount가 `SkeletalMesh::Get_SourceVertices().size()`와 같은지
- targetCount가 0이 아닌지

검증 실패 시 morph 전체 실패가 아니라 해당 mesh만 스킵하는 방향이 안전하다.

## Morph compute 연결 작업

### 1. shader 작성

새 파일:

```text
Engine/Bin/ShaderFiles/Compute/Shader_Compute_MorphDeform.hlsl
```

구성:

```hlsl
#include "../Engine_Shader_ComputeAnimTypes.hlsli"

StructuredBuffer<SourceVertex> g_SourceVertices : register(t0);
StructuredBuffer<MorphVertexRange> g_VertexRanges : register(t1);
StructuredBuffer<MorphGpuInfluence> g_Influences : register(t2);
StructuredBuffer<float> g_MorphWeights : register(t3);
RWStructuredBuffer<SourceVertex> g_OutputVertices : register(u0);

cbuffer MorphDeformCB : register(b0)
{
    uint g_VertexCount;
    uint g_TargetCount;
    uint g_Pad0;
    uint g_Pad1;
};
```

`SourceVertex`는 skinning shader와 동일한 layout을 써야 한다. 중복을 줄이려면 나중에 `SourceVertex`도 compute anim hlsli로 올릴 수 있다.

### 2. shader 등록

`DT_Shader.json`에 추가:

```json
{
    "Id": "ComputeShader_MorphDeform",
    "Path": "../../../Engine/Bin/ShaderFiles/Compute/Shader_Compute_MorphDeform.hlsl",
    "Type": "Compute",
    "Level": "Static"
}
```

### 3. runtime 생성부 연결

`PlayableParts`, `Monster_Part`, 추후 `ModelViewerObject`에서:

```cpp
if (_model->Has_MorphData())
{
    Add_Component(ETOI(LevelType::Static), TEXT("ComputeShader_MorphDeform"), morphShader);
    runtimeDesc.morphShader = morphShader;
}
```

ModelEditor preview는 `ModelViewerObject`가 현재 일반 bone matrix render 경로를 쓰고 있으므로, compute skinning preview를 켤지 별도 preview pass를 만들지 결정이 필요하다.

## ModelEditor Preview 계획

### 1차 Preview

ModelEditor에서 `.morph` 생성 후 정보만 확인한다.

표시 항목:

```text
Morph targets count
Morph meshes count
Selected mesh vertex count
Selected target delta count
Top 20 target names
```

### 2차 Preview

`ModelViewerObject`에 morph weight UI를 붙인다.

UI 예:

```text
Morph Targets
[jawOpen]        slider 0.0 ~ 1.0
[eyeBlinkLeft]   slider 0.0 ~ 1.0
Reset Weights
```

호출:

```cpp
_modelCom->Set_MorphWeight(targetIndex, weight);
```

### 3차 Preview

ModelEditor에서도 `AnimComputeRuntime` 경로를 사용해서 실제 GPU morph + skinning 결과를 본다.

필요 작업:

- `ModelViewerObject`에 compute skinning runtime 추가
- `ComputeShader_BoneRemap`
- `ComputeShader_AnimMeshSkinning`
- `ComputeShader_MorphDeform`
- Render 시 `Render_ComputeSkinned()` 사용

## 검증 체크리스트

### 데이터 검증

- DNA target count가 예상과 맞는가
- target 이름이 비어 있지 않은가
- mesh 매칭이 성공하는가
- vertex count가 model mesh와 같은가
- delta index가 vertexCount 범위를 넘지 않는가
- range start/count가 delta 배열 범위를 넘지 않는가

### 런타임 검증

- morph weight가 전부 0이면 기존 모델과 동일한가
- 특정 target 1.0 적용 시 해당 표정만 변하는가
- 여러 target을 동시에 켰을 때 누적이 깨지지 않는가
- animation 재생 중 morph가 skinning 이전에 적용되는가
- normal delta가 없는 경우 조명 깨짐이 심하지 않은가

### 성능 검증

- target weight가 바뀔 때만 morph dispatch 되는가
- animation은 매 프레임 skinning dispatch 되는가
- morph buffer가 매 프레임 재생성되지 않는가
- weight buffer만 갱신되는가

## 주의 사항

### Vertex order

Morph delta는 vertex index 기반이다. DNA와 `.model`의 vertex order가 다르면 표정이 완전히 깨진다. 얼굴 mesh 변환에서는 Assimp 최적화 플래그를 줄여야 할 가능성이 높다.

검토 대상:

```text
aiProcess_JoinIdenticalVertices
aiProcess_OptimizeMeshes
aiProcess_ImproveCacheLocality
```

얼굴 morph export 모드에서는 위 옵션을 끄는 별도 import profile을 둘 수 있다.

### Mesh split

FBX import 과정에서 mesh가 합쳐지거나 쪼개지면 DNA mesh와 1:1 대응이 실패한다. 얼굴 mesh는 가능한 한 원본 mesh 단위를 유지해야 한다.

### Normal delta

DNA에서 normal delta를 확보하지 못하면 1차는 position delta만 적용한다. 조명 품질이 부족하면 나중에 normal delta 추출 또는 GPU normal 재계산을 검토한다.

### DNA 라이브러리

DNA 파일은 단순 JSON이나 raw mesh 파일이 아니므로 직접 binary parsing을 손으로 구현하지 않는 편이 좋다. 가능한 경우 DNA reader/SDK 래퍼를 붙이고, ModelEditor는 그 결과만 `MorphData`로 변환한다.

## 1차 구현 순서

1. `DnaMorphParser` 빈 클래스 추가
2. DNA file path UI 추가
3. DNA reader 연동 방식 확정
4. DNA target/mesh 목록 로그 출력
5. mesh matching 구현
6. `MorphData` 구성
7. `.morph` binary export
8. `.morph` debug json export
9. `ModelDeform::Try_LoadMorphData()` 구현
10. `ModelEditor`에서 morph target 목록 표시

## 2차 구현 순서

1. `Shader_Compute_MorphDeform.hlsl` 작성
2. `DT_Shader.json` 등록
3. runtime 생성부에서 morph shader 조건부 연결
4. `ModelViewerObject` compute skinning preview 도입
5. morph weight slider UI 추가
6. animation + morph 동시 preview 검증

## 완료 기준

1차 완료:

- ModelEditor에서 DNA 파일을 선택할 수 있다.
- DNA에서 target 목록과 mesh 목록을 읽어 로그로 확인할 수 있다.
- `.model`과 같은 경로에 `.morph` 파일이 생성된다.
- `.morph` debug json으로 range/delta 개수를 확인할 수 있다.
- Engine에서 `.morph` 파일을 읽어 `ModelDeform`에 넣을 수 있다.

2차 완료:

- ModelEditor에서 morph target slider를 조절할 수 있다.
- weight 0에서 원본과 동일하다.
- weight 1에서 선택 target 표정이 보인다.
- 애니메이션 재생 중에도 morph가 적용된다.
