#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MEInstancedMeshActor final
{
public:
    MEInstancedMeshActor() = default;
    ~MEInstancedMeshActor() = default;

public:
    HRESULT Initialize(ComPtr<Device> device, ComPtr<Context> context);
    HRESULT Render();

private:
    Shared<ModelCom> _model;
    Shared<ShaderCom> _shader;
    ComPtr<Device> _device;
    ComPtr<Context> _context;
    ComPtr<Buffer> _instanceVB;
    uint32 _instanceCount = {};
    uint32 _instanceCapacity = {};

    wstring _loadedModelPath = {};

private:
    HRESULT CreateInstanceBuffer(const vector<MESHINSTANCEDATA>& instances);
    void GenerateInstances(const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed, vector<MESHINSTANCEDATA>& outInstances);
    HRESULT Ready_Components();

public:
    HRESULT UpdateInstances(const wstring& modelPath, const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed);

public:
    static Shared<MEInstancedMeshActor> Create(ComPtr<Device> device, ComPtr<Context> context);
};

NS_END
