#pragma once
#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class InstancedMeshActor final : public GameObject
{
    GENERATED_GAMEOBJECT(InstancedMeshActor, ObjectType::Foliage)

public:
    typedef struct tagInstancedMeshDesc final : public GameObject::GAMEOBJECT_DESC
    {
        wstring modelPath = {};
        uint32 instanceCount = {};
        MESHINSTANCEDATA* instances = {};
    } INSTANCED_MESH_DESC;

public:
    InstancedMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    InstancedMeshActor(const InstancedMeshActor& rhs);
    virtual ~InstancedMeshActor() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void Priority_Update(float fTimeDelta) override;
    virtual void Update(float fTimeDelta) override;
    virtual void Late_Update(float fTimeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<ModelCom> _model;

    ComPtr<Buffer> _instanceVB;
    uint32 _instanceCount = {};
    uint32 _instanceCapacity = {};

private:
    HRESULT Ready_Components(const INSTANCED_MESH_DESC& desc);
    HRESULT Bind_ShaderResources();
    HRESULT CreateInstanceBuffer(const MESHINSTANCEDATA* instances, uint32 count);

public:
    HRESULT UpdateInstances(const MESHINSTANCEDATA* instances, uint32 count);

public:
    static Shared<InstancedMeshActor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* pArg) override;
    virtual void Free() override;
};

NS_END
