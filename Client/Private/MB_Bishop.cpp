#include "MB_Bishop.h"
#include "ClientInstance.h"
#include "Mon_Bishop_Body.h"
#include "Mon_Lancelier_Weapon.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MB_Bishop)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Bishop";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Battle RunLoop Move Speed", _battleRunLoopMoveSpeed, 0.f, 30.f, 0.1f);

    return true;
}

MB_Bishop::MB_Bishop(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_Bishop::MB_Bishop(const MB_Bishop& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_Bishop::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Bishop::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);

    if (_combatStat)
        _combatStat->Get_MonsterDesc().isBoss = true;

    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    //_body->Set_Animation("Bishop_Idle", 0.3f, true);

    //_combatStat->Set_Skill("Eveque_Projectiles");
    _combatStat->Set_Skill("Eveque_SingleSpear");
    _combatStat->Set_Skill("Eveque_DoubleSpear");
    //_combatStat->Set_Skill("Eveque_Defense");
    _combatStat->Set_Skill("Eveque_GroundAoE");
    _combatStat->Set_Skill("Eveque_EnragedCombo");
    _combatStat->Set_Skill("Eveque_Spawn_Mini");

    return S_OK;
}

void MB_Bishop::BeginPlay()
{
    Monster_BattleBase::BeginPlay();

    // GroundAoe가 프레임드랍나서 미리 준비해놓기
    if (CLIENT != nullptr)
        CLIENT->Prewarm_Effect(L"Beshop_Ice", L"Layer_Effect", 9);
}

void MB_Bishop::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    if (_body)
    {
        _body->Play_Animation(timeDelta);
    }
}

bool MB_Bishop::Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const
{
    if (_body == nullptr)
        return false;

    const Matrix* socketMatrix = _body->Get_SocketBoneMatrixPtr("Aim_Target");
    if (socketMatrix == nullptr)
        return false;

    const Matrix socketWorldMatrix = *socketMatrix * _body->Get_CombinedWorldMatrix();
    outWorldPos = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
    return true;
}

HRESULT MB_Bishop::Ready_PartObjects()
{
    Mon_Bishop_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Bishop";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Bishop_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Bishop_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);


    Mon_Lancelier_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Lancelier_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("WeaponAttachPointR");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Lancelier_Weapon"), TEXT("Part_WeaponR"), &weaponDesc), E_FAIL);

    _weaponR = dynamic_pointer_cast<Mon_Lancelier_Weapon>(_partObjects[TEXT("Part_WeaponR")]);
    CHECK_NULL(_weaponR, E_FAIL);

    _weaponR->Set_Visible(false);

  //  Mon_Lancelier_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Lancelier_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("WeaponAttachPointL");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Lancelier_Weapon"), TEXT("Part_WeaponL"), &weaponDesc), E_FAIL);

    _weaponL = dynamic_pointer_cast<Mon_Lancelier_Weapon>(_partObjects[TEXT("Part_WeaponL")]);
    CHECK_NULL(_weaponL, E_FAIL);

    _weaponL->Set_Visible(false);

    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_body->Get_CombinedWorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketName = "Aim_Target";
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Aim_Target");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL);

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weak, E_FAIL);

    return S_OK;
}

Shared<MB_Bishop> MB_Bishop::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Bishop>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Bishop");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Bishop::Clone(void* arg)
{
    auto clone = make_shared<MB_Bishop>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Bishop");
        return nullptr;
    }

    return clone;
}

void MB_Bishop::Free()
{
    Monster_BattleBase::Free();
}
