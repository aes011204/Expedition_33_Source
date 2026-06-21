#include "BattleQTEController.h"

#include "CombatStatCom.h"

HRESULT BattleQTEController::Initialize()
{
    Clear();
    return S_OK;
}

void BattleQTEController::Begin(const BattleQTERequest& request)
{
    _activeRequest = request;
    _lastResult = BattleQTEResult::None;
    _hasLastResult = false;
    _isActive = true;

    OnQTEStarted.Broadcast(_activeRequest);
}

bool BattleQTEController::Resolve(BattleQTEResult result)
{
    if (!_isActive)
        return false;

    if (!Is_ValidResult(result))
        return false;

    _isActive = false;
    _lastResult = result;
    _hasLastResult = true;
    _pendingNextAttackDamageBonusRate += Resolve_DamageBonusRate(result); // QTE가 여러 번 들어오면 다음 공격 보정에 누적한다.

    OnQTEResolved.Broadcast(result);
    return true;
}

bool BattleQTEController::FailIfActive()
{
    if (!_isActive)
        return false;

    return Resolve(BattleQTEResult::Fail);
}

void BattleQTEController::Clear()
{
    _activeRequest = {};
    _lastResult = BattleQTEResult::None;
    _isActive = false;
    _hasLastResult = false;
    _pendingNextAttackDamageBonusRate = 0.f;
}

bool BattleQTEController::Is_ValidResult(BattleQTEResult result) const
{
    return result == BattleQTEResult::Fail ||
        result == BattleQTEResult::SuccessNormal ||
        result == BattleQTEResult::SuccessPerfect;
}

bool BattleQTEController::Is_NextAttackDamageType(AttackType attackType) const
{
    switch (attackType)
    {
    case AttackType::BaseAttack:
    case AttackType::SingleSkill:
    case AttackType::SingleAOE:
    case AttackType::Combo:
    case AttackType::ComboLast:
    case AttackType::FreeAimLight:
    case AttackType::FreeAimWeak:
        return true;
    default:
        return false;
    }
}

float BattleQTEController::Resolve_DamageBonusRate(BattleQTEResult result) const
{
    switch (result)
    {
    case BattleQTEResult::Fail:
        return _activeRequest.failDamageBonusRate;
    case BattleQTEResult::SuccessNormal:
        return _activeRequest.normalDamageBonusRate;
    case BattleQTEResult::SuccessPerfect:
        return _activeRequest.perfectDamageBonusRate;
    default:
        return 0.f;
    }
}

float BattleQTEController::Consume_NextAttackDamageMultiplier(const BattleChar& attacker, AttackType attackType)
{
    if (_pendingNextAttackDamageBonusRate == 0.f)
        return 1.f;

    if (attacker.statCom == nullptr || !attacker.statCom->Is_Player())
        return 1.f;

    if (!Is_NextAttackDamageType(attackType))
        return 1.f;

    float multiplier = 1.f + _pendingNextAttackDamageBonusRate;
    if (multiplier < 0.f)
        multiplier = 0.f;

    _pendingNextAttackDamageBonusRate = 0.f; // 다음 유효 공격에 들어간 순간 누적 QTE 보정을 한 번만 소비한다.

    return multiplier;
}

Shared<BattleQTEController> BattleQTEController::Create()
{
    auto instance = make_shared<BattleQTEController>();

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("Failed to Created : BattleQTEController");
        return nullptr;
    }

    return instance;
}

void BattleQTEController::Free()
{
    OnQTEStarted.Clear();
    OnQTEResolved.Clear();

    Clear();
    Base::Free();
}
