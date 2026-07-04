#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "PhysX_Handle.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
struct StaticCollisionData;
NS_END

NS_BEGIN(Client)

class MapStaticMesh final : public GameObject
{
    GENERATED_GAMEOBJECT(MapStaticMesh, ObjectType::StaticMesh)

public:
    static constexpr size_t InvalidSourceStaticMeshIndex = static_cast<size_t>(-1);

    typedef struct tagMapStaticMeshDesc final : public GameObject::GAMEOBJECT_DESC
    {
        Shared<ModelCom> model = nullptr;
        Shared<StaticCollisionData> collisionData = nullptr;
        wstring name = {};
        wstring modelPath = {};
        Vec3 position = Vec3::Zero;
        Quat rotation = Quat::Identity;
        Vec3 scale = Vec3::One;
        size_t sourceStaticMeshIndex = InvalidSourceStaticMeshIndex;
        wstring sourceMapPath = {};
        bool visible = true;
        bool cullEnabled = true;
        bool castShadow = true;
    } MAPSTATICMESH_DESC;

public:
    MapStaticMesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MapStaticMesh(const MapStaticMesh& prototype);
    virtual ~MapStaticMesh() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    HRESULT Render_Shadow() override;

    virtual bool Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const override;
    virtual HRESULT RenderPickingID(uint32 id) override;
    json To_Json() const override;
    void From_Json(const json& data) override;

    Shared<ModelCom> Get_Model() const { return _model; }
    Shared<StaticCollisionData> Get_CollisionData() const { return _collisionData; }
    bool Has_SourceStaticMeshEntry() const { return _sourceStaticMeshIndex != InvalidSourceStaticMeshIndex; }
    size_t Get_SourceStaticMeshIndex() const { return _sourceStaticMeshIndex; }
    const wstring& Get_SourceStaticMeshId() const { return _sourceStaticMeshId; }
    const wstring& Get_SourceStaticMeshModelPath() const { return _sourceStaticMeshModelPath; }
    const wstring& Get_SourceMapPath() const { return _sourceMapPath; }
    bool Is_MapVisible() const { return _visible; }
    void Set_MapVisible(bool visible) { _visible = visible; }

private:
    Shared<ShaderCom>   _shader;
    Shared<ModelCom>    _model;

    PhysXStaticActorHandle _physxStaticHandle{};
    Shared<StaticCollisionData> _collisionData;
    bool _usesStaticCollision = false;
    bool _hasEclipseGlow = false;

    bool _visible = true;
    bool _cullEnabled = true;
    bool _castShadow = true;

    size_t _sourceStaticMeshIndex = InvalidSourceStaticMeshIndex;
    wstring _sourceStaticMeshId = {};
    wstring _sourceStaticMeshModelPath = {};
    wstring _sourceMapPath = {};

private:
    HRESULT Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel);
    HRESULT Ready_EmptyModelComponent();
    void Register_ModelComponent();
    HRESULT Bind_ShaderResources();
    void Refresh_HasEclipseGlow();

public:
    void Rebuild_StaticCollisionFromModel();

public:
    static Shared<MapStaticMesh> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject>   Clone(void* arg) override;
    virtual void Free() override;

};

NS_END
