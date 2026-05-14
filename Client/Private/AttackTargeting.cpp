#include "pch.h"
#include "AttackTargeting.h"

#include "Input_Manager.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "Battle_Enum.h"
#include "PlayerStatCom.h"

NS_BEGIN(Client)

    AttackTargeting::AttackTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

AttackTargeting::~AttackTargeting()
{
}

HRESULT AttackTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void AttackTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool AttackTargeting::EnterInvoke()
{
    LOG_DEBUG("AttackTargeting::EnterInvoke");

    LOG_DEBUG("TARGETTING {}", TargetIndex);

    return Battle_State::EnterInvoke();
}

void AttackTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();



    if (!owner || !battleFSM)
        return;



    if (GAME->KeyDown(KEY_TYPE::KEY_1))
    {
        owner->Get_TurnAction().target = owner->Get_Enemies()[TargetIndex];
        owner->Get_TurnAction()._selected_SkillKey = dynamic_pointer_cast<PlayerStatCom>(owner->Get_TurnAction()._curChar.statCom)->Get_Basic_Skill();
        LOG_DEBUG("AttackTargeting: confirm target index={}", TargetIndex);
        LOG_DEBUG("AttackTargeting: selected skill={}", owner->Get_TurnAction()._selected_SkillKey.scriptClassName.c_str());

        owner->ExecuteAction(owner->Get_TurnAction());
     
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::KEY_2))
    {
        TargetIndex = (TargetIndex+1 )% owner->Get_EnemiesNum();
        LOG_DEBUG("TARGETTING {}", TargetIndex);

        // 임시 기본 공격은 무조건 적 공격 이라는 가정 / 동료 치료 불가
        return;
    }

}

void AttackTargeting::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void AttackTargeting::ExitInvoke()
{
    LOG_DEBUG("AttackTargeting::ExitInvoke");
    Battle_State::ExitInvoke();
}

void AttackTargeting::Free()
{
    Battle_State::Free();
}

Shared<AttackTargeting> AttackTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<AttackTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
