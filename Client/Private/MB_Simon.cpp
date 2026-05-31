#include "MB_Simon.h"

#include "Mon_Simon_Body.h"
#include "Mon_Simon_Weapon.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"

IMPLEMENT_REFLECTION(MB_Simon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Simon";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);

    return true;
}

MB_Simon::MB_Simon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_Simon::MB_Simon(const MB_Simon& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_Simon::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Simon::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    //_body->Set_Animation("Simon_Idle", 0.3f, true);



    return S_OK;
}

void MB_Simon::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    if (_body)
    {
        _body->Play_Animation(timeDelta);
    }
}



HRESULT MB_Simon::Ready_PartObjects()
{
    Mon_Simon_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Simon";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL); // 배틀 몬스터도 공용 바디 파츠 prototype은 Static에서 clone한다.

    _body = dynamic_pointer_cast<Mon_Simon_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_Simon_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Simon_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL); // 배틀 몬스터 무기 파츠도 Static prototype을 재사용한다.

    _weapon = dynamic_pointer_cast<Mon_Simon_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);

    return S_OK;
}

Shared<MB_Simon> MB_Simon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Simon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Simon");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Simon::Clone(void* arg)
{
    auto clone = make_shared<MB_Simon>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Simon");
        return nullptr;
    }

    return clone;
}

void MB_Simon::Free()
{
    Monster_BattleBase::Free();
}
