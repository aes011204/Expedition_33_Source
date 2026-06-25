#include "BattleAnimutils.h"

#include "ModelCom.h"
#include "BattleMechanismCom.h"
#include "Buff.h"
#include "CombatStatCom.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "MB_PotatoBag.h"

NS_BEGIN(Client)

namespace
{
    bool Has_BattleBuff(const BattleChar& actor, Buffs buffId)
    {
        if (actor.statCom == nullptr)
            return false;

        for (const auto& buff : actor.statCom->Get_Buff())
        {
            if (buff == nullptr)
                continue;

            if (buff->Is_PendingRemove())
                continue;

            if (buff->Get_BuffId() == buffId)
                return true;
        }

        return false;
    }
}

namespace BattleAnimUtils
{
    BattleAnimType Resolve_BattleIdleAnimType(const BattleChar& actor)
    {
        // 스턴 부여돼있으면 스턴 (파괴)
        if (Has_BattleBuff(actor, Buffs::Stun))
            return BattleAnimType::Idle_Stun;

        const auto battle = CLIENT->Get_BattleController();
        if (battle != nullptr && battle->Is_BattleStarted() && actor.statCom && actor.statCom->Is_Player())
        {
            const BattleChar& turnActor = battle->Get_TurnActionRequest().actor;

            // 자기 턴이 아닌 플레이어는 기본 idle 대신 Waiting을 사용한다.
            if (turnActor.actor != nullptr && turnActor.actor != actor.actor)
                return BattleAnimType::Waiting;

        }

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

        if (auto potato = dynamic_pointer_cast<MB_PotatoBag>(actor.actor))
        {
            switch (potato->Get_CurPhase())
            {
            case Monster_BattleBase::BattlePhase::Opening:
                return BattleAnimType::Base_Idle;

            case Monster_BattleBase::BattlePhase::Mid:
                return BattleAnimType::Defensive_Idle;

            case Monster_BattleBase::BattlePhase::Critical:
                return BattleAnimType::Virtuose_Idle;

            default:
                return BattleAnimType::Base_Idle; 
            }
        }


        return BattleAnimType::Base_Idle; // 마엘이 아니면 기존 규칙 유지
    }

    void Play_BattleIdle(const BattleChar& actor, float blendTime)
    {
        if (actor.ModelCom == nullptr)
            return;

        BattleAnimType idleType = Resolve_BattleIdleAnimType(actor);

        if (idleType == BattleAnimType::Waiting ||
            idleType == BattleAnimType::Base_Idle)
        {
            const float roll = GAME->Random(0.f, 1.f);

            if (roll < 0.75f)
            {
                const int pick = static_cast<int>(GAME->Random(0.f, 3.f));
                const BattleAnimType lookAroundType =
                    pick == 0 ? BattleAnimType::Waiting_LookAround_0 :
                    pick == 1 ? BattleAnimType::Waiting_LookAround_1 :
                    BattleAnimType::Waiting_LookAround_2;

                if (actor.ModelCom->Set_AnimationByState(ETOI(lookAroundType), blendTime, false))
                    return;
            }
        }

        actor.ModelCom->Set_AnimationByState(
            ETOI(idleType),
            blendTime,
            true);
    }

    bool Play_TurnStartOrBattleIdle(const BattleChar& actor, float blendTime)
    {
        if (actor.ModelCom == nullptr)
            return false;

        if (actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Turn_Start), blendTime, false))
            return true;

        Play_BattleIdle(actor, blendTime);
        return false;
    }
}

NS_END
