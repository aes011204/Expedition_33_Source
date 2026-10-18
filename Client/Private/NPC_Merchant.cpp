#include "NPC_Merchant.h"
#include "ClientInstance.h"
#include "NPC_Body.h"

IMPLEMENT_REFLECTION(NPC_Merchant)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "NPC_Merchant";
    info.category = "NPC";
    NPC_Base::Register_CommonReflectionProperties(info);

    return true;
}

NPC_Merchant::NPC_Merchant(const ComPtr<Device>& device, const ComPtr<Context>& context) : NPC_Base(device, context)
{
    _worldMoveSpeedScale = 0.1f;
    _defaultAnimationKey = "Merchant_BagPack_Idle";
}

NPC_Merchant::NPC_Merchant(const NPC_Merchant& prototype) : NPC_Base(prototype)
{
}

HRESULT NPC_Merchant::Initialize_Prototype()
{
    return NPC_Base::Initialize_Prototype();
}

HRESULT NPC_Merchant::Initialize(void* arg)
{
    CHECK_FAILED(NPC_Base::Initialize(arg), E_FAIL);
    return S_OK;
}

void NPC_Merchant::Priority_Update(float timeDelta)
{
    NPC_Base::Priority_Update(timeDelta);
}

void NPC_Merchant::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    NPC_Base::Update(timeDelta);
}

HRESULT NPC_Merchant::Render()
{
    return S_OK;
}

HRESULT NPC_Merchant::Render_Shadow()
{
    return S_OK;
}

void NPC_Merchant::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    NPC_Base::OnBeginOverlap(self, other);
}

HRESULT NPC_Merchant::Ready_Components()
{
    NPC_Base::Ready_Components();
    return S_OK;
}

HRESULT NPC_Merchant::Ready_PartObjects()
{
    NPC_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Merchant";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Merchant_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<NPC_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    return S_OK;
}

Shared<NPC_Merchant> NPC_Merchant::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Merchant>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : NPC_Merchant");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> NPC_Merchant::Clone(void* arg)
{
    auto clone = make_shared<NPC_Merchant>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Merchant::Free()
{
    NPC_Base::Free();
}
