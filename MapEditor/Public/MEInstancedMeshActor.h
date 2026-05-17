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

class MEInstancedMeshActor final : public GameObject
{
public:
    MEInstancedMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MEInstancedMeshActor(const MEInstancedMeshActor& prototype);
    virtual ~MEInstancedMeshActor() = default;

public:
    Shared<ModelCom> GetModel() const { return _model; }

    void Set_CameraFar(float camfar) { _cameraFar = camfar; }

public:
    virtual HRESULT Initialize(void* arg) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ModelCom> _model;
    Shared<ShaderCom> _shader;
    ComPtr<Buffer> _instanceVB;
    uint32 _instanceCount = {};
    uint32 _instanceCapacity = {};
    float _cameraFar = 1000.f;

    wstring _loadedModelPath = {};

    Shared<ComputeShaderCom> _cullShader;
    Shared<ComputeStructuredBuffer> _instanceSrcBuffer;
    Shared<ComputeStructuredBuffer> _sphereBuffer;
    ComPtr<Buffer> _visibleVB;
    ComPtr<UnorderedAccessView> _visibleUAV;
    ComPtr<Buffer> _cullCB;
    ComPtr<Buffer> _countStaging;
    uint32 _visibleCount = {};

private:
    HRESULT CreateInstanceBuffer(const vector<Matrix>& matrices);
    void GenerateInstances(const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed, vector<MESHINSTANCEDATA>& outInstances);
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();

    HRESULT CreateCullBuffers(const vector<Matrix>& matrices, const vector<InstanceTransform>& transforms);
    void DispatchCull();
    static array<Vec4, 6> ExtractFrustumPlanes(const Matrix& vp);

public:
    HRESULT UpdateInstances(const wstring& modelPath, const vector<InstanceTransform>& transforms);

public:
    static Shared<MEInstancedMeshActor> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
