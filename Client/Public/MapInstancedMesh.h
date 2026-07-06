#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "PhysX_Handle.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class ComputeShaderCom;
class ComputeStructuredBuffer;
struct StaticCollisionData;
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

private:
    static constexpr float INSTANCE_CULL_DIST = 125.f;
    static constexpr float LARGE_OBJECT_HEIGHT = 5.f;

public:
    typedef struct tagMapInstancedMeshDesc final : public GameObject::GAMEOBJECT_DESC
    {
        Shared<ModelCom> model;
        Shared<StaticCollisionData> collisionData = nullptr;
        wstring modelPath = {};
        wstring displayName = {};
        vector<InstanceTransform> transforms;
        bool visible = true;
        bool cullEnabled = true;
        bool castShadow = true;
        float cullDist = 125.f;
        bool hasCollision = true;
    } MAPINSTANCEDMESH_DESC;

public:
    MapInstancedMesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MapInstancedMesh(const MapInstancedMesh& prototype);
    virtual ~MapInstancedMesh() = default;

public:
    void Set_LodModel(int lod, Shared<ModelCom> model);

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT RenderPickingID(uint32 id) override;
    HRESULT Render_Shadow() override;
    virtual bool Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const override;
    virtual bool Allows_DynamicShadowCascade(uint32 cascadeIndex) const override;
    virtual uint32 Get_RenderHotspotInstanceCount() const override { return _instanceCount; }
    virtual wstring Get_RenderHotspotSourceModelPath() const override { return _modelPath; }
    virtual bool Get_RenderHotspotInstancedMeshStats(RenderHotspotInstancedMeshStats& outStats) const override;
    virtual bool Is_InstancedMeshShadowBakeable() const override { return true; }

    bool Is_MapVisible() const { return _visible; }
    void Set_MapVisible(bool visible) { _visible = visible; }

    virtual json To_Json() const override;
    virtual void From_Json(const json& node) override;

private:
    Shared<ShaderCom> _shader;
    Shared<ModelCom> _lodModels[3];
    Shared<StaticCollisionData> _collisionData;
    ComPtr<Buffer> _instanceVB;
    uint32 _instanceCount = {};
    uint32 _instanceCapacity = {};

    wstring _modelPath = {};
    vector<InstanceTransform> _transforms;

    Shared<ComputeShaderCom> _cullShader;
    Shared<ComputeStructuredBuffer> _instanceSrcBuffer;
    Shared<ComputeStructuredBuffer> _sphereBuffer;
    ComPtr<Buffer> _visibleStructured[3];
    ComPtr<UnorderedAccessView> _visibleUAV[3];
    ComPtr<Buffer> _visibleVB[3];
    ComPtr<Buffer> _cullCB;
    vector<ComPtr<Buffer>> _indirectArgsBuffers[3];
    ComPtr<Buffer> _debugVisibleCountReadbackBuffers[3];
    array<uint32, 3> _debugLastDrawInstanceCounts = {};
    uint32 _debugLastVisibleCountReadbackRequestId = 0;
    bool _debugHasDrawInstanceCounts = false;
    ComPtr<Buffer> _potatoBagArenaInstanceVB;
    uint32 _potatoBagArenaInstanceCount = 0;
    uint32 _potatoBagArenaInstanceBufferRevision = 0;
    bool _potatoBagArenaInstanceBufferReady = false;
    vector<PhysXStaticActorHandle> _physxStaticHandles;
    BoundingBox _groupBounds = {};

    // Shadow compute shader
    Shared<ComputeShaderCom> _shadowCullShader;
    ComPtr<Buffer> _shadowCullCB;
    ComPtr<Buffer> _shadowVisibleStructured;
    ComPtr<UnorderedAccessView> _shadowVisibleUAV;
    ComPtr<Buffer> _shadowVisibleVB;
    vector<ComPtr<Buffer>> _shadowIndirectArgsBuffers[3];

    float _lod1Dist = 80.f;
    float _lod2Dist = 110.f;

    float _accTime = {};
    bool _hasGlow = false;
    bool _visible = true;   
    bool _cullEnabled = true;
    bool _castShadow = true;
    float _cullDist = 125.f;

    uint32 _maxShadowCascade = 3u;

private:
    HRESULT Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel);
    HRESULT CreateInstanceBuffer(const vector<Matrix>& matrices);
    HRESULT Prepare_PotatoBagArenaInstanceBuffer();
    HRESULT Bind_ShaderResources();

    HRESULT CreateCullBuffers(const vector<Matrix>& matrices, const vector<InstanceTransform>& transforms);
    HRESULT CreateShadowIndirectArgsBuffers();
    void ResetShadowIndirectArgsBuffers();
    uint32 Resolve_ShadowLodIndex(uint32 cascadeIndex) const;
    void DispatchCull();
    bool Readback_DebugVisibleCounts(uint32 requestId);
    void DispatchShadowCull(uint32 cascadeIndex, uint32 shadowLod);
    static array<Vec4, 6> ExtractFrustumPlanes(const Matrix& vp);
    void Refresh_HasGlow();

public:
    static Shared<MapInstancedMesh> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
