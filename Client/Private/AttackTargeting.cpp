#include "pch.h"
#include "AttackTargeting.h"
#include "Input_Manager.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "Battle_Enum.h"
#include "ClientInstance.h"
#include "PlayerStatCom.h"
#include "Battle_Event.h"
#include "Event_Bus.h"

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

    auto owner = Get_Owner();
    if (!owner)
        return false;

    TargetIndex = 0;

    const auto enemies = owner->Get_AliveEnemies();
    if (!enemies.empty())
    {
        TargetIndex %= enemies.size();
        owner->Set_TargetPreview(enemies[TargetIndex]);
    }


    return Battle_State::EnterInvoke();
}

void AttackTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    const auto enemies = owner->Get_AliveEnemies();
    if (enemies.empty())
        return;

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F))
    {
        owner->Set_TargetPreview(enemies[TargetIndex]);
        owner->Get_TurnAction().target = owner->Get_AliveEnemies()[TargetIndex];
        owner->Request_WidePresentationMode();  

        auto data = CLIENT->Get_Skill(owner->Get_TurnAction()._curChar.statCom->Get_FirstSkill());
        owner->Get_TurnAction()._selected_SkillKey = *data;

        LOG_DEBUG("AttackTargeting: confirm target index={}", TargetIndex);
        LOG_DEBUG("AttackTargeting: selected skill={}", owner->Get_TurnAction()._selected_SkillKey.scriptClassName.c_str());

        owner->ExecuteAction(owner->Get_TurnAction());
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::A))
    {
        // 왼쪽 끝
        if (Try_MoveTargetIndex(-1, enemies.size()) == false)
            return;

        owner->Set_TargetPreview(enemies[TargetIndex]);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::ENEMYFOCUS;
        event.target = enemies[TargetIndex].actor;
        GAME->Get_EventBus().Publish(event);

        LOG_DEBUG("TARGETTING {}", TargetIndex);

        // 임시 기본 공격은 무조건 적 공격 이라는 가정 / 동료 치료 불가
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::D))
    {
        // 오른쪽 끝
         if (Try_MoveTargetIndex(+1, enemies.size()) == false)
            return;

        owner->Set_TargetPreview(enemies[TargetIndex]);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::ENEMYFOCUS;
        event.target = enemies[TargetIndex].actor;
        GAME->Get_EventBus().Publish(event);

        LOG_DEBUG("TARGETTING {}", TargetIndex);
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

    if (auto owner = Get_Owner())
        owner->Clear_TargetPreview();

    Battle_State::ExitInvoke();
}

bool AttackTargeting::Try_MoveTargetIndex(int32 direction, size_t enemyCount)
{
    if (enemyCount == 0)
        return false;

    int32 nextIndex = static_cast<int32>(TargetIndex);

    while (true)
    {
        nextIndex += direction; // A면 왼쪽, D면 오른쪽

        // 범위 밖으로 나가면 막기
        if (nextIndex < 0 || nextIndex >= static_cast<int32>(enemyCount))
            return false;

        TargetIndex = static_cast<uint32>(nextIndex);
        return true;
    }
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
