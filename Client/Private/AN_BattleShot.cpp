#include "AN_BattleShot.h"
#include <GameObject.h>
#include "Battle_Event.h"
#include "GameInstance.h"
#include "Event_Bus.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "PlayableParts.h"

IMPLEMENT_REFLECTION(AN_BattleShot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("Camera Mode", _cameraMode, BattleCameraMode);
    PROPERTY_BOOL("Use Clip", _useClip);
    PROPERTY_BOOL("Restart Clip", _restartClip);
    PROPERTY_BOOL("Keep Current Clip", _keepCurrentClip);
    PROPERTY_STRING("Preset ID", _presetId);
    PROPERTY_STRING("Clip ID", _clipId);

    return true;
}

void AN_BattleShot::Execute(const AnimNotifyContext& context)
{
    auto owner = context.owner;

    if(owner == nullptr)
        return;

    auto battle = CLIENT->Get_BattleController();
    
    //LOG_DEBUG("Notify owner = {}",
    //    context.owner ? String::ToString(context.owner->Get_Owner()->Get_Name()).c_str() : "null");
    //
    //LOG_DEBUG("Current turn actor = {}",
    //    battle && battle->Get_TurnAction()._curChar.actor
    //        ? String::ToString(battle->Get_TurnAction()._curChar.actor->Get_Name()).c_str()
    //        : "null");

    if(_cameraMode == BattleCameraMode::WIDE)
    {
      int a = 10;
    }

    auto character = dynamic_pointer_cast<Character>(owner->GetSharedPtr<PlayableParts>()->Get_Owner());

    if(character == nullptr)
        return;

    EvtBattlePresentation event {};
        event.cameraMode = _cameraMode;

    if(_cameraMode == BattleCameraMode::PARRY && battle && battle->Is_GroupParryActive())
    {
         return;
    }
    event.subject = character;

    if (battle)
    {
        const auto& action = battle->Get_TurnAction();
        event.skillId = action.GetSkillData().skillKey;
        event.opponent = action.GetInitTarget().actor;
        const auto& steps = action.GetSteps();
        const uint32 curStepIndex = action.GetCurStepIndex();
        event.presetId = _presetId;
        event.clipId = _clipId;

        if (curStepIndex < steps.size() && !steps[curStepIndex].targets.empty())
        {
            const auto& targets = steps[curStepIndex].targets;
            const size_t centerIndex = targets.size() / 2;
            event.opponent =targets[centerIndex].actor;
        }

        if (action.GetCurAnimIndex() < action.GetSkillData().AnimPhases.size())
        {
            const Client::BattleActionPhase& phase = action.GetSkillData().AnimPhases[action.GetCurAnimIndex()];
             event.presetId = !_presetId.empty()? _presetId: phase.cameraPresetId;
            event.clipId = !_clipId.empty() ? _clipId : phase.cameraClipId;
        }

    }
    event.useClip = _useClip;
    event.restartClip = _restartClip;
    event.keepCurrentClip = _keepCurrentClip;
    GAME->Get_EventBus().Publish(event);
}
