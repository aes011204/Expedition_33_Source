#include "NPC_Baby.h"
#include "ClientInstance.h"
#include "NPC_Body.h"
#include "NPC_Hair.h"

IMPLEMENT_REFLECTION(NPC_Baby)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "NPC_Baby";
    info.category = "NPC";
    NPC_Base::Register_CommonReflectionProperties(info);

    return true;
}

NPC_Baby::NPC_Baby(const ComPtr<Device>& device, const ComPtr<Context>& context) : NPC_Base(device, context)
{
    _worldMoveSpeedScale = 0.1f;
    _defaultAnimationKey = "Idle";
}

NPC_Baby::NPC_Baby(const NPC_Baby& prototype) : NPC_Base(prototype)
{
}

HRESULT NPC_Baby::Initialize_Prototype()
{
    return NPC_Base::Initialize_Prototype();
}

HRESULT NPC_Baby::Initialize(void* arg)
{
    CHECK_FAILED(NPC_Base::Initialize(arg), E_FAIL);
    return S_OK;
}

void NPC_Baby::Priority_Update(float timeDelta)
{
    NPC_Base::Priority_Update(timeDelta);
}

void NPC_Baby::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    NPC_Base::Update(timeDelta);
}

HRESULT NPC_Baby::Render()
{
    return S_OK;
}

HRESULT NPC_Baby::Render_Shadow()
{
    return S_OK;
}

void NPC_Baby::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    NPC_Base::OnBeginOverlap(self, other);
}

HRESULT NPC_Baby::Ready_Components()
{
    NPC_Base::Ready_Components();
    return S_OK;
}

HRESULT NPC_Baby::Ready_PartObjects()
{
    NPC_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Baby";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Baby_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<NPC_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    NPC_Hair::HEAD_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::NonAnim;
    headDesc.modelTag = L"Model_Baby_Hair";
    headDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Baby_Hair"), TEXT("Part_Hair"), &headDesc), E_FAIL);

    _hair = dynamic_pointer_cast<NPC_Hair>(_partObjects[TEXT("Part_Hair")]);
    CHECK_NULL(_hair, E_FAIL);

    return S_OK;
}

Shared<NPC_Baby> NPC_Baby::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Baby>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : NPC_Baby");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> NPC_Baby::Clone(void* arg)
{
    auto clone = make_shared<NPC_Baby>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Baby::Free()
{
    NPC_Base::Free();
}
