#include "pch.h"
#include "SkillTargeting.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"

NS_BEGIN(Client)

SkillTargeting::SkillTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

SkillTargeting::~SkillTargeting()
{
}

HRESULT SkillTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void SkillTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool SkillTargeting::EnterInvoke()
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    _targetIndex = 0;

    const auto enemies = owner->Get_AliveEnemies();
    if (enemies.empty())
    {
        owner->Clear_TargetPreview();
        return false;
    }

    owner->Set_TargetPreview(enemies[_targetIndex]);

    return Battle_State::EnterInvoke();
}

void SkillTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    const auto enemies = owner->Get_AliveEnemies();
    if (enemies.empty())
    {
        owner->Clear_TargetPreview();
        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    if (_targetIndex >= enemies.size())
        _targetIndex = 0;

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        owner->Clear_TargetPreview();
        battleFSM->Change_State(BattleTurnState::ChooseSkill);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::A))
    {
        if (Try_MoveTargetIndex(-1, enemies.size()))
            owner->Set_TargetPreview(enemies[_targetIndex]);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::D))
    {
        if (Try_MoveTargetIndex(+1, enemies.size()))
            owner->Set_TargetPreview(enemies[_targetIndex]);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F))
    {
        owner->Set_TargetPreview(enemies[_targetIndex]);

        auto& action = owner->Get_TurnAction();
        action.target = enemies[_targetIndex]; // 여기서 선택 스킬 + 선택 타겟

        LOG_DEBUG("SkillTargeting: confirm skill={}, targetIndex={}",
            action._selected_SkillKey.skillKey.c_str(),
            _targetIndex);

        battleFSM->Change_State(BattleTurnState::AttackPlaying);
        owner->ExecuteAction(action);
        return;
    }
}

void SkillTargeting::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void SkillTargeting::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

bool SkillTargeting::Try_MoveTargetIndex(int32 direction, size_t enemyCount)
{
    if (enemyCount == 0)
        return false;

    const int32 nextIndex = static_cast<int32>(_targetIndex) + direction;
    if (nextIndex < 0 || nextIndex >= static_cast<int32>(enemyCount))
        return false; // 끝에서 더 넘어가지 않게 막는다.

    _targetIndex = static_cast<uint32>(nextIndex);
    return true;
}

void SkillTargeting::Free()
{
    Battle_State::Free();
}

Shared<SkillTargeting> SkillTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<SkillTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
