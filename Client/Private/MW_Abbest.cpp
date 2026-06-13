#include "MW_Abbest.h"
#include "ClientInstance.h"
#include "Mon_Abbest_Body.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MW_Abbest)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Abbest";
    info.category = "Monster";

    return true;
}

MW_Abbest::MW_Abbest(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_WorldBase(device, context)
{
    _worldMoveSpeedScale = 0.4f;
}

MW_Abbest::MW_Abbest(const MW_Abbest& prototype)
    : Monster_WorldBase(prototype)
{
}

HRESULT MW_Abbest::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Abbest::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Abbest::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Abbest::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Abbest::Render()
{
    return S_OK;
}

HRESULT MW_Abbest::Render_Shadow()
{
    return S_OK;
}

void MW_Abbest::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Abbest::Ready_Components()
{
    CHECK_FAILED(Monster_WorldBase::Ready_Components(), E_FAIL);
    return S_OK;
}

HRESULT MW_Abbest::Ready_PartObjects()
{
    Mon_Abbest_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Abbest";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Abbest_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Abbest_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_body->Get_CombinedWorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketName = "Eyes_Position";
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Eyes_Position");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL);

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weak, E_FAIL);

    return S_OK;
}

Shared<MW_Abbest> MW_Abbest::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Abbest>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Abbest");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Abbest::Clone(void* arg)
{
    auto clone = make_shared<MW_Abbest>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Abbest::Free()
{
    Monster_WorldBase::Free();
}
