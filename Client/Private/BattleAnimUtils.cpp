#include "BattleAnimutils.h"

#include "ModelCom.h"
#include "BattleMechanismCom.h"

NS_BEGIN(Client)

namespace BattleAnimUtils
{
    BattleAnimType Resolve_BattleIdleAnimType(const BattleChar& actor)
    {
        if (actor.heroId == HeroId::Maelle && actor.mechanismCom != nullptr)
        {
            switch (actor.mechanismCom->Get_MaelleStance())
            {
            case BattleMechanismCom::MaelleStance::Offensive:
                return BattleAnimType::Offensive_Idle; // 공격 태세면 공격 idle

            case BattleMechanismCom::MaelleStance::Defensive:
                return BattleAnimType::Defensive_Idle; // 방어 태세면 방어 idle

            case BattleMechanismCom::MaelleStance::Virtuose:
                return BattleAnimType::Virtuose_Idle; // 명인 태세면 명인 idle

            case BattleMechanismCom::MaelleStance::Stanceless:
            default:
                return BattleAnimType::Base_Idle; // 태세 없음이면 기존 기본 idle
            }
        }

        return BattleAnimType::Base_Idle; // 마엘이 아니면 기존 규칙 유지
    }

    void Play_BattleIdle(const BattleChar& actor, float blendTime)
    {
        if (actor.ModelCom == nullptr)
            return;

        actor.ModelCom->Set_AnimationByState(
            ETOI(Resolve_BattleIdleAnimType(actor)),
            blendTime,
            true);
    }
}

NS_END
