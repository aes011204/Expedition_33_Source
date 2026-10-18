#include "BattleResultRecoder.h"
#include "CombatStatCom.h"

void BattleResultRecoder::Reset_ForBattle()
{
    _victoryStats = {};
}

void BattleResultRecoder::Update(float unscaledDletaTime)
{
    _victoryStats.elapsedSec += max(0.f, unscaledDletaTime);
}

void BattleResultRecoder::Record_DamageApplied(const DamageAppliedInfo& info)
{
    if (info.damage <= 0.f || info.target.statCom == nullptr)
        return;

    const uint32 damageValue = static_cast<uint32>(roundf(info.damage));

    if (info.attacker.statCom && info.attacker.statCom->Is_Player())
    {
        _victoryStats.totalDamageDealt += damageValue;
        _victoryStats.maxDamage = max(_victoryStats.maxDamage, damageValue);
    }

    if (info.target.statCom->Is_Player())
    {
        _victoryStats.totalDamageTaken += damageValue;
    }
}

void BattleResultRecoder::Record_ReactionSucceeded(BattleReactionType reactionType)
{
    if (reactionType == BattleReactionType::Parry)
    {
        ++_victoryStats.parrySuccessCount;
        LOG_INFO("[Parry] Battle parry success count={}", _victoryStats.parrySuccessCount);
    }
    else if (reactionType == BattleReactionType::Dodge)
    {
        ++_victoryStats.dodgeSuccessCount;
    }
}

Shared<BattleResultRecoder> BattleResultRecoder::Create()
{
    return make_shared<BattleResultRecoder>();
}

void BattleResultRecoder::Free()
{
    Reset_ForBattle();

    Base::Free();
}
