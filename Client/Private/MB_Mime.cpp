#include "MB_Mime.h"
#include "Mon_Mime_Body.h"
#include "Mon_Mime_Bagutte.h"
#include "MonsterStatCom.h"
#include "Mon_WeakPoint.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "ModelCom.h"

namespace
{
    constexpr float kMimeUnbrokenDamageMultiplier = 0.3f; // 파괴 전에는 최종피해 감소
    constexpr float kMimeBreakDamageRate = 1.f;           // 데미지 1 : 파괴 게이지 1
}
    
IMPLEMENT_REFLECTION(MB_Mime)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Mime";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);

    return true;
}

MB_Mime::MB_Mime(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_Mime::MB_Mime(const MB_Mime& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_Mime::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Mime::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);

    _isWallDamageReductionActive = false;
    _hasTriggeredBreak = false;

    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    //_combatStat->Set_Skill("Mime_AttHamor_3");
    //_combatStat->Set_Skill("Mime_AttHand");
    //_combatStat->Set_Skill("Mime_Wall");
    //_combatStat->Set_Skill("Mime_AttHamor");

    return S_OK;
}

void MB_Mime::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    if (_body)
        _body->Play_Animation(timeDelta);
}

void MB_Mime::Adjust_IncomingBattleDamage(const BattleChar& attacker, float& damage, AttackType attackType) const
{
    UNREFERENCED_PARAMETER(attacker);
    UNREFERENCED_PARAMETER(attackType);

    if (damage <= 0.f || _combatStat == nullptr)
        return;

    if (_hasTriggeredBreak)
        return;

    if (!_isWallDamageReductionActive)
        return;

    // 파괴 한번 됐으면, 데미지 감소 미적용
    if (_combatStat->IsBreak())
        return;

    damage *= kMimeUnbrokenDamageMultiplier;
}

bool MB_Mime::Try_ApplyBattleBreakOnDamage(const DamageAppliedInfo& info, bool canTriggerBreak)
{
    if (_combatStat == nullptr || info.damage <= 0.f)
        return false;

    if (info.target.actor.get() != this)
        return false;

    if (_combatStat->Get_AliveStatus() == EAliveStatus::Dead)
        return false;

    if (_hasTriggeredBreak)
        return false;

    if (_combatStat->IsBreak())
        return false;

    _combatStat->Add_Break(info.damage * kMimeBreakDamageRate); // 실제로 깎인 HP 피해량만큼 파괴 게이지를 누적한다.

    if (_combatStat->Get_CurBreak() < _combatStat->Get_MaxBreak())
        return false;

    if (!canTriggerBreak)
        return false;

    _combatStat->SetBreak(true);
    _hasTriggeredBreak = true;
    _isWallDamageReductionActive = false;
    _combatStat->Set_Break(_combatStat->Get_MaxBreak());

    if (const auto battle = CLIENT->Get_BattleController())
        battle->Apply_Buff(info.target, Buffs::Stun, 1, 1);

    if (info.target.ModelCom != nullptr)
    {
        const bool playedBreakEnter =
            info.target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Stun_Enter), 0.2f, false);

        if (!playedBreakEnter)
            info.target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Idle_Stun), 0.2f, true); // Mime animmap에는 현재 Idle_Stun만 확인됨.
    }

    return true;
}

void MB_Mime::Start_WallDamageReduction()
{
    if (_hasTriggeredBreak)
        return;

    _isWallDamageReductionActive = true;
}

HRESULT MB_Mime::Ready_PartObjects()
{
    Mon_Mime_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Mime";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Mime_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Mime_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_Mime_Bagutte::WEAPON_DESC bagutteDesc{};
    bagutteDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bagutteDesc.modelType = ModelType::NonAnim;
    bagutteDesc.modelTag = L"Model_Mime_Bagutte";
    bagutteDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("headSocket");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Mime_Bagutte"), TEXT("Part_Bagutte"), &bagutteDesc), E_FAIL);

    _bagutte = dynamic_pointer_cast<Mon_Mime_Bagutte>(_partObjects[TEXT("Part_Bagutte")]);
    CHECK_NULL(_bagutte, E_FAIL);

   
    return S_OK;
}

Shared<MB_Mime> MB_Mime::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Mime>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Mime");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Mime::Clone(void* arg)
{
    auto clone = make_shared<MB_Mime>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Mime");
        return nullptr;
    }

    return clone;
}

void MB_Mime::Free()
{
    Monster_BattleBase::Free();
}
