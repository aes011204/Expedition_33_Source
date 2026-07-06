#include "ANS_WeaponEcho.h"
#include "BattleController.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "Monster_BattleBase.h"
#include "Mon_Simon_Weapon.h"
#include "BattleAction.h"
#include "BattleReactionController.h"

namespace
{
    constexpr const wchar_t* kSimonEchoPhase2TrailEffectName = L"Simon_Trail_Phase2_Green";
    constexpr const wchar_t* kSimonEchoPhase3TrailEffectName = L"Simon_Trail_Phase3_Red";

    struct ActiveWeaponEchoWindow
    {
        const ANS_WeaponEcho* notifyState = nullptr;
        GameObject* owner = nullptr;
        GameObject* rootOwner = nullptr;
        string animationName{};
        float startSec = 0.f;
        float endSec = 0.f;
        float delaySec = 0.2f;
        uint32 echoCount = 1;
    };

    vector<ActiveWeaponEchoWindow> g_activeWeaponEchoWindows;

    GameObject* Resolve_RootNotifyOwner(GameObject* owner)
    {
        if (owner == nullptr)
            return nullptr;

        Shared<GameObject> current = owner->GetSharedPtr<GameObject>();
        Shared<GameObject> root = current;
        while (current)
        {
            root = current;
            current = current->Get_Owner();
        }

        return root.get();
    }
    bool Is_MatchingEchoWindow(const AnimNotifyContext& context, const ActiveWeaponEchoWindow& window)
    {
        GameObject* contextRootOwner = Resolve_RootNotifyOwner(context.owner);

        if (window.rootOwner != nullptr && contextRootOwner != nullptr)
        {
            if (window.rootOwner != contextRootOwner)
                return false;
        }
        else if (window.owner != context.owner)
        {
            return false;
        }

        if (!window.animationName.empty() &&
            !context.animationName.empty() &&
            window.animationName != context.animationName)
            return false;

        if (context.currentTimeSec + 1e-4f < window.startSec ||
            context.currentTimeSec > window.endSec + 1e-4f)
            return false;

        return true;
    }

    bool Is_LastAttackStep(const BattleAction& turnAction, uint32 stepIndex)
    {
        const Skill_Data& skillData = turnAction.GetSkillData();
        return !skillData.targetingType.empty() &&
            stepIndex + 1 >= skillData.targetingType.size();

    }

    wstring Resolve_SimonEchoTrailEffectName(const AnimNotifyContext& context, uint32 echoCount)
    {
        if (context.isPreview)
        {
            if (echoCount == 0)
                return {};

            return echoCount >= 2
                ? wstring(kSimonEchoPhase3TrailEffectName)
                : wstring(kSimonEchoPhase2TrailEffectName);
        }

        if (context.owner == nullptr)
            return {};

        Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
        while (current)
        {
            Monster_BattleBase* battleMonster = dynamic_cast<Monster_BattleBase*>(current.get());
            if (battleMonster != nullptr)
            {
                if (battleMonster->Get_SourcePrefabName() != Constants::Prefab::MB_Simon_V2)
                    return {};

                return battleMonster->Get_CurPhase() == Monster_BattleBase::BattlePhase::Critical
                    ? wstring(kSimonEchoPhase3TrailEffectName)
                    : wstring(kSimonEchoPhase2TrailEffectName);
            }

            current = current->Get_Owner();
        }

        return {};
    }
}


IMPLEMENT_REFLECTION(ANS_WeaponEcho)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("부위 태그", _partTag);
    PROPERTY_FLOAT("샘플 간격", _sampleInterval, 0.f, 0.2f, 0.001f);
    PROPERTY_FLOAT("지연 시간", _delaySec, 0.f, 1.0f, 0.01f);

    PROPERTY_INT("에코 개수 Override", _echoCountOverride, 0.f, 2.f, 1.f);

    PROPERTY_BOOL("Echo Material Override", _overrideEchoMaterial);
    PROPERTY_WSTRING("Ec1 Material Bundle", _echoCount1MaterialBundleKey);
    PROPERTY_WSTRING("Ec2 Material Bundle", _echoCount2MaterialBundleKey);

    return true;
}

void ANS_WeaponEcho::On_Begin(const AnimNotifyContext& context)
{
    _lastSampleElapsedSec = -1.f;
    _stateStartTimeSec = context.currentTimeSec;

    g_activeWeaponEchoWindows.erase(
        remove_if(
            g_activeWeaponEchoWindows.begin(),
            g_activeWeaponEchoWindows.end(),
            [this, &context](const ActiveWeaponEchoWindow& window)
            {
                return window.notifyState == this &&
                    window.owner == context.owner &&
                    window.animationName == context.animationName;
            }),
        g_activeWeaponEchoWindows.end());

    ActiveWeaponEchoWindow window{};
    window.notifyState = this;
    window.owner = context.owner;
    window.rootOwner = Resolve_RootNotifyOwner(context.owner);
    window.animationName = context.animationName;
    window.startSec = _stateStartTimeSec;
    window.endSec = _stateStartTimeSec + max(0.f, context.notifyStateDurationSec);
    window.delaySec = max(0.f, _delaySec);
    window.echoCount = Resolve_EchoCount(context);
    g_activeWeaponEchoWindows.push_back(window);

    if (!context.isPreview && window.echoCount > 0)
{
    auto battle = CLIENT->Get_BattleController();
    if (battle != nullptr)
    {
        const BattleAction& turnAction = battle->Get_TurnAction();
        const auto snapshots = battle->Get_ReactionController().Get_ActiveReactionWindowSnapshots();

        for (const auto& snapshot : snapshots)
        {
            const bool sourceStepIsLastAttack = Is_LastAttackStep(turnAction, snapshot.context.stepIndex);

            battle->Get_ReactionController().Suppress_CounterForSourceStep(snapshot.context.stepIndex);

            for (uint32 laneIndex = 0; laneIndex < window.echoCount; ++laneIndex)
            {
                const uint32 echoLaneIndex = laneIndex + 1;
                BattleReactionController::ReactionInputContext echoContext = snapshot.context;
                echoContext.isEcho = true;
                echoContext.consumeKey =
                    BattleReactionController::Make_ReactionConsumeKey(snapshot.context.stepIndex, echoLaneIndex);
                echoContext.allowCounterOnSuccess =
                    sourceStepIsLastAttack && laneIndex + 1 == window.echoCount;

                const float echoDelay = window.delaySec * static_cast<float>(echoLaneIndex);
                const float remainingDelay = max(0.f, echoDelay - snapshot.elapsedSec);
                const float sourceWindowDuration = snapshot.elapsedSec + snapshot.remainingSec;

                battle->Get_ReactionController().Queue_EchoReactionWindow(
                    remainingDelay,
                    sourceWindowDuration,
                    echoContext);
            }
        }
    }
}


    if (window.echoCount > 0)
    {
        if (Mon_Simon_Weapon* simonWeapon = Resolve_TargetSimonWeapon(context))
        {
            simonWeapon->Set_WeaponEchoTrailEffectOverride(Resolve_SimonEchoTrailEffectName(context, window.echoCount));

            Mon_Simon_Weapon::WeaponEchoMaterialOverrideDesc materialOverrides[2]{};
            uint32 materialOverrideCount = 0;

            if (_overrideEchoMaterial)
            {
                materialOverrideCount = std::min<uint32>(window.echoCount, 2u);
                for (uint32 laneIndex = 0; laneIndex < materialOverrideCount; ++laneIndex)
                {
                    const bool useSecondEchoMaterial = window.echoCount >= 2;
                    const wstring& bundleKey = useSecondEchoMaterial
                        ? _echoCount2MaterialBundleKey
                        : _echoCount1MaterialBundleKey;

                    materialOverrides[laneIndex].enabled = !bundleKey.empty();
                    materialOverrides[laneIndex].materialBundleKey = String::ToString(bundleKey);
                }
            }

            simonWeapon->Begin_WeaponEchoPlayback(
                _delaySec,
                window.echoCount,
                context.model,
                context.animationName,
                context.currentTimeSec,
                materialOverrideCount > 0 ? materialOverrides : nullptr,
                materialOverrideCount);

            if (Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner))
            {
                wstring activeTrailEffectName{};
                wstring activeTrailLayerTag{};
                EffectCom::TrailOffsetRuntimeDesc activeTrailOffset{};
                if (effectCom->Try_GetActiveTrailEffect(_partTag, activeTrailEffectName, activeTrailLayerTag, activeTrailOffset))
                    simonWeapon->Begin_WeaponEchoTrail(activeTrailEffectName, activeTrailLayerTag, activeTrailOffset);
            }
        }
    }

    Record_Sample(context, true);
}

void ANS_WeaponEcho::On_Tick(const AnimNotifyContext& context)
{
    Record_Sample(context, false);
}

void ANS_WeaponEcho::On_End(const AnimNotifyContext& context)
{
    Record_Sample(context, true);

    if (Mon_Simon_Weapon* simonWeapon = Resolve_TargetSimonWeapon(context))
        simonWeapon->End_WeaponEchoPlayback();

    g_activeWeaponEchoWindows.erase(
        remove_if(
            g_activeWeaponEchoWindows.begin(),
            g_activeWeaponEchoWindows.end(),
            [this, &context](const ActiveWeaponEchoWindow& window)
            {
                return window.notifyState == this &&
                    window.owner == context.owner &&
                    window.animationName == context.animationName;
            }),
        g_activeWeaponEchoWindows.end());

    _lastSampleElapsedSec = -1.f;
}

void ANS_WeaponEcho::Queue_EchoAttackSteps(const AnimNotifyContext& context, const BattleChar& attacker,
    const vector<BattleChar>& targets, const BattleChar& reactionTarget, uint32 sourceStepIndex, AttackType attackType,
    float powerMultiplier)
{
    if (context.isPreview || context.owner == nullptr || targets.empty())
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    for (const ActiveWeaponEchoWindow& window : g_activeWeaponEchoWindows)
    {
        if (!Is_MatchingEchoWindow(context, window))
            continue;

        for (uint32 laneIndex = 0; laneIndex < window.echoCount; ++laneIndex)
        {
            const uint32 echoLaneIndex = laneIndex + 1;
            const uint32 consumeKey =
                BattleReactionController::Make_ReactionConsumeKey(sourceStepIndex, echoLaneIndex);

            battle->Queue_DelayedEchoAttackStep(
                window.delaySec * static_cast<float>(echoLaneIndex),
                attacker,
                targets,
                reactionTarget,
                sourceStepIndex,
                consumeKey,
                attackType,
                powerMultiplier);
        }

        return;
    }
}

void ANS_WeaponEcho::Queue_EchoReactionWindowsFromNotify(const AnimNotifyContext& context,
    BattleReactionType reactionType, const BattleAction& turnAction, float sourceWindowDurationSec)
{
    if (context.isPreview || context.owner == nullptr)
        return;

    if (reactionType != BattleReactionType::Dodge &&
        reactionType != BattleReactionType::Parry)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    const uint32 sourceStepIndex = turnAction.GetCurStepIndex();
    const auto& targets = turnAction.GetCurStepTarget();
    if (targets.empty())
        return;

    for (const ActiveWeaponEchoWindow& window : g_activeWeaponEchoWindows)
    {
        if (!Is_MatchingEchoWindow(context, window))
            continue;

        if (window.echoCount == 0)
            return;

        const bool sourceStepIsLastAttack = Is_LastAttackStep(turnAction, sourceStepIndex);

        battle->Get_ReactionController().Suppress_CounterForSourceStep(sourceStepIndex);

        for (uint32 laneIndex = 0; laneIndex < window.echoCount; ++laneIndex)
        {
            const uint32 echoLaneIndex = laneIndex + 1;
            BattleReactionController::ReactionInputContext echoContext{};
            echoContext.type = reactionType;
            echoContext.stepIndex = sourceStepIndex;
            echoContext.consumeKey =
                BattleReactionController::Make_ReactionConsumeKey(sourceStepIndex, echoLaneIndex);
            echoContext.targets = targets;
            echoContext.counterTarget = turnAction.Get_CurChar();
            echoContext.isEcho = true;
            echoContext.allowCounterOnSuccess =
                sourceStepIsLastAttack && laneIndex + 1 == window.echoCount;

            battle->Get_ReactionController().Queue_EchoReactionWindow(
                window.delaySec * static_cast<float>(echoLaneIndex),
                sourceWindowDurationSec,
                echoContext);
        }

        return;
    }
}

void ANS_WeaponEcho::Clear_ActiveEchoWindows()
{
    g_activeWeaponEchoWindows.clear();
}

Mon_Simon_Weapon* ANS_WeaponEcho::Resolve_TargetSimonWeapon(const AnimNotifyContext& context) const
{
    if (context.owner == nullptr)
        return nullptr;

    Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
    while (current)
    {
        ContainerObject* ownerContainer = dynamic_cast<ContainerObject*>(current.get());
        if (ownerContainer != nullptr)
        {
            Mon_Simon_Weapon* weapon = dynamic_cast<Mon_Simon_Weapon*>(ownerContainer->Get_PartObject(_partTag));
            if (weapon != nullptr)
                return weapon;
        }

        current = current->Get_Owner();
    }

    return nullptr;
}

uint32 ANS_WeaponEcho::Resolve_EchoCount(const AnimNotifyContext& context) const
{
    if (context.isPreview && _echoCountOverride > 0)
        return std::clamp(static_cast<uint32>(_echoCountOverride), 1u, 2u);

    const uint32 autoEchoCount = Resolve_AutoEchoCount(context);
    if (autoEchoCount == 0)
        return 0u;

    if (context.owner != nullptr)
    {
        Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
        while (current)
        {
            Monster_BattleBase* battleMonster = dynamic_cast<Monster_BattleBase*>(current.get());
            if (battleMonster != nullptr)
            {
                const string& sourcePrefabName = battleMonster->Get_SourcePrefabName();
                if (sourcePrefabName == Constants::Prefab::MB_Simon ||
                    sourcePrefabName == Constants::Prefab::MB_Simon_V2)
                    return autoEchoCount;

                break;
            }

            current = current->Get_Owner();
        }
    }

    if (_echoCountOverride > 0)
        return std::clamp(static_cast<uint32>(_echoCountOverride), 1u, 2u);

    return autoEchoCount;
}

uint32 ANS_WeaponEcho::Resolve_AutoEchoCount(const AnimNotifyContext& context) const
{
    if (context.owner == nullptr)
        return 1u;

    Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
    while (current)
    {
        Monster_BattleBase* battleMonster = dynamic_cast<Monster_BattleBase*>(current.get());
        if (battleMonster != nullptr)
        {
            const string& sourcePrefabName = battleMonster->Get_SourcePrefabName();
            if (sourcePrefabName == Constants::Prefab::MB_Simon)
                return 0u;

            if (sourcePrefabName == Constants::Prefab::MB_Simon_V2)
            {
                if (battleMonster->Get_CurPhase() == Monster_BattleBase::BattlePhase::Critical)
                    return 2u;

                return 1u;
            }

            return 1u;
        }

        current = current->Get_Owner();
    }

    return 1u;
}

void ANS_WeaponEcho::Record_Sample(const AnimNotifyContext& context, bool force)
{
    Mon_Simon_Weapon* simonWeapon = Resolve_TargetSimonWeapon(context);
    if (simonWeapon == nullptr)
        return;

    simonWeapon->Update_WeaponEchoSource(context.model, context.animationName, context.currentTimeSec);

    const float elapsedSec = Get_ElapsedSec(context);
    const float interval = max(0.f, _sampleInterval);
    const bool timeMovedBackward = _lastSampleElapsedSec >= 0.f && elapsedSec + 1e-4f < _lastSampleElapsedSec;
    const bool shouldSample =
        force ||
        timeMovedBackward ||
        _lastSampleElapsedSec < 0.f ||
        interval <= 0.f ||
        elapsedSec - _lastSampleElapsedSec + 1e-4f >= interval;

    if (!shouldSample)
        return;

    simonWeapon->Record_WeaponEchoSample(elapsedSec);

    _lastSampleElapsedSec = elapsedSec;
}

float ANS_WeaponEcho::Get_ElapsedSec(const AnimNotifyContext& context) const
{
    return max(0.f, context.currentTimeSec);
}
