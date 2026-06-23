#include "NPC_Sumo.h"
#include "ClientInstance.h"
#include "NPC_Body.h"
#include "NPC_Hair.h"

IMPLEMENT_REFLECTION(NPC_Sumo)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "NPC_Sumo";
    info.category = "NPC";

    return true;
}

NPC_Sumo::NPC_Sumo(const ComPtr<Device>& device, const ComPtr<Context>& context) : NPC_Base(device, context)
{
    _worldMoveSpeedScale = 0.1f;
}

NPC_Sumo::NPC_Sumo(const NPC_Sumo& prototype) : NPC_Base(prototype)
{
}

HRESULT NPC_Sumo::Initialize_Prototype()
{
    return NPC_Base::Initialize_Prototype();
}

HRESULT NPC_Sumo::Initialize(void* arg)
{
    CHECK_FAILED(NPC_Base::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);
    _body->Set_Animation("Sumo_Idle", 0.3, true);

    return S_OK;
}

void NPC_Sumo::Priority_Update(float timeDelta)
{
    NPC_Base::Priority_Update(timeDelta);
}

void NPC_Sumo::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    NPC_Base::Update(timeDelta);
}

HRESULT NPC_Sumo::Render()
{
    return S_OK;
}

HRESULT NPC_Sumo::Render_Shadow()
{
    return S_OK;
}

void NPC_Sumo::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    NPC_Base::OnBeginOverlap(self, other);
}

HRESULT NPC_Sumo::Ready_Components()
{
    NPC_Base::Ready_Components();
    return S_OK;
}

HRESULT NPC_Sumo::Ready_PartObjects()
{
    NPC_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Sumo";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Sumo_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<NPC_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    NPC_Hair::HEAD_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::NonAnim;
    headDesc.modelTag = L"Model_Sumo_Hair";
    headDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Sumo_Hair"), TEXT("Part_Hair"), &headDesc), E_FAIL);

    _hair = dynamic_pointer_cast<NPC_Hair>(_partObjects[TEXT("Part_Hair")]);
    CHECK_NULL(_hair, E_FAIL);

    return S_OK;
}

Shared<NPC_Sumo> NPC_Sumo::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Sumo>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : NPC_Sumo");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> NPC_Sumo::Clone(void* arg)
{
    auto clone = make_shared<NPC_Sumo>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Sumo::Free()
{
    NPC_Base::Free();
}
