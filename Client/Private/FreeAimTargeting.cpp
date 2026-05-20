#include "FreeAimTargeting.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "BattleTurnCamera.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "PhysX_Types.h"
#include "GameObject.h"
#include "Character.h"

FreeAimTargeting::FreeAimTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

HRESULT FreeAimTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void FreeAimTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool FreeAimTargeting::EnterInvoke()
{
    Clear_HitTarget();

    return Battle_State::EnterInvoke();
}

void FreeAimTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    Refresh_HitTarget();

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        Clear_HitTarget();
        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::LBUTTON))
    {
        auto& turnAction = owner->Get_TurnAction();
        turnAction.target = _currentHitTarget;
        turnAction.type = EBattleContext::FreeAimShoot;

        // 일단은 테스트
        const Skill_Data* data = CLIENT->Get_Skill(turnAction._curChar.statCom->Get_FirstSkill());
        if (!data)
            return;

        turnAction._selected_SkillKey = *data;

        owner->ExecuteAction(turnAction);
        return;
    }
}

void FreeAimTargeting::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void FreeAimTargeting::ExitInvoke()
{
    Clear_HitTarget();

    Battle_State::ExitInvoke();
}

bool FreeAimTargeting::Try_FindHitEnemy(BattleChar& outEnemy) const
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    const auto battleCamera = owner->Get_BattleTurnCamera();
    if (battleCamera == nullptr)
        return false;

    const Vec3 originPos = battleCamera->Get_AimOrigin();
    Vec3 direction = battleCamera->Get_AimDirection();
    if (direction.LengthSquared() <= 0.0001f)
        return false;

    direction.Normalize();

    PhysXQueryHit hit{};
    const bool hasHit = GAME->Sweep_Sphere(
        originPos, direction, 100.f, 0.15f, Collision_Preset::Player_Target, hit);

    if (!hasHit || hit.owner == nullptr)
        return false;

    const vector<BattleChar> aliveEnemies = owner->Get_AliveEnemies();
    for (const auto& enemy : aliveEnemies)
    {
        if (enemy.actor == nullptr || enemy.statCom == nullptr)
            continue;

        if (enemy.actor.get() == hit.owner)
        {
            outEnemy = enemy;
            return true;
        }
    }

    return false;
}

void FreeAimTargeting::Refresh_HitTarget()
{
    BattleChar hitEnemy{};
    if (!Try_FindHitEnemy(hitEnemy))
    {
        Clear_HitTarget();
        return;
    }

    _currentHitTarget = hitEnemy;
    _hasCurrentHitTarget = true;

    if (auto owner = Get_Owner())
        owner->Set_TargetPreview(hitEnemy);

}

void FreeAimTargeting::Clear_HitTarget()
{
    _currentHitTarget = {};
    _hasCurrentHitTarget = false;

    if (auto owner = Get_Owner())
        owner->Clear_TargetPreview();
}

Shared<FreeAimTargeting> FreeAimTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<FreeAimTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

void FreeAimTargeting::Free()
{
    Battle_State::Free();
}
