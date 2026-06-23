#include "NPC_Noco.h"
#include "ClientInstance.h"
#include "NPC_Body.h"
#include "NPC_Hair.h"

IMPLEMENT_REFLECTION(NPC_Noco)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "NPC_Noco";
    info.category = "NPC";

    return true;
}

NPC_Noco::NPC_Noco(const ComPtr<Device>& device, const ComPtr<Context>& context) : NPC_Base(device, context)
{
    _worldMoveSpeedScale = 0.1f;
}

NPC_Noco::NPC_Noco(const NPC_Noco& prototype) : NPC_Base(prototype)
{
}

HRESULT NPC_Noco::Initialize_Prototype()
{
    return NPC_Base::Initialize_Prototype();
}

HRESULT NPC_Noco::Initialize(void* arg)
{
    CHECK_FAILED(NPC_Base::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);
    _body->Set_Animation("Noco", 0.3, true);

    return S_OK;
}

void NPC_Noco::Priority_Update(float timeDelta)
{
    NPC_Base::Priority_Update(timeDelta);
}

void NPC_Noco::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    NPC_Base::Update(timeDelta);
}

HRESULT NPC_Noco::Render()
{
    return S_OK;
}

HRESULT NPC_Noco::Render_Shadow()
{
    return S_OK;
}

void NPC_Noco::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    NPC_Base::OnBeginOverlap(self, other);
}

HRESULT NPC_Noco::Ready_Components()
{
    NPC_Base::Ready_Components();
    return S_OK;
}

HRESULT NPC_Noco::Ready_PartObjects()
{
    NPC_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Noco";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Noco_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<NPC_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    NPC_Hair::HEAD_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = L"Model_Noco_Hair";
    headDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Noco_Hair"), TEXT("Part_Hair"), &headDesc), E_FAIL);

    _hair = dynamic_pointer_cast<NPC_Hair>(_partObjects[TEXT("Part_Hair")]);
    CHECK_NULL(_hair, E_FAIL);

    return S_OK;
}

Shared<NPC_Noco> NPC_Noco::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Noco>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : NPC_Noco");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> NPC_Noco::Clone(void* arg)
{
    auto clone = make_shared<NPC_Noco>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Noco::Free()
{
    NPC_Base::Free();
}
