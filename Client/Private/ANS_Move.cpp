#include "ANS_Move.h"

#include <GameObject.h>
#include <ModelCom.h>
#include "BattleController.h"
#include "ClientInstance.h"
#include "Character_BattleBase.h"
#include "BattleSpawnRegistry.h"
#include "TransformCom.h"
#include "Monster_Part.h"

void ANS_Move::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    auto& action = battle->Get_TurnAction();

    _moveTarget = nullptr;
    _startPos = {};
    _goalPos = {};
    _startRot = {};
    _goalRot = {};
    _isCounter = false;

    if (action.Get_CurChar().ModelCom.get() != context.model)
    {
        const Shared<BattleSpawnRegistry> spawnRegistry = battle->Get_BattleSpawnRegistry();
        if (spawnRegistry == nullptr)
            return;

        // context.model에 해당하는 battle actor를 찾아야 함
        const BattleChar* counterTarget = nullptr;

        for (const BattleChar& target : battle->Get_BattleHeroes())
        {
            if (target.ModelCom.get() == context.model)
            {
                counterTarget = &target;
                break;
            }
        }

        if (counterTarget == nullptr)
            return;

        if (counterTarget->actor == nullptr)
            return;

        const BattleSpawnRegistry::BattleEntry* spawnEntry =
            spawnRegistry->Find_SpawnEntry(counterTarget->actor);
        if (spawnEntry == nullptr)
            return;

        _moveTarget = counterTarget->actor->Get_Transform();
        if (_moveTarget == nullptr)
            return;

        _startPos = _moveTarget->Get_Position();
        _goalPos = spawnEntry->spawnPosition;

        _startRot = _moveTarget->Get_RotationEuler();
        _goalRot = spawnEntry->spawnRotation;

        _isCounter = true;
    }
    else
    {
        const BattleChar& curChar = action.Get_CurChar();
        if (curChar.actor == nullptr || curChar.ModelCom == nullptr)
            return;

        curChar.ModelCom->Set_RootMotionEnabled(false);

        _moveTarget = curChar.actor->Get_Transform();
        if (_moveTarget == nullptr)
            return;

        _startPos = _moveTarget->Get_Position();
        _goalPos = action.GetPhaseGoalPos();

        _startRot = _moveTarget->Get_RotationEuler();
        _goalRot = _startRot;

        const auto& phases = action.GetSkillData().AnimPhases;
        if (action.GetCurAnimIndex() < phases.size())
        {
            const BattleActionPhase& phase = phases[action.GetCurAnimIndex()];
            if (phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
                _goalRot = action.GetOriginRot();
        }

        _isCounter = false;
    }
}

void ANS_Move::On_Tick(const AnimNotifyContext& context)
{

    ////
    if (context.isPreview || context.isCinematic)
        return;

    if (_moveTarget == nullptr)
        return;

    const float curProgress = context.notifyStateProgress;
    _moveTarget->Set_Position(Vec3::Lerp(_startPos, _goalPos, curProgress));


    float startYaw = _startRot.y;
    float goalYaw = _goalRot.y;

    float deltaYaw = goalYaw - startYaw;
    while (deltaYaw > 180.f)
        deltaYaw -= 360.f;
    while (deltaYaw < -180.f)
        deltaYaw += 360.f;

    Vec3 nextRot = _startRot;
    nextRot.y = startYaw + deltaYaw * curProgress;

    _moveTarget->Set_RotationEuler(nextRot);
}

void ANS_Move::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    if (_moveTarget == nullptr)
        return;

    _moveTarget->Set_Position(_goalPos);

    Vec3 finalRot = _startRot;
    finalRot.y = _goalRot.y;
    _moveTarget->Set_RotationEuler(finalRot);
}
