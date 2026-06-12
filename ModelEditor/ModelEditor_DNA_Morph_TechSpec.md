# ModelEditor DNA Morph Technical Implementation

## 구현 목표

ModelEditor에서 DNA 파일을 읽어 얼굴 morph 데이터를 생성하고, 모델 파일과 같은 경로에 `.morph` 파일로 저장한다. Engine은 `.model` 로드 후 같은 stem의 `.morph`를 자동 로드하여 `ModelDeform`에 주입한다. 런타임에서는 `AnimComputeRuntime`이 morph 데이터를 skinning 이전의 source vertex buffer로 변환한다.

## 최종 데이터 흐름

```text
ModelEditor
    DNA file
      -> DnaMorphParser
      -> MorphData
      -> MorphDataExporter
      -> <modelName>.morph

Engine
    <modelName>.model
      -> ModelCom load
      -> Try_LoadMorphData()
      -> ModelDeform::Set_MorphData()
      -> AnimComputeRuntime::Prepare_MorphDeform()
      -> Shader_Compute_MorphDeform
      -> Shader_Compute_AnimMeshSkinning
```

## 구현 단위

### 1. ModelEditor: DNA parser wrapper

새 파일:

```text
ModelEditor/Public/DnaMorphParser.h
ModelEditor/Private/DnaMorphParser.cpp
```

역할:

- DNA reader 라이브러리와 ModelEditor 사이의 얇은 어댑터
- DNA 파일 로드/해제
- DNA mesh 목록 조회
- DNA blend shape target 목록 조회
- DNA target별 vertex delta 조회
- Engine `MorphData`로 변환

인터페이스:

```cpp
class DnaMorphParser final
{
public:
    HRESULT Load_DNA(const wstring& dnaFilePath);
    HRESULT Build_MorphData(
        const vector<ModelMeshData>& modelMeshes,
        MorphData& outData);

    const vector<string>& Get_TargetNames() const;
    const vector<string>& Get_MeshNames() const;

    void Clear();
};
```

주의:

- DNA binary를 직접 파싱하지 않는다.
- DNA Reader/SDK 호출부는 이 클래스 안에 숨긴다.
- SDK 타입이 프로젝트 전역 헤더로 새지 않게 `.cpp`에 최대한 감춘다.

## DNA reader adapter

DNA SDK API 이름이 프로젝트에 고정되기 전까지는 아래 내부 adapter 형태로 감싼다.

```cpp
struct DnaMeshDesc
{
    string name;
    uint32 vertexCount{};
};

struct DnaTargetDesc
{
    string name;
};

struct DnaVertexDelta
{
    uint32 vertexIndex{};
    Vec3 deltaPosition{};
    Vec3 deltaNormal{};
};
```

`DnaMorphParser.cpp` 내부에서만 사용하는 캐시:

```cpp
vector<DnaMeshDesc> _dnaMeshes;
vector<DnaTargetDesc> _targets;
vector<vector<vector<DnaVertexDelta>>> _meshTargetDeltas;
```

의미:

```text
_meshTargetDeltas[dnaMeshIndex][targetIndex] = 해당 mesh/target의 delta 목록
```

## MorphData 변환 규칙

Engine 쪽 `MorphData`는 target range 기반으로 저장한다.

```cpp
MorphData.targets[targetIndex]
MorphMeshData.vertexRanges[targetIndex]
MorphMeshData.vertexDeltas[range.start ~ range.start + range.count]
```

변환 순서:

1. DNA target 목록을 `MorphData.targets`에 복사
2. DNA mesh를 model mesh에 매칭
3. 매칭된 mesh마다 `MorphMeshData` 생성
4. targetIndex 순서대로 delta를 `vertexDeltas`에 append
5. append 직전 offset/count를 `vertexRanges[targetIndex]`에 기록

의사 코드:

```cpp
for each matched mesh:
    MorphMeshData meshData;
    meshData.meshIndex = modelMeshIndex;
    meshData.meshName = modelMeshName;
    meshData.vertexCount = modelVertexCount;
    meshData.vertexRanges.resize(targetCount);

    for targetIndex in targets:
        uint32 start = meshData.vertexDeltas.size();

        for delta in dnaMeshTargetDeltas[dnaMeshIndex][targetIndex]:
            if delta.vertexIndex >= modelVertexCount:
                continue;

            MorphVertexDelta outDelta;
            outDelta.vertexIndex = delta.vertexIndex;
            outDelta.weightScale = 1.f;
            outDelta.deltaPosition = delta.deltaPosition;
            outDelta.deltaNormal = delta.deltaNormal;
            meshData.vertexDeltas.push_back(outDelta);

        meshData.vertexRanges[targetIndex].start = start;
        meshData.vertexRanges[targetIndex].count = meshData.vertexDeltas.size() - start;

    outData.meshes.push_back(std::move(meshData));
```

## Mesh matching

새 helper 후보:

```text
ModelEditor/Public/DnaMeshMatcher.h
ModelEditor/Private/DnaMeshMatcher.cpp
```

처음에는 `DnaMorphParser` 내부 private 함수로 시작해도 된다. 길어지면 분리한다.

매칭 우선순위:

1. DNA mesh name == model mesh name
2. sanitize(DNA mesh name) == sanitize(model mesh name)
3. vertexCount 동일 + name 일부 포함
4. vertexCount 동일한 후보가 1개뿐이면 사용

실패 정책:

- 매칭 실패 mesh는 skip
- 전체 변환 실패로 처리하지 않는다
- 로그는 반드시 남긴다

로그:

```text
[DNA Morph] Mesh matched: dna=Lune_FaceMesh, modelIndex=0, model=Lune_FaceMesh
[DNA Morph] Mesh skipped: dna=Lune_Eyelashes, reason=no matching model mesh
```

## ModelConverter 변경

대상 파일:

```text
ModelEditor/Public/ModelConverter.h
ModelEditor/Private/ModelConverter.cpp
```

추가 멤버:

```cpp
bool _exportMorphFromDNA{ false };
char _dnaPathBuffer[MAX_PATH]{};
wstring _dnaPath{};
```

추가 함수:

```cpp
HRESULT Export_MorphData(const wstring& outputDir, const string& modelName);
```

UI 추가 위치:

`ModelConverter::Render()`의 export option 영역.

```cpp
ImGui::Checkbox("Export Morph From DNA", &_exportMorphFromDNA);
ImGui::InputText("DNA Path", _dnaPathBuffer, sizeof(_dnaPathBuffer));
```

Browse 버튼은 2차로 미뤄도 된다. 1차는 text input만으로 충분하다.

호출 위치:

```cpp
if (_exportModelData)
{
    CHECK_FAILED(Export_ModelData(_aiScene, outputDir, modelName), E_FAIL);
}

if (_exportMorphFromDNA)
{
    CHECK_FAILED(Export_MorphData(outputDir, modelName), E_FAIL);
}

CHECK_FAILED(Export_AnimationData(_aiScene, outputDir, modelName), E_FAIL);
```

권장 순서:

- model export 성공 후 morph export
- animation export 전후는 큰 상관 없지만, model mesh 기준 검증이 필요하므로 model export 뒤가 자연스럽다.

## MorphDataExporter

새 파일:

```text
ModelEditor/Public/MorphDataExporter.h
ModelEditor/Private/MorphDataExporter.cpp
```

역할:

- `MorphData`를 `.morph` binary로 저장
- 필요하면 debug json 저장

인터페이스:

```cpp
class MorphDataExporter final
{
public:
    static HRESULT Export_Binary(const wstring& filePath, const MorphData& morphData);
    static HRESULT Export_DebugJson(const wstring& filePath, const MorphData& morphData);
};
```

## .morph binary format

Header:

```cpp
struct MorphFileHeader
{
    char magic[4];      // MORP
    uint32 version;     // 1
    uint32 targetCount;
    uint32 meshCount;
};
```

문자열:

```text
uint32 length
char[length] utf8Bytes
```

저장 순서:

```text
MorphFileHeader
sourceName string

target names:
    string[targetCount]

meshes:
    uint32 meshIndex
    meshName string
    uint32 vertexCount
    uint32 rangeCount
    uint32 deltaCount
    MorphVertexRange[rangeCount]
    MorphVertexDelta[deltaCount]
```

검증:

- `rangeCount == targetCount`
- `range.start + range.count <= deltaCount`
- `delta.vertexIndex < vertexCount`

## Engine loader

대상:

```text
Engine/Public/ModelDeform.h
Engine/Private/ModelDeform.cpp
Engine/Private/ModelCom.cpp
```

`ModelDeform::Try_LoadMorphData(const string& modelPath)` 구현:

```text
modelPath -> extension replace ".morph"
file exists check
header read
magic/version validate
MorphData read
Set_MorphData(std::move(data))
```

실패 정책:

- `.morph` 없음: `S_OK`
- 파일 있음 + header invalid: `E_FAIL`
- 파일 있음 + 일부 mesh invalid: 해당 mesh skip 또는 전체 fail 중 선택

1차는 개발 단계이므로 invalid file은 `E_FAIL`로 잡는 편이 낫다.

## ModelCom load 위치

`Try_LoadMorphData(modelPath)`는 `.model` 로드 후 호출해야 한다.

조건:

- `_meshes` 생성 완료
- skeletal mesh source vertices 접근 가능
- mesh name / vertex count 검증 가능

흐름:

```text
ModelCom::Load model binary
    -> meshes 생성
    -> bones/materials 생성
    -> Try_LoadMorphData(modelPath)
```

## Compute shader

새 파일:

```text
Engine/Bin/ShaderFiles/Compute/Shader_Compute_MorphDeform.hlsl
```

include:

```hlsl
#include "../Engine_Shader_ComputeAnimTypes.hlsli"
```

입력:

```hlsl
StructuredBuffer<SourceVertex> g_SourceVertices : register(t0);
StructuredBuffer<MorphVertexRange> g_VertexRanges : register(t1);
StructuredBuffer<MorphGpuInfluence> g_Influences : register(t2);
StructuredBuffer<float> g_MorphWeights : register(t3);
RWStructuredBuffer<SourceVertex> g_OutputVertices : register(u0);
```

cbuffer:

```hlsl
cbuffer MorphDeformCB : register(b0)
{
    uint g_VertexCount;
    uint g_TargetCount;
    uint g_Pad0;
    uint g_Pad1;
};
```

주의:

- `MorphDeformCB`는 공용 hlsli가 아니라 morph shader 파일 안에 둔다.
- `MorphVertexRange`, `MorphGpuInfluence`는 공용 hlsli에 둔다.
- `SourceVertex`는 현재 skinning shader 안에 local struct로 있으므로, morph shader에도 같은 layout을 쓰거나 추후 hlsli로 승격한다.

## Shader registration

대상:

```text
Client/Bin/Resources/Data/json/DT_Shader.json
Client/Default/Client.vcxproj
Client/Default/Client.vcxproj.filters
```

JSON:

```json
{
    "Id": "ComputeShader_MorphDeform",
    "Path": "../../../Engine/Bin/ShaderFiles/Compute/Shader_Compute_MorphDeform.hlsl",
    "Type": "Compute",
    "Level": "Static"
}
```

## Runtime 연결

`AnimComputeRuntime::CreateDesc`는 이미 `morphShader`를 받을 수 있다.

연결 대상:

```text
Client/Private/PlayableParts.cpp
Client/Private/Monster_Part.cpp
ModelEditor/Private/ModelViewerObject.cpp
```

조건부 연결:

```cpp
Shared<ComputeShaderCom> morphShader{};

if (_model->Has_MorphData())
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("ComputeShader_MorphDeform"), morphShader), E_FAIL);
}

runtimeDesc.morphShader = morphShader;
```

ModelEditor는 아직 `AnimComputeRuntime`을 쓰지 않으므로 preview 단계에서 별도 적용한다.

## ModelEditor preview

### 1차

파일 생성/로딩 검증만 한다.

UI 표시:

```text
Morph file path
target count
mesh count
selected mesh delta count
target list
```

### 2차

`ModelViewerObject`에서 morph weight slider를 제공한다.

```cpp
_modelCom->Set_MorphWeight(targetIndex, weight);
```

### 3차

ModelEditor viewer도 compute skinning 경로를 사용한다.

필요 component:

```text
ComputeShader_BoneRemap
ComputeShader_AnimMeshSkinning
ComputeShader_MorphDeform
AnimComputeRuntime
```

## 위험 요소

### Vertex order

DNA delta는 vertex index 기반이다. `ModelConverter`가 vertex order를 바꾸면 morph가 깨진다.

검토해야 할 Assimp flag:

```text
aiProcess_JoinIdenticalVertices
aiProcess_ImproveCacheLocality
aiProcess_OptimizeMeshes
```

얼굴 morph export 모드에서는 위 flag를 끄는 별도 import profile을 둔다.

### Mesh name mismatch

DNA mesh name과 model mesh name이 다를 수 있다. name 매칭 실패 시 vertex count fallback을 사용하되, 후보가 여러 개면 skip한다.

### Normal delta

DNA에서 normal delta 확보가 안 되면 1차는 position delta만 저장한다. normal은 zero delta로 두고, 조명 품질 문제를 확인한 뒤 확장한다.

### SDK dependency

DNA reader SDK는 ModelEditor에만 직접 붙인다. Engine runtime은 `.morph`만 읽어야 하며 DNA SDK를 의존하지 않는다.

## 구현 단계

### Step 1

- `DnaMorphParser` 추가
- DNA 파일 로드/target list 로그 출력

### Step 2

- DNA mesh list 로그 출력
- model mesh와 matching

### Step 3

- `MorphData` 구성
- debug json export

### Step 4

- `.morph` binary export
- `ModelDeform::Try_LoadMorphData()` binary read

### Step 5

- `Shader_Compute_MorphDeform.hlsl` 작성
- `DT_Shader.json` 등록
- runtime morph shader 연결

### Step 6

- ModelEditor morph target slider preview
- animation + morph 동시 검증

## 완료 기준

- DNA 파일에서 target list를 읽을 수 있다.
- DNA mesh가 model mesh에 매칭된다.
- `.morph` 파일이 model 옆에 생성된다.
- Engine에서 `.morph`를 읽고 `ModelDeform`에 주입한다.
- morph weight 0에서 원본과 동일하다.
- morph weight 1에서 target delta가 적용된다.
- animation 재생 중 morph가 skinning 이전에 적용된다.
