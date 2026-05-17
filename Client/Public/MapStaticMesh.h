#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "PhysX_Handle.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class MapStaticMesh final : public GameObject
{
    GENERATED_GAMEOBJECT(MapStaticMesh, ObjectType::StaticMesh)

public:
    typedef struct tagMapStaticMeshDesc final : public GameObject::GAMEOBJECT_DESC
    {
        Shared<ModelCom> model = nullptr;
        wstring modelPath = {};
        Vec3 position = Vec3::Zero;
        Quat rotation = Quat::Identity;
        Vec3 scale = Vec3::One;
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

private:
    Shared<ShaderCom>   _shader;
    Shared<ModelCom>    _model;

    PhysXStaticActorHandle _physxStaticHandle{};

private:
    HRESULT Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel);
    HRESULT Bind_ShaderResources();

public:
    static Shared<MapStaticMesh> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject>   Clone(void* arg) override;
    virtual void Free() override;

};

NS_END
