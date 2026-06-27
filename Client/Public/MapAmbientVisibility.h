#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "PhysX_Handle.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class MapAmbientVisibility final : public GameObject
{
    GENERATED_GAMEOBJECT(MapAmbientVisibility, ObjectType::StaticMesh)

public:
    static constexpr size_t InvalidSourceStaticMeshIndex = static_cast<size_t>(-1);

    typedef struct tagMapAmbientVisibilityDesc final : public GameObject::GAMEOBJECT_DESC
    {
        Shared<ModelCom> model = nullptr;
        wstring name = {};
        wstring modelPath = {};
        Vec3 position = Vec3::Zero;
        Quat rotation = Quat::Identity;
        Vec3 scale = Vec3::One;
        size_t sourceStaticMeshIndex = InvalidSourceStaticMeshIndex;
        wstring sourceMapPath = {};
        float ambientVisibilityAmount = 0.15f;
        bool visible = true;
        bool cullEnabled = true;
    } MAP_AMBIENT_VISIBILITY_DESC;

public:
    MapAmbientVisibility(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MapAmbientVisibility(const MapAmbientVisibility& prototype);
    virtual ~MapAmbientVisibility() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

    virtual bool Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const override;
    virtual HRESULT RenderPickingID(uint32 id) override;
    json To_Json() const override;
    void From_Json(const json& data) override;

    Shared<ModelCom> Get_Model() const { return _model; }
    const wstring& Get_SourceMapPath() const { return _sourceMapPath; }

private:
    Shared<ShaderCom>   _shader;
    Shared<ModelCom>    _model;

    bool _visible = true;
    bool _cullEnabled = true;
    float _ambientVisibilityAmount = 0.15f;
    size_t _sourceStaticMeshIndex = InvalidSourceStaticMeshIndex;
    wstring _sourceStaticMeshId = {};
    wstring _sourceStaticMeshModelPath = {};
    wstring _sourceMapPath = {};

private:
    HRESULT Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel);
    void Register_ModelComponent();
    HRESULT Bind_ShaderResources();

public:
    static Shared<MapAmbientVisibility> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject>   Clone(void* arg) override;
    virtual void Free() override;

};

NS_END
