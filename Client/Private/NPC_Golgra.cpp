#include "NPC_Golgra.h"
#include "ClientInstance.h"
#include "NPC_Body.h"
#include "NPC_Hair.h"

IMPLEMENT_REFLECTION(NPC_Golgra)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "NPC_Golgra";
    info.category = "NPC";

    return true;
}


NPC_Golgra::NPC_Golgra(const ComPtr<Device>& device, const ComPtr<Context>& context) :NPC_Base(device, context)
{
    _worldMoveSpeedScale = 0.1f;
}

NPC_Golgra::NPC_Golgra(const NPC_Golgra& prototype) : NPC_Base(prototype)
{
}

HRESULT NPC_Golgra::Initialize_Prototype()
{
    return NPC_Base::Initialize_Prototype();
}

HRESULT NPC_Golgra::Initialize(void* arg)
{
    CHECK_FAILED(NPC_Base::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);
    _body->Set_Animation("Idles_Golgra_V2", 0.3f, true);
    return S_OK;
}

void NPC_Golgra::Priority_Update(float timeDelta)
{
    NPC_Base::Priority_Update(timeDelta);
}

void NPC_Golgra::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    NPC_Base::Update(timeDelta);
}

HRESULT NPC_Golgra::Render()
{
    return S_OK;
}

HRESULT NPC_Golgra::Render_Shadow()
{
    return S_OK;
}

void NPC_Golgra::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    NPC_Base::OnBeginOverlap(self, other);
}

HRESULT NPC_Golgra::Ready_Components()
{
    NPC_Base::Ready_Components();
    return S_OK;
}

HRESULT NPC_Golgra::Ready_PartObjects()
{
    NPC_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Golgra";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Golgra_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<NPC_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    NPC_Hair::HEAD_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::NonAnim;
    headDesc.modelTag = L"Model_Golgra_Hair";
    headDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Golgra_Hair"), TEXT("Part_Hair"), &headDesc), E_FAIL);

    _hair = dynamic_pointer_cast<NPC_Hair>(_partObjects[TEXT("Part_Hair")]);
    CHECK_NULL(_hair, E_FAIL);

    return S_OK;
}

Shared<NPC_Golgra> NPC_Golgra::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Golgra>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : NPC_Golgra");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> NPC_Golgra::Clone(void* arg)
{
    auto clone = make_shared<NPC_Golgra>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Golgra::Free()
{
    NPC_Base::Free();
}
