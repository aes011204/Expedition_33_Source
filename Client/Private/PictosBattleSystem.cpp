#include "PictosBattleSystem.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "PlayerStatCom.h"

namespace
{
    constexpr const char* kPictosEnergisingTurn = "EnergisingTurn";
    constexpr const char* kPictosGlassCannon = "GlassCannon";
    constexpr const char* kPictosWarmingUp = "WarmingUp";
    constexpr const char* kPictosRoulette = "Roulette";
    constexpr const char* kPictosAugmentedAttack = "AugmentedAttack";
    constexpr const char* kPictosAugmentedAim = "AugmentedAim";
}

HRESULT PictosBattleSystem::Initialize()
{
    return S_OK;
}

void PictosBattleSystem::Clear_RuntimeState()
{
    _warmingUpDamageBonusByStat.clear();
    _rouletteDamageMultiplierByStat.clear();
    _pendingActivations.clear();
}

uint32 PictosBattleSystem::Resolve_TurnStartApBonus(const BattleChar& actor) const
{
    if (actor.statCom == nullptr || !actor.statCom->Is_Player())
        return 0;

    return Has_EquippedPictosId(actor.statCom, kPictosEnergisingTurn) ? 1u : 0u;
}

void PictosBattleSystem::ProcessTurnStart(const BattleChar& actor)
{
    if (actor.statCom == nullptr || !actor.statCom->Is_Player())
        return;

    CombatStatCom* statKey = actor.statCom.get();

    // 활력의 턴
    if (Has_EquippedPictosId(actor.statCom, kPictosEnergisingTurn))
        Queue_Activation(actor, kPictosEnergisingTurn);

    // 예열
    if (Has_EquippedPictosId(actor.statCom, kPictosWarmingUp))
    {
        _warmingUpDamageBonusByStat[statKey] += 5.f;
        Queue_Activation(actor, kPictosWarmingUp);
    }

    // 룰렛
    if (Has_EquippedPictosId(actor.statCom, kPictosRoulette))
    {
        const bool highRoll = GAME->Random(0.f, 1.f) < 0.5f;
        _rouletteDamageMultiplierByStat[statKey] = highRoll ? 1.5f : 0.5f;
        Queue_Activation(actor, kPictosRoulette);
    }
    else
    {
        _rouletteDamageMultiplierByStat.erase(statKey);
    }
}

vector<PictosBattleSystem::PictosActivationEvent> PictosBattleSystem::Consume_PendingActivations()
{
    vector<PictosActivationEvent> result = std::move(_pendingActivations);
    _pendingActivations.clear();

    return result;
}

void PictosBattleSystem::Queue_Activation(const BattleChar& actor, const string& pictosId)
{
    if (actor.heroId == HeroId::END || pictosId.empty())
        return;

    const auto duplicated = find_if(
        _pendingActivations.begin(),
        _pendingActivations.end(),
        [&](const PictosActivationEvent& event)
        {
            return event.heroId == actor.heroId && event.pictosId == pictosId;
        });

    if (duplicated != _pendingActivations.end())
    {
        ++duplicated->activationCount;
        return;
    }

    PictosActivationEvent event{};
    event.heroId = actor.heroId;
    event.pictosId = pictosId;
    event.displayName = Resolve_PictosDisplayName(pictosId);

    _pendingActivations.push_back(event);
}

wstring PictosBattleSystem::Resolve_PictosDisplayName(const string& pictosId) const
{
    if (const auto* desc = CLIENT->Get_PictosDesc(pictosId))
        return desc->name;

    return wstring(pictosId.begin(), pictosId.end());
}

float PictosBattleSystem::Resolve_DamageMultiplier(const BattleChar& attacker, const BattleChar& target,
    AttackType attackType)
{
    if (!Is_PictosBaseDamageType(attackType))
        return 1.f;

    float multiplier = 1.f;

    // 플레이어만 적용
    if (attacker.statCom && attacker.statCom->Is_Player())
    {
        // 유리 대포
        if (Has_EquippedPictosId(attacker.statCom, kPictosGlassCannon))
        {
            multiplier += 0.25f;
            Queue_Activation(attacker, kPictosGlassCannon);
        }

        // 증강 공격
        if (attackType == AttackType::BaseAttack &&
            Has_EquippedPictosId(attacker.statCom, kPictosAugmentedAttack))
        {
            multiplier += 0.5f;
            Queue_Activation(attacker, kPictosAugmentedAttack);
        }

        // 증강 조준
        if ((attackType == AttackType::FreeAimLight || attackType == AttackType::FreeAimWeak) &&
            Has_EquippedPictosId(attacker.statCom, kPictosAugmentedAim))
        {
            multiplier += 0.5f;
            Queue_Activation(attacker, kPictosAugmentedAim);
        }

        // 예열
        if (auto it = _warmingUpDamageBonusByStat.find(attacker.statCom.get()); it != _warmingUpDamageBonusByStat.end())
            multiplier += it->second * 0.01f;

        // 룰렛
        if (auto it = _rouletteDamageMultiplierByStat.find(attacker.statCom.get()); it != _rouletteDamageMultiplierByStat.end())
            multiplier *= it->second;
    }

    // 유리대포 -> 받는 피해
    if (target.statCom && target.statCom->Is_Player() &&
        Has_EquippedPictosId(target.statCom, kPictosGlassCannon))
    {
        multiplier *= 1.25f;
        Queue_Activation(target, kPictosGlassCannon);
    }

    return multiplier;
}

bool PictosBattleSystem::Has_EquippedPictosId(const Shared<CombatStatCom>& statCom, const string& pictosId) const
{
    const Shared<PlayerStatCom> playerStat = dynamic_pointer_cast<PlayerStatCom>(statCom);
    if (playerStat == nullptr)
        return false;

    const auto& equippedIds = playerStat->Get_EquippedPictosIds();
    return find(equippedIds.begin(), equippedIds.end(), pictosId) != equippedIds.end();
}

bool PictosBattleSystem::Is_PictosBaseDamageType(AttackType attackType) const
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

Shared<PictosBattleSystem> PictosBattleSystem::Create()
{
    auto instance = make_shared<PictosBattleSystem>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : PictosBattleSystem");
        return nullptr;
    }

    return instance;
}

void PictosBattleSystem::Free()
{
    _warmingUpDamageBonusByStat.clear();
    _rouletteDamageMultiplierByStat.clear();
    _pendingActivations.clear();

    Base::Free();
}
