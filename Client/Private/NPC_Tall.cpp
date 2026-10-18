#include "NPC_Tall.h"
#include "ClientInstance.h"
#include "NPC_Body.h"
#include "NPC_Hair.h"

IMPLEMENT_REFLECTION(NPC_Tall)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "NPC_Tall";
    info.category = "NPC";
    NPC_Base::Register_CommonReflectionProperties(info);

    return true;
}

NPC_Tall::NPC_Tall(const ComPtr<Device>& device, const ComPtr<Context>& context) : NPC_Base(device, context)
{
    _worldMoveSpeedScale = 0.1f;
    _defaultAnimationKey = "Gestral_Tall";
}

NPC_Tall::NPC_Tall(const NPC_Tall& prototype) : NPC_Base(prototype)
{
}

HRESULT NPC_Tall::Initialize_Prototype()
{
    return NPC_Base::Initialize_Prototype();
}

HRESULT NPC_Tall::Initialize(void* arg)
{
    CHECK_FAILED(NPC_Base::Initialize(arg), E_FAIL);
    return S_OK;
}

void NPC_Tall::Priority_Update(float timeDelta)
{
    NPC_Base::Priority_Update(timeDelta);
}

void NPC_Tall::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    NPC_Base::Update(timeDelta);
}

HRESULT NPC_Tall::Render()
{
    return S_OK;
}

HRESULT NPC_Tall::Render_Shadow()
{
    return S_OK;
}

void NPC_Tall::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    NPC_Base::OnBeginOverlap(self, other);
}

HRESULT NPC_Tall::Ready_Components()
{
    NPC_Base::Ready_Components();
    return S_OK;
}

HRESULT NPC_Tall::Ready_PartObjects()
{
    NPC_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Tall";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Tall_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<NPC_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    NPC_Hair::HEAD_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::NonAnim;
    headDesc.modelTag = L"Model_Tall_Hair";
    headDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Tall_Hair"), TEXT("Part_Hair"), &headDesc), E_FAIL);

    _hair = dynamic_pointer_cast<NPC_Hair>(_partObjects[TEXT("Part_Hair")]);
    CHECK_NULL(_hair, E_FAIL);

    return S_OK;
}

Shared<NPC_Tall> NPC_Tall::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Tall>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : NPC_Tall");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> NPC_Tall::Clone(void* arg)
{
    auto clone = make_shared<NPC_Tall>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Tall::Free()
{
    NPC_Base::Free();
}
