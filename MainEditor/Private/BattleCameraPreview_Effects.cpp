#include "pch.h"
#include "BattleCameraPreview_View.h"

#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "ClientInstance.h"
#include "Client_Struct.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "PreviewObjectHelpers.h"

namespace
{
bool Is_PreviewNotifyTimeInRange(float targetTime, float previousTime, float currentTime, bool wrapped, bool includeStartEdge)
{
    if (!wrapped)
    {
        if (includeStartEdge)
            return previousTime <= targetTime + 1e-4f && targetTime <= currentTime + 1e-4f;

        if (previousTime <= 1e-4f && targetTime <= 1e-4f)
            return targetTime <= currentTime;

        return previousTime < targetTime && targetTime <= currentTime;
    }

    if (includeStartEdge)
        return targetTime >= previousTime - 1e-4f || targetTime <= currentTime + 1e-4f;

    return targetTime > previousTime || targetTime <= currentTime + 1e-4f;
}

bool Is_PreviewEffectNotifyStateType(const string& typeName)
{
    return typeName == "ANS_PlayEffect" ||
        typeName == "ANS_SourceHistoryEffect";
}

Vec3 JsonToVec3(const json& value, const Vec3& fallback = Vec3::Zero)
{
    if (!value.is_array() || value.size() < 3)
        return fallback;

    return Vec3(
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>());
}

}

namespace MainEditor
{
void BattleCameraPreview_View::Update_PreviewPlayEffectNotifies(float previousTimeSec, float currentTimeSec)
{
    Shared<ModelCom> model = Resolve_PreviewSubjectModel();
    Shared<GameObject> previewRootObject = Resolve_PreviewBattleActor(_previewContext.subjectSide);
    if (model == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewPlayEffectState();
        return;
    }

    const char* animationNamePtr = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
    if (animationNamePtr == nullptr || *animationNamePtr == '\0')
    {
        Reset_PreviewPlayEffectState();
        return;
    }

    const string animationName = animationNamePtr;
    const bool subjectChanged =
        _previewPlayEffectSubjectSide != _previewContext.subjectSide ||
        _previewPlayEffectSubjectSlot != _previewContext.subjectSlot;
    const bool animationChanged = _previewPlayEffectAnimationName != animationName;

    if (subjectChanged || animationChanged)
    {
        Reset_PreviewPlayEffectState();
        previousTimeSec = 0.f;
    }
    else
    {
        previousTimeSec = std::max(0.f, _previewPlayEffectPreviousTimeSec);
    }

    const AnimNotifyData* animationData = model->Find_AnimNotifyDataByState(model->Get_AnimationStateIndex());
    if (animationData == nullptr)
    {
        _previewPlayEffectAnimationName = animationName;
        _previewPlayEffectPreviousTimeSec = currentTimeSec;
        _previewPlayEffectSubjectSide = _previewContext.subjectSide;
        _previewPlayEffectSubjectSlot = _previewContext.subjectSlot;
        _previewPlayEffectIncludeStartEdge = false;
        return;
    }

    const bool rewound = currentTimeSec + 1e-4f < previousTimeSec;
    if (rewound)
    {
        Reset_PreviewPlayEffectState();
        _previewPlayEffectAnimationName = animationName;
        _previewPlayEffectPreviousTimeSec = currentTimeSec;
        _previewPlayEffectSubjectSide = _previewContext.subjectSide;
        _previewPlayEffectSubjectSlot = _previewContext.subjectSlot;
        return;
    }

    const bool includeStartEdge = _previewPlayEffectIncludeStartEdge;
    const bool timeAdvanced = includeStartEdge || currentTimeSec > previousTimeSec;

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = model.get();
    context.animationName = animationName;
    context.previousTimeSec = previousTimeSec;
    context.currentTimeSec = currentTimeSec;
    context.deltaTime = std::max(0.f, currentTimeSec - previousTimeSec);
    context.isPreview = true;

    if (timeAdvanced)
    {
        for (const AnimNotifyEventEntry& entry : animationData->notifies)
        {
            if (entry.notify == nullptr)
                continue;
            const string typeName = entry.notify->Get_TypeName();
            if (typeName != "AN_PlayEffect" &&
                typeName != "AN_PlaySound" &&
                typeName != "AN_SpawnLineGroundEffect")
                continue;
            if (!Is_PreviewNotifyTimeInRange(entry.timeSec, previousTimeSec, currentTimeSec, false, includeStartEdge))
                continue;

            if (typeName == "AN_SpawnLineGroundEffect")
            {
                Shared<GameObject> subjectObject = Resolve_PreviewBattleActor(_previewContext.subjectSide);
                Shared<GameObject> opponentObject = Resolve_PreviewBattleActor(_previewContext.opponentSide);
                Shared<TransformCom> subjectTransform = subjectObject ? subjectObject->Get_Transform() : nullptr;
                Shared<TransformCom> opponentTransform = opponentObject ? opponentObject->Get_Transform() : nullptr;
                if (subjectTransform == nullptr || opponentTransform == nullptr)
                    continue;

                const json payload = entry.notify->To_Json();
                const wstring effectName = String::ToWString(payload.value("_effectName", string{}));
                const wstring layerTag = String::ToWString(payload.value("_layerTag", string("Layer_Effect")));
                const uint32 segmentNum = payload.value("_segmentNum", 4u);
                const uint32 segmentIndex = payload.value("_segmentIndex", 1u);

                if (effectName.empty() || segmentNum <= 1 || segmentIndex == 0)
                    continue;

                Vec3 attackerPos = subjectTransform->Get_WorldPosition();
                Vec3 targetPos = opponentTransform->Get_WorldPosition();
                targetPos.y = attackerPos.y;

                Vec3 dir = targetPos - attackerPos;
                const float length = dir.Length();
                if (length <= 0.0001f)
                    continue;

                dir.Normalize();

                const float segment = length / static_cast<float>(segmentNum - 1);
                const Vec3 spawnPos = attackerPos + dir * (static_cast<float>(segmentIndex) * segment);

                float scaleValue = 1.2f;
                if (segmentIndex == 1)
                    scaleValue = 0.6f;
                else if (segmentIndex == 2)
                    scaleValue = 0.9f;
                else if (segmentIndex == 3)
                    scaleValue = 1.2f;

                Client::PlayEffectDesc desc{};
                desc.effectName = effectName;
                desc.layerTag = layerTag;
                desc.worldPosition = spawnPos;
                desc.localOffset = JsonToVec3(payload["_localOffset"]);
                desc.localRotation = JsonToVec3(payload["_localRotation"]);
                desc.scale = Vec3(scaleValue, scaleValue, scaleValue);
                CLIENT->Play_Effect(desc);
                continue;
            }

            entry.notify->Execute(context);
        }
    }

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (!Is_PreviewEffectNotifyStateType(entry.notifyState->Get_TypeName()))
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStateIds.push_back(entry.id);

        AnimNotifyContext stateContext = context;
        stateContext.notifyStateDurationSec = entry.durationSec;
        stateContext.notifyStateProgress = entry.durationSec > 0.f
            ? std::clamp((currentTimeSec - entry.startSec) / entry.durationSec, 0.f, 1.f)
            : 1.f;
        const string payloadJson = entry.notifyState->To_Json().dump();

        PreviewPlayEffectStateRuntime* runtime = nullptr;
        for (PreviewPlayEffectStateRuntime& candidate : _previewPlayEffectStateRuntimes)
        {
            if (candidate.stateId == entry.id)
            {
                runtime = &candidate;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewPlayEffectStateRuntime newRuntime{};
            newRuntime.stateId = entry.id;
            newRuntime.notifyState = entry.notifyState;
            newRuntime.payloadJson = payloadJson;
            _previewPlayEffectStateRuntimes.push_back(newRuntime);

            entry.notifyState->On_Begin(stateContext);
            continue;
        }

        if (runtime->notifyState != entry.notifyState ||
            runtime->payloadJson != payloadJson)
        {
            if (runtime->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtime->notifyState->On_End(endContext);
            }

            runtime->notifyState = entry.notifyState;
            runtime->payloadJson = payloadJson;
            entry.notifyState->On_Begin(stateContext);
            continue;
        }

        if (timeAdvanced)
            entry.notifyState->On_Tick(stateContext);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewPlayEffectStateRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewPlayEffectStateRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewPlayEffectStateRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            _previewPlayEffectStateRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewPlayEffectStateRuntimes.erase(_previewPlayEffectStateRuntimes.begin() + runtimeIndex);
    }

    _previewPlayEffectAnimationName = animationName;
    _previewPlayEffectPreviousTimeSec = currentTimeSec;
    _previewPlayEffectIncludeStartEdge = false;
    _previewPlayEffectSubjectSide = _previewContext.subjectSide;
    _previewPlayEffectSubjectSlot = _previewContext.subjectSlot;
}

void BattleCameraPreview_View::Reset_PreviewPlayEffectState()
{
    Shared<ModelCom> model = Resolve_PreviewSubjectModel();
    Shared<GameObject> previewRootObject = Resolve_PreviewBattleActor(_previewContext.subjectSide);

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = model.get();
    context.notifyStateProgress = 1.f;
    context.isPreview = true;

    for (PreviewPlayEffectStateRuntime& runtime : _previewPlayEffectStateRuntimes)
    {
        if (runtime.notifyState && context.owner != nullptr)
            runtime.notifyState->On_End(context);
    }

    _previewPlayEffectStateRuntimes.clear();
    _previewPlayEffectAnimationName.clear();
    _previewPlayEffectPreviousTimeSec = 0.f;
    _previewPlayEffectIncludeStartEdge = true;
    _previewPlayEffectSubjectSide = _previewContext.subjectSide;
    _previewPlayEffectSubjectSlot = _previewContext.subjectSlot;

    PreviewObjectHelpers::Stop_AttachedPreviewEffects(_previewHeroObject);
    PreviewObjectHelpers::Stop_AttachedPreviewEffects(_previewEnemyObject);
    PreviewObjectHelpers::Stop_AttachedPreviewEffects(_previewNpcObject);
}
}
