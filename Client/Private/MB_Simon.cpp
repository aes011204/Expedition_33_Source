#include "MB_Simon.h"

#include "Mon_Simon_Body.h"
#include "Mon_Simon_Hair.h"
#include "Mon_Simon_Weapon.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"
#include "WeaponPrefabLoader.h"

IMPLEMENT_REFLECTION(MB_Simon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Simon";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Weapon Scale Multiplier", _weaponScaleMultiplier, 0.1f, 3.f, 0.01f);


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

    //_combatStat->Set_Skill("Simon_SwordGround");
    //_combatStat->Set_Skill("Simon_Debuff");
    //_combatStat->Set_Skill("Simon_Strong_Combo");
    //_combatStat->Set_Skill("Simon_Long_Combo");
    //_combatStat->Set_Skill("Simon_Short_Combo");
    //_combatStat->Set_Skill("Simon_Berserk");
    //_combatStat->Set_Skill("Simon_LightSpeed");
    //_combatStat->Set_Skill("Simon_Sword_Att");

    // 1페
    // Skill06 -> 지면에 꽂기
    // Skill08 -> 체력 1
    // Skill04 -> 강력한 콤보
    // Skill1  -> 긴 콤보 
    // Skill2  -> 짧은 콤보

    // 2페
    // Skill_5  -> 캠퍼스에서 제거
    // Skill07  -> 빛의 속도로 공격
    // Skill_3  -> 뤼미에르의 검이 공격

    return S_OK;
}

void MB_Simon::Update(float timeDelta)
{
    if (_body)
    {
        _body->Play_Animation(timeDelta);
    }

    Monster_BattleBase::Update(timeDelta);
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

    Mon_Simon_Hair::HEAD_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = L"Model_Simon_Hair";
    headDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("headSocket");
    headDesc.collisionModel = _body->Get_Model().get();
    headDesc.collisionOwnerWorldMatrix = &_body->Get_CombinedWorldMatrix();

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Hair"), TEXT("Part_Hair"), &headDesc), E_FAIL);

    _hair = dynamic_pointer_cast<Mon_Simon_Hair>(_partObjects[TEXT("Part_Hair")]);
    CHECK_NULL(_hair, E_FAIL);

	WeaponPrefabLoader::LoadedWeaponPrefab loadedPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::Simon_Weapon, loadedPrefab, "Mon_Simon_Weapon"))
        return E_FAIL;

    Mon_Simon_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = loadedPrefab.modelType;
    weaponDesc.modelTag = loadedPrefab.modelTag;
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_NULL(weaponDesc.socketMatrix, E_FAIL);
    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

    _weapon = dynamic_pointer_cast<Mon_Simon_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);

    _weapon->From_Json(loadedPrefab.rootJson);

    if (const Shared<TransformCom> weaponTransform = _weapon->Get_Transform())
    _weaponBaseScale = weaponTransform->Get_Scale();

    _weapon->Set_SourcePrefabName(Constants::Prefab::Simon_Weapon);

    return S_OK;
}

bool MB_Simon::Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const
{
    return partTag != TEXT("Part_Weapon");
}

void MB_Simon::From_Json(const json& data)
{
    Monster_BattleBase::From_Json(data);

    Apply_WeaponScaleMultiplier();
}

void MB_Simon::Apply_WeaponScaleMultiplier()
{
    if (!_weapon)
        return;

    const Shared<TransformCom> transform = _weapon->Get_Transform();
    if (!transform)
        return;

    const float multiplier = max(_weaponScaleMultiplier, 0.1f);
    transform->Set_Scale(Vec3(
        _weaponBaseScale.x * multiplier,
        _weaponBaseScale.y * multiplier,
        _weaponBaseScale.z * multiplier));
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
