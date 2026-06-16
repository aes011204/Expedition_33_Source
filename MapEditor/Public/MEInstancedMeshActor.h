#pragma once
#include "MapEditorDefines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
class ComputeShaderCom;
class ComputeStructuredBuffer;
NS_END

NS_BEGIN(MapEditor)

class MEModelManager;

class MEInstancedMeshActor final : public GameObject
{
public:
    MEInstancedMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MEInstancedMeshActor(const MEInstancedMeshActor& prototype);
    virtual ~MEInstancedMeshActor() = default;

public:
    Shared<ModelCom> GetModel() const { return _lodModels[0]; }
    Shared<ModelCom> GetLodModel(int lod) const { return _lodModels[lod]; }
    uint32 Get_InstanceCount() const { return _instanceCount; }

    void Set_CameraFar(float camfar) { _cameraFar = camfar; }
    void Set_CullShader(Shared<ComputeShaderCom> shader) { _cullShader = shader; }
    void Set_LodModel(int lod, Shared<ModelCom> model);

public:
    virtual HRESULT Initialize(void* arg) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT RenderPickingID(uint32 id) override;

private:
    Shared<ModelCom> _lodModels[3];
    Shared<ShaderCom> _shader;
    ComPtr<Buffer> _instanceVB;
    uint32 _instanceCount = {};
    uint32 _instanceCapacity = {};
    float _cameraFar = 1000.f;

    wstring _loadedModelPath = {};

    Shared<ComputeShaderCom> _cullShader;
    Shared<ComputeStructuredBuffer> _instanceSrcBuffer;
    Shared<ComputeStructuredBuffer> _sphereBuffer;
    ComPtr<Buffer> _cullCB;

    ComPtr<Buffer> _visibleStructured[3];
    ComPtr<UnorderedAccessView> _visibleUAV[3];
    ComPtr<Buffer> _visibleVB[3];
    vector<ComPtr<Buffer>> _indirectArgsBuffers[3];

    float _lod1Dist = 80.f;
    float _lod2Dist = 200.f;

private:
    HRESULT CreateInstanceBuffer(const vector<Matrix>& matrices);
    void GenerateInstances(const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed, vector<MESHINSTANCEDATA>& outInstances);
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();

    HRESULT CreateCullBuffers(const vector<Matrix>& matrices, const vector<InstanceTransform>& transforms);
    void DispatchCull();
    static array<Vec4, 6> ExtractFrustumPlanes(const Matrix& vp);

public:
    HRESULT UpdateInstances(const wstring& modelPath, const vector<InstanceTransform>& transforms, MEModelManager* modelManager = nullptr);

public:
    static Shared<MEInstancedMeshActor> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
