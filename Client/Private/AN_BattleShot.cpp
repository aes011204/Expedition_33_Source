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

    auto character = dynamic_pointer_cast<Character>(owner->GetSharedPtr<PlayableParts>()->Get_Owner());

    if(character == nullptr)
        return;
    
    EvtBattlePresentation event {};
    event.cameraMode = _cameraMode;
    event.subject = character;
    if (battle)
        event.skillId = battle->Get_TurnAction().GetSkillData().skillKey;
    event.useClip = _useClip;
    event.restartClip = _restartClip;
    GAME->Get_EventBus().Publish(event);
}
