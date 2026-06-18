#include "AN_BattleShot.h"
#include <GameObject.h>
#include "Battle_Event.h"
#include "GameInstance.h"
#include "Event_Bus.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "PlayableParts.h"

namespace
{
bool Is_BattleRosterModel(const BattleController& battle, const ModelCom* model)
{
    if (model == nullptr)
        return false;

    for (const BattleChar& hero : battle.Get_BattleHeroes())
    {
        if (hero.ModelCom.get() == model)
            return true;
    }

    for (const BattleChar& enemy : battle.Get_BattleEnemies())
    {
        if (enemy.ModelCom.get() == model)
            return true;
    }

    return false;
}
}

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
    if (context.isPreview)
        return;

    auto owner = context.owner;

    if (owner == nullptr || context.model == nullptr)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr || !battle->Is_BattleStarted())
        return;

    if (!Is_BattleRosterModel(*battle, context.model))
        return;

    auto character = dynamic_pointer_cast<Character>(owner->GetSharedPtr<PlayableParts>()->Get_Owner());

    if (character == nullptr)
        return;

    EvtBattlePresentation event{};
    event.cameraMode = _cameraMode;

    if (_cameraMode == BattleCameraMode::PARRY &&
        battle != nullptr &&
        battle->Get_ReactionController().Is_GroupParryActive())
    {
        return; // 단체 패링 중에는 ReactionController 상태를 보고 중복 Parry 막기
    }

    if( _cameraMode == BattleCameraMode::BATTLEINTRO)
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
            event.opponent = targets[centerIndex].actor;
        }

        if (action.GetCurAnimIndex() < action.GetSkillData().AnimPhases.size())
        {
            const Client::BattleActionPhase& phase = action.GetSkillData().AnimPhases[action.GetCurAnimIndex()];
            event.presetId = !_presetId.empty() ? _presetId : phase.cameraPresetId;
            event.clipId = !_clipId.empty() ? _clipId : phase.cameraClipId;
        }

    }
    event.useClip = _useClip;
    event.restartClip = _restartClip;
    event.keepCurrentClip = _keepCurrentClip;

    const string modeName = string(magic_enum::enum_name(_cameraMode));
    LOG_INFO(
        "[BattleCamTrace] AN_BattleShot animation={} timeSec={:.3f} mode={} prefab={} useClip={} presetId={} clipId={}",
        context.animationName.c_str(),
        context.currentTimeSec,
        modeName.c_str(),
        character->Get_SourcePrefabName().c_str(),
        _useClip,
        event.presetId.c_str(),
        event.clipId.c_str());

    GAME->Get_EventBus().Publish(event);

}
