#include "MB_PotatoBag.h"
#include "Mon_PotatoBag_Body.h"
#include "Mon_PotatoBag_Weapon.h"
#include "Mon_PotatoBag_Shield.h"
#include "Mon_WeakPoint.h"
#include "MonsterStatCom.h"
#include "EffectCom.h"
#include "WeaponPrefabLoader.h"
#include "ModelCom.h"

IMPLEMENT_REFLECTION(MB_PotatoBag)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_PotatoBag";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);

    return true;
}

MB_PotatoBag::MB_PotatoBag(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_PotatoBag::MB_PotatoBag(const MB_PotatoBag& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_PotatoBag::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_PotatoBag::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    //_combatStat->Set_Skill("PotatoBag_AttShield");
    //_combatStat->Set_Skill("PotatoBag_AttWeapon");
    //_combatStat->Set_Skill("PotatoBag_AttAoE");
    //_combatStat->Set_Skill("PotatoBag_AttShieldGun");
    //_combatStat->Set_Skill("PotatoBag_Lighting");
    //_combatStat->Set_Skill("PotatoBag_DefenseMod");

    Get_Transform()->Set_SpeedPerSec(4.f);

    return S_OK;
}

void MB_PotatoBag::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    if (_body)
    {
        _body->Play_Animation(timeDelta);
        Apply_NoShieldArmBreakPose();
    }
}

bool MB_PotatoBag::Try_Consume_FreeAimWeakPoint()
{
    if (!Monster_BattleBase::Try_Consume_FreeAimWeakPoint())
        return false;

    Enter_NoShieldPhase();

    return true;
}

bool MB_PotatoBag::Try_ApplyBattleBreakOnDamage(const DamageAppliedInfo& info, bool canTriggerBreak)
{
    return Character_BattleBase::Try_ApplyBattleBreakOnDamage(info, canTriggerBreak);
}

Monster_BattleBase::BattlePhase MB_PotatoBag::Get_CurPhase() const
{
    if (_curPhase == NoShield)
        return BattlePhase::Critical;

    if (Get_HpRatio() > 0.9f)
        return BattlePhase::Opening;

    if (_curPhase == Defense)
        return BattlePhase::Mid;

    return BattlePhase::Opening;
}

bool MB_PotatoBag::Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const
{
    return partTag != TEXT("Part_Weapon");
}
bool MB_PotatoBag::Enter_NoShieldPhase()
{
    if (_curPhase == NoShield)
        return false;

    if (_shield)
    {
        if (const auto effectCom = _shield->Get_Component<EffectCom>())
            effectCom->Start_VanishDissolve(.5f, "T_Perlin_Noise_M", true);
    }

    _curPhase = NoShield;
    Hide_FreeAimWeakPoint();
    Apply_NoShieldArmBreakPose();

    return true;
}

void MB_PotatoBag::Apply_NoShieldArmBreakPose()
{
    if (_curPhase != NoShield || _body == nullptr)
        return;

    const Shared<ModelCom> model = _body->Get_Model();
    if (model == nullptr)
        return;

    const int32 upperArmIndex = model->Get_BoneIndex("upperarm_l");
    if (upperArmIndex < 0)
        return;

    model->Set_BoneLocalMatrix(static_cast<uint32>(upperArmIndex), Matrix::CreateScale(0.f));
    model->Rebuild_BoneCombinedMatrices();
}

HRESULT MB_PotatoBag::Ready_PartObjects()
{
    Mon_PotatoBag_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_PotatoBag";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_PotatoBag_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_PotatoBag_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    WeaponPrefabLoader::LoadedWeaponPrefab loadedPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::PotatoBag_Weapon, loadedPrefab, "Mon_PotatoBag_Weapon"))
        return E_FAIL;

    Mon_PotatoBag_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = loadedPrefab.modelType;
    weaponDesc.modelTag = loadedPrefab.modelTag;
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_NULL(weaponDesc.socketMatrix, E_FAIL);
    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_PotatoBag_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

    _weapon = dynamic_pointer_cast<Mon_PotatoBag_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);
    _weapon->From_Json(loadedPrefab.rootJson);
    _weapon->Set_SourcePrefabName(Constants::Prefab::PotatoBag_Weapon);

    Mon_PotatoBag_Shield::SHIELD_DESC shieldDesc{};
    shieldDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    shieldDesc.modelType = ModelType::NonAnim;
    shieldDesc.modelTag = L"Model_PotatoBag_Shield";
    shieldDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_L");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_PotatoBag_Shield"), TEXT("Part_Shield"), &shieldDesc), E_FAIL);

    _shield = dynamic_pointer_cast<Mon_PotatoBag_Shield>(_partObjects[TEXT("Part_Shield")]);
    CHECK_NULL(_shield, E_FAIL);

    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_body->Get_CombinedWorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketName = "upperarm_l";
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("upperarm_l");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL);

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weak, E_FAIL);

    return S_OK;
}

bool MB_PotatoBag::Should_AppendPreTurnEndPhase(const ActionRequest& request) const
{
    UNREFERENCED_PARAMETER(request);

    return _curPhase == Defense && Get_CurPhase() == BattlePhase::Mid;
}

Shared<MB_PotatoBag> MB_PotatoBag::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_PotatoBag>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_PotatoBag");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_PotatoBag::Clone(void* arg)
{
    auto clone = make_shared<MB_PotatoBag>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_PotatoBag");
        return nullptr;
    }

    return clone;
}

void MB_PotatoBag::Free()
{
    Monster_BattleBase::Free();
}
