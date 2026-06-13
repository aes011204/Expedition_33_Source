#include "MB_Abbest.h"
#include "Mon_WeakPoint.h"
#include "MonsterStatCom.h"

#include "Mon_Abbest_Body.h"

IMPLEMENT_REFLECTION(MB_Abbest)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Abbest";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);

    return true;
}

MB_Abbest::MB_Abbest(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_Abbest::MB_Abbest(const MB_Abbest& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_Abbest::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Abbest::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    if (Shared<ModelCom> model = _body ? _body->Get_Model() : nullptr)
        model->Set_Animation("Abbest_ENM_ElementalGrunt_IdleLooping2", true);


    _combatStat->Set_Skill("Abbest_Knife");

    Get_Transform()->Set_SpeedPerSec(4.f);

    return S_OK;
}

void MB_Abbest::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    if (_body)
        _body->Play_Animation(timeDelta);
}

HRESULT MB_Abbest::Ready_PartObjects()
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

Shared<MB_Abbest> MB_Abbest::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Abbest>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Abbest");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Abbest::Clone(void* arg)
{
    auto clone = make_shared<MB_Abbest>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Abbest");
        return nullptr;
    }

    return clone;
}

void MB_Abbest::Free()
{
    Monster_BattleBase::Free();
}
