#pragma once
#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class ComputeShaderCom;
class ComputeStructuredBuffer;
NS_END

NS_BEGIN(Client)

struct InstanceTransform
{
    Vec3 _position;
    Vec4 _rotation;
    Vec3 _scale;
};

class MapInstancedMesh final : public GameObject
{
    GENERATED_GAMEOBJECT(MapInstancedMesh, ObjectType::StaticMesh)

public:
    typedef struct tagMapInstancedMeshDesc final : public GameObject::GAMEOBJECT_DESC
    {
        Shared<ModelCom> model;
        wstring modelPath = {};
        vector<InstanceTransform> transforms;
    } MAPINSTANCEDMESH_DESC;

public:
    MapInstancedMesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MapInstancedMesh(const MapInstancedMesh& prototype);
    virtual ~MapInstancedMesh() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT RenderPickingID(uint32 id) override;
    HRESULT Render_Shadow() override;

    virtual json To_Json() const override;
    virtual void From_Json(const json& node) override;

private:
    Shared<ShaderCom> _shader;
    Shared<ModelCom> _model;
    ComPtr<Buffer> _instanceVB;
    uint32 _instanceCount = {};
    uint32 _instanceCapacity = {};

    wstring _modelPath = {};
    vector<InstanceTransform> _transforms;

    Shared<ComputeShaderCom> _cullShader;
    Shared<ComputeStructuredBuffer> _instanceSrcBuffer;
    Shared<ComputeStructuredBuffer> _sphereBuffer;
    ComPtr<Buffer> _visibleStructured;
    ComPtr<UnorderedAccessView> _visibleUAV;
    ComPtr<Buffer> _visibleVB;
    ComPtr<Buffer> _cullCB;
    vector<ComPtr<Buffer>> _indirectArgsBuffers;
    BoundingBox _groupBounds = {};

private:
    HRESULT Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel);
    HRESULT CreateInstanceBuffer(const vector<Matrix>& matrices);
    HRESULT Bind_ShaderResources();

    HRESULT CreateCullBuffers(const vector<Matrix>& matrices, const vector<InstanceTransform>& transforms);
    void DispatchCull();
    static array<Vec4, 6> ExtractFrustumPlanes(const Matrix& vp);

public:
    static Shared<MapInstancedMesh> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
