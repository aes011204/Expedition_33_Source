#include "MB_Volester.h"

#include "Mon_Volester_Body.h"
#include "Mon_Lancelier_Weapon.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"

IMPLEMENT_REFLECTION(MB_Volester)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Volester";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);

    return true;
}

MB_Volester::MB_Volester(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_Volester::MB_Volester(const MB_Volester& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_Volester::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Volester::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

   // _body->Set_Animation("Volester_Idle", 0.3f, true);

    //_combatStat->Set_Skill("Volester_SlowShot");
    //_combatStat->Set_Skill("Volester_SpeedShot");


    return S_OK;
}

void MB_Volester::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    if (_body)
    {
        _body->Play_Animation(timeDelta);
    }
}



HRESULT MB_Volester::Ready_PartObjects()
{
    Mon_Volester_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Volester";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Volester_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Volester_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    return S_OK;
}

Shared<MB_Volester> MB_Volester::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Volester>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Volester");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Volester::Clone(void* arg)
{
    auto clone = make_shared<MB_Volester>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Volester");
        return nullptr;
    }

    return clone;
}

void MB_Volester::Free()
{
    Monster_BattleBase::Free();
}
