#pragma once
#include "MapEditorDefines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MEInstancedMeshActor final : public GameObject
{
public:
    MEInstancedMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MEInstancedMeshActor(const MEInstancedMeshActor& prototype);
    virtual ~MEInstancedMeshActor() = default;

public:
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

private:
    HRESULT CreateInstanceBuffer(const vector<MESHINSTANCEDATA>& instances);
    void GenerateInstances(const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed, vector<MESHINSTANCEDATA>& outInstances);
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();

public:
    HRESULT UpdateInstances(const wstring& modelPath, const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed);

public:
    static Shared<MEInstancedMeshActor> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
