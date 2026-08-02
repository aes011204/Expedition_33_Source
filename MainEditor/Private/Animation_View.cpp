#include "Animation_View.h"
#include "Animation_NameUtils.h"
#include "AnimationNotifyEditor.h"
#include "AnimSequencerAdapter.h"
#include "AnimNotify_Parser.h"
#include "ANS_JumpCounter.h"
#include "ANS_Trail.h"
#include "ANS_WorldTimeScale.h"
#include "AnimationModifier.h"
#include "ChainModifier.h"
#include "ClientInstance.h"
#include "PhysicsModifier.h"
#include "PoseFollowChainModifier.h"
#include "ContainerObject.h"
#include "EditorInstance.h"
#include "Editor_Camera_Free.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "DebugDraw.h"
#include "Editor_Macro.h"
#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "Inspector.h"
#include "ModelPhysicsParser.h"
#include "ModelCom.h"
#include "Prefab_View.h"
#include "PartObject.h"
#include "PlayableParts.h"
#include "PreviewObjectHelpers.h"
#include "RenderTarget.h"
#include "SkyBoxFlow.h"
#include "SkyCloudCard.h"
#include "TransformCom.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "Mon_Simon_Hair.h"
#include "imgui_internal.h"
#include "Notification_Manager.h"
#include "PartObject.h"
#include "Player_BattleBase.h"
#include "Weapon.h"
#include "Mon_Dualliste_Sword_L.h"
#include "Mon_Dualliste_Sword_R.h"
#include "Mon_PotatoBag_Weapon.h"
#include "Mon_Simon_Weapon.h"
#include "Monster_Part.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
constexpr uint64 kPreviewWeaponEchoTrailTokenMask = uint64{ 1 } << 63;

Shared<ModelCom> Resolve_PreviewTrailPartModel(const PartObject* partObject)
{
    if (partObject == nullptr)
        return nullptr;

    if (const Weapon* weaponPart = dynamic_cast<const Weapon*>(partObject))
        return weaponPart->Get_Model();

    if (const Client::Monster_Part* monsterPart = dynamic_cast<const Client::Monster_Part*>(partObject))
        return monsterPart->Get_Model();

    if (const PlayableParts* playablePart = dynamic_cast<const PlayableParts*>(partObject))
        return playablePart->Get_Model();

    return nullptr;
}

const Matrix* Resolve_PreviewTrailPartSocketMatrix(const PartObject* partObject, const string& socketName)
{
    if (partObject == nullptr)
        return nullptr;

    if (const Weapon* weaponPart = dynamic_cast<const Weapon*>(partObject))
        return weaponPart->Get_SocketBoneMatrixPtr(socketName);

    if (const Client::Monster_Part* monsterPart = dynamic_cast<const Client::Monster_Part*>(partObject))
        return monsterPart->Get_SocketBoneMatrixPtr(socketName);

    if (const PlayableParts* playablePart = dynamic_cast<const PlayableParts*>(partObject))
        return playablePart->Get_SocketBoneMatrixPtr(socketName);

    return nullptr;
}

wstring Resolve_PreviewDefaultTrailEffectName(const PartObject* partObject)
{
    if (partObject == nullptr)
        return {};

    if (const Weapon* weaponPart = dynamic_cast<const Weapon*>(partObject))
        return weaponPart->Get_TrailEffectName();

    if (const Client::Mon_Simon_Weapon* simonWeapon = dynamic_cast<const Client::Mon_Simon_Weapon*>(partObject))
        return simonWeapon->Get_TrailEffectName();

    if (const Client::Mon_Dualliste_Sword_L* duallisteSwordL = dynamic_cast<const Client::Mon_Dualliste_Sword_L*>(partObject))
        return duallisteSwordL->Get_TrailEffectName();

    if (const Client::Mon_Dualliste_Sword_R* duallisteSwordR = dynamic_cast<const Client::Mon_Dualliste_Sword_R*>(partObject))
        return duallisteSwordR->Get_TrailEffectName();

    if (const Client::Mon_PotatoBag_Weapon* potatoBagWeapon = dynamic_cast<const Client::Mon_PotatoBag_Weapon*>(partObject))
        return potatoBagWeapon->Get_TrailEffectName();

    return {};
}
}

namespace
{
    const Vec3 kPreviewSkyCloudLocalOffset = Vec3(28.f, 34.f, 82.f);
    constexpr float kTrailOffsetDuplicatePhaseEpsilon = 0.0001f;

    enum class TrailOffsetMode : int32
    {
        None,
        TipExtend,
        Advanced
    };

    TrailOffsetMode Resolve_TrailOffsetMode(const Client::ANS_Trail& trailNotifyState)
    {
        const Client::ANS_Trail::LocalOffsetCurve& baseCurve = trailNotifyState.Get_BaseLocalOffsetCurve();
        const Client::ANS_Trail::LocalOffsetCurve& tipCurve = trailNotifyState.Get_TipLocalOffsetCurve();
        if (baseCurve.enabled || !baseCurve.keys.empty() || tipCurve.enabled || !tipCurve.keys.empty())
            return TrailOffsetMode::Advanced;

        const Client::ANS_Trail::TipExtendCurve& extendCurve = trailNotifyState.Get_TipExtendCurve();
        if (extendCurve.enabled || !extendCurve.keys.empty())
            return TrailOffsetMode::TipExtend;

        return TrailOffsetMode::None;
    }

    bool Is_SameVec3(const Vec3& lhs, const Vec3& rhs)
    {
        return
            lhs.x == rhs.x &&
            lhs.y == rhs.y &&
            lhs.z == rhs.z;
    }

    Client::ANS_Trail::TipExtendInterpolation Draw_TrailOffsetInterpolationCombo(
        const char* label,
        Client::ANS_Trail::TipExtendInterpolation interpolation,
        bool& changed)
    {
        const char* names[] = { "Linear", "Constant" };
        int32 selected = interpolation == Client::ANS_Trail::TipExtendInterpolation::Constant ? 1 : 0;
        ImGui::SetNextItemWidth(105.f);
        if (ImGui::Combo(label, &selected, names, IM_ARRAYSIZE(names)))
            changed = true;

        return selected == 1
            ? Client::ANS_Trail::TipExtendInterpolation::Constant
            : Client::ANS_Trail::TipExtendInterpolation::Linear;
    }

    float Evaluate_TipExtendCurve(const Client::ANS_Trail::TipExtendCurve& curve, float phase)
    {
        if (curve.keys.empty())
            return 0.f;

        phase = std::clamp(phase, 0.f, 1.f);
        if (phase <= curve.keys.front().phase)
            return curve.keys.front().value;

        for (size_t keyIndex = 1; keyIndex < curve.keys.size(); ++keyIndex)
        {
            const Client::ANS_Trail::TipExtendKey& previous = curve.keys[keyIndex - 1];
            const Client::ANS_Trail::TipExtendKey& next = curve.keys[keyIndex];
            if (phase > next.phase)
                continue;

            if (previous.interpolation == Client::ANS_Trail::TipExtendInterpolation::Constant)
                return previous.value;

            const float range = max(kTrailOffsetDuplicatePhaseEpsilon, next.phase - previous.phase);
            const float t = std::clamp((phase - previous.phase) / range, 0.f, 1.f);
            return std::lerp(previous.value, next.value, t);
        }

        return curve.keys.back().value;
    }

    Vec3 Evaluate_LocalOffsetCurve(const Client::ANS_Trail::LocalOffsetCurve& curve, float phase)
    {
        if (curve.keys.empty())
            return Vec3::Zero;

        phase = std::clamp(phase, 0.f, 1.f);
        if (phase <= curve.keys.front().phase)
            return curve.keys.front().value;

        for (size_t keyIndex = 1; keyIndex < curve.keys.size(); ++keyIndex)
        {
            const Client::ANS_Trail::LocalOffsetKey& previous = curve.keys[keyIndex - 1];
            const Client::ANS_Trail::LocalOffsetKey& next = curve.keys[keyIndex];
            if (phase > next.phase)
                continue;

            if (previous.interpolation == Client::ANS_Trail::TipExtendInterpolation::Constant)
                return previous.value;

            const float range = max(kTrailOffsetDuplicatePhaseEpsilon, next.phase - previous.phase);
            const float t = std::clamp((phase - previous.phase) / range, 0.f, 1.f);
            return Vec3::Lerp(previous.value, next.value, t);
        }

        return curve.keys.back().value;
    }

    void Upsert_TipExtendKey(Client::ANS_Trail::TipExtendCurve& curve, float phase, float value)
    {
        phase = std::clamp(phase, 0.f, 1.f);
        for (Client::ANS_Trail::TipExtendKey& key : curve.keys)
        {
            if (std::abs(key.phase - phase) <= kTrailOffsetDuplicatePhaseEpsilon)
            {
                key.phase = phase;
                key.value = value;
                return;
            }
        }

        Client::ANS_Trail::TipExtendKey key{};
        key.phase = phase;
        key.value = value;
        curve.keys.push_back(key);
    }

    void Upsert_LocalOffsetKey(Client::ANS_Trail::LocalOffsetCurve& curve, float phase, const Vec3& value)
    {
        phase = std::clamp(phase, 0.f, 1.f);
        for (Client::ANS_Trail::LocalOffsetKey& key : curve.keys)
        {
            if (std::abs(key.phase - phase) <= kTrailOffsetDuplicatePhaseEpsilon)
            {
                key.phase = phase;
                key.value = value;
                return;
            }
        }

        Client::ANS_Trail::LocalOffsetKey key{};
        key.phase = phase;
        key.value = value;
        curve.keys.push_back(key);
    }

    Client::ANS_Trail::TipExtendCurve Make_DefaultTipExtendCurve()
    {
        Client::ANS_Trail::TipExtendCurve curve{};
        curve.enabled = true;
        curve.keys = {
            { 0.f, 0.f, Client::ANS_Trail::TipExtendInterpolation::Linear },
            { 0.5f, 0.f, Client::ANS_Trail::TipExtendInterpolation::Linear },
            { 1.f, 0.f, Client::ANS_Trail::TipExtendInterpolation::Linear }
        };
        return curve;
    }

    Client::ANS_Trail::LocalOffsetCurve Make_DefaultLocalOffsetCurve(bool enabled)
    {
        Client::ANS_Trail::LocalOffsetCurve curve{};
        curve.enabled = enabled;
        if (enabled)
        {
            curve.keys = {
                { 0.f, Vec3::Zero, Client::ANS_Trail::TipExtendInterpolation::Linear },
                { 1.f, Vec3::Zero, Client::ANS_Trail::TipExtendInterpolation::Linear }
            };
        }
        return curve;
    }

    string Resolve_NotifyAnimationKey(const Animation_View::AnimationEntry& entry)
    {
        if (!entry.playbackName.empty())
            return entry.playbackName;

        if (!entry.name.empty())
            return entry.name;

        return entry.displayName;
    }

    float Resolve_TrailDrainDuration(const Shared<const EffectDefinition>& definition)
    {
        constexpr float fallbackDuration = 0.25f;
        if (definition == nullptr)
            return fallbackDuration;

        float duration = fallbackDuration;
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled || emitter.kind != EffectEmitterKind::Trail)
                continue;

            const ComputeTrailEmitterDesc* trailDesc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc);
            if (trailDesc == nullptr)
                continue;

            duration = max(duration, trailDesc->segmentLifetime);
        }

        return max(0.001f, duration);
    }

    float Resolve_SourceHistoryDrainDuration(const Shared<const EffectDefinition>& definition)
    {
        constexpr float fallbackDuration = 0.5f;
        if (definition == nullptr)
            return fallbackDuration;

        float duration = fallbackDuration;
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled)
                continue;

            if (emitter.kind == EffectEmitterKind::Ribbon)
            {
                const ComputeRibbonEmitterDesc* ribbonDesc = get_if<ComputeRibbonEmitterDesc>(&emitter.concreteDesc);
                if (ribbonDesc == nullptr || ribbonDesc->sourceMode != EffectSourceHistoryRibbonSourceMode::SelfRoot)
                    continue;

                duration = max(duration, ribbonDesc->sampleLifetime);
                continue;
            }

            if (emitter.kind == EffectEmitterKind::SourceHistorySpriteTrail)
            {
                const ComputeSourceHistorySpriteTrailEmitterDesc* spriteTrailDesc =
                    get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitter.concreteDesc);
                if (spriteTrailDesc == nullptr || spriteTrailDesc->sourceMode != EffectSourceHistoryRibbonSourceMode::SelfRoot)
                    continue;

                float spriteDuration = spriteTrailDesc->sampleLifetime + spriteTrailDesc->stampLifetime;
                if (spriteTrailDesc->pathReplay.enabled)
                {
                    spriteDuration +=
                        spriteTrailDesc->pathReplay.delayTime +
                        spriteTrailDesc->pathReplay.drainDuration;
                }

                duration = max(duration, spriteDuration);
            }
        }

        return max(0.001f, duration + 0.1f);
    }

    AnimNotifyData* Find_OrNormalize_NotifyAnimation(
        AnimNotifyAsset& asset,
        const Animation_View::AnimationEntry& entry)
    {
        const string runtimeKey = Resolve_NotifyAnimationKey(entry);

        if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, runtimeKey))
            return animation;

        if (!entry.name.empty())
        {
            if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.name))
            {
                animation->animationName = runtimeKey;
                return animation;
            }
        }

        if (!entry.displayName.empty())
        {
            if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.displayName))
            {
                animation->animationName = runtimeKey;
                return animation;
            }
        }

        return &AnimNotify_Parser::Get_OrAddAnimation(asset, runtimeKey);
    }

    const AnimNotifyData* Find_NotifyAnimation(
        const AnimNotifyAsset& asset,
        const Animation_View::AnimationEntry& entry)
    {
        const string runtimeKey = Resolve_NotifyAnimationKey(entry);

        if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, runtimeKey))
            return animation;

        if (!entry.name.empty())
        {
            if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.name))
                return animation;
        }

        if (!entry.displayName.empty())
        {
            if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.displayName))
                return animation;
        }

        return nullptr;
    }

    void Invalidate_AnimNotifyAssetsForObject(
        const Shared<GameObject>& gameObject,
        const wstring& sourceModelPath)
    {
        if (gameObject == nullptr)
            return;

        if (const Shared<ModelCom> modelCom = gameObject->Get_Component<ModelCom>())
        {
            if (sourceModelPath.empty() || modelCom->Get_SourceModelPath() == sourceModelPath)
                modelCom->Invalidate_AnimNotifyAsset();
        }

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            Invalidate_AnimNotifyAssetsForObject(partObject, sourceModelPath);
        }
    }

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

        return targetTime > previousTime || targetTime <= currentTime;
    }

    bool Contains_CaseInsensitive(const string& text, const char* filter)
    {
        if (filter == nullptr || filter[0] == '\0')
            return true;

        string source = text;
        string needle = filter;

        auto toLower = [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        };

        std::transform(source.begin(), source.end(), source.begin(), toLower);
        std::transform(needle.begin(), needle.end(), needle.begin(), toLower);

        return source.find(needle) != string::npos;
    }

    struct ModifierPropertySection
    {
        const char* title;
        const char* help;
    };

    bool Starts_With(const string& text, const char* prefix)
    {
        const size_t prefixLength = std::strlen(prefix);
        return text.size() >= prefixLength && text.compare(0, prefixLength, prefix) == 0;
    }

    const char* Resolve_ChainModifierSection(const string& propertyName)
    {
        if (Starts_With(propertyName, "collision."))
            return "Collision";

        if (propertyName == "runtime.gravity"
            || propertyName == "runtime.windDirection"
            || propertyName == "runtime.windScale"
            || propertyName == "runtime.windStartRatio"
            || propertyName == "runtime.windWeightScale")
            return "Forces";

        if (propertyName == "motion.movementInertiaScale"
            || propertyName == "motion.ownerMovementInertiaScale"
            || propertyName == "motion.tipMovementInertiaScale"
            || propertyName == "motion.maxMovementOffset")
            return "Motion Input / Limit";

        if (propertyName == "motion.damping"
            || propertyName == "motion.tipDampingScale"
            || propertyName == "runtime.constraintDeadzone"
            || propertyName == "runtime.resetDistance"
            || propertyName == "runtime.targetFrameRate")
            return "Damping / Stability";

        if (propertyName == "shape.poseRestoreStrength"
            || propertyName == "shape.tipPoseRestoreScale"
            || propertyName == "shape.rootOutputAlpha"
            || propertyName == "shape.tipOutputAlpha")
            return "Restore / Output";

        if (Starts_With(propertyName, "shape."))
            return "Shape / Bend";

        return "Other";
    }

    const char* Resolve_PoseFollowModifierSection(const string& propertyName)
    {
        if (propertyName == "enableCollision"
            || propertyName == "particleCollisionRadius"
            || propertyName == "collisionRadiusPadding"
            || propertyName == "collisionPrimitiveScale"
            || propertyName == "collisionOutputAlpha"
            || propertyName == "debugDrawCapsules"
            || propertyName == "debugActiveChainCount"
            || propertyName == "debugActiveNodeCount"
            || propertyName == "debugLoadedCapsuleCount"
            || propertyName == "debugActiveCapsuleCount"
            || propertyName == "debugCollisionSource"
            || propertyName == "debugCollisionHitCount"
            || propertyName == "debugSegmentCollisionHitCount"
            || propertyName == "debugFinalCollisionHitCount"
            || propertyName == "debugMinCollisionMargin")
            return "Collision";

        if (propertyName == "windDirection"
            || propertyName == "windScale"
            || propertyName == "ownerMotionScale"
            || propertyName == "animatedMotionScale"
            || propertyName == "ownerMotionDeadzone"
            || propertyName == "ownerRotationDeadzoneDegree"
            || propertyName == "animatedMotionDeadzone"
            || propertyName == "maxExternalOffset")
            return "Motion Input / Gates";

        if (propertyName == "damping"
            || propertyName == "tipDampingScale"
            || propertyName == "idleSettleStrength"
            || propertyName == "sleepVelocityThreshold"
            || propertyName == "sleepPoseDistance")
            return "Damping / Settle";

        if (propertyName == "poseFollowStrength"
            || propertyName == "tipFollowScale"
            || propertyName == "maxBendAngleDegree"
            || propertyName == "tipBendAngleScale"
            || propertyName == "bendLimitStrength"
            || propertyName == "surfaceConstraintStrength")
            return "Follow / Shape";

        if (propertyName == "rootOutputAlpha"
            || propertyName == "tipOutputAlpha"
            || propertyName == "idleOutputAlphaScale"
            || propertyName == "outputSmoothAlpha"
            || propertyName == "idleOutputSmoothAlpha"
            || propertyName == "outputDeadzoneDegree")
            return "Output / Idle";

        if (propertyName == "solverIterations"
            || propertyName == "targetFrameRate"
            || propertyName == "resetDistance")
            return "Runtime";

        return "Other";
    }

    const char* Resolve_ModifierPropertySection(const string& className, const string& propertyName)
    {
        if (className == "ChainModifierRuntime")
            return Resolve_ChainModifierSection(propertyName);

        if (className == "PoseFollowChainModifierRuntime")
            return Resolve_PoseFollowModifierSection(propertyName);

        return nullptr;
    }

    bool Draw_ModifierRuntimeProperty(
        AnimationModifier* modifier,
        ModelCom* model,
        const ClassReflectionInfo* reflectionInfo,
        void* reflectionTarget,
        const PropertyInfo& property)
    {
        PropertyInfo drawProperty = property;
        if (reflectionInfo->className == "ChainModifierRuntime")
        {
            if (property.name == "motion.ownerMovementInertiaScale")
                drawProperty.displayName = "Owner Iner";
            else if (property.name == "motion.tipMovementInertiaScale")
                drawProperty.displayName = "Tip Iner";
            else if (property.name == "shape.poseRestoreStrength")
                drawProperty.displayName = "Restore";
            else if (property.name == "shape.tipPoseRestoreScale")
                drawProperty.displayName = "Tip Restore";
            else if (property.name == "shape.surfaceConstraintStrength")
                drawProperty.displayName = "Surface";
            else if (property.name == "runtime.constraintDeadzone")
                drawProperty.displayName = "Const Dead";
            else if (property.name == "runtime.targetFrameRate")
                drawProperty.displayName = "FPS";
        }
        else if (reflectionInfo->className == "PoseFollowChainModifierRuntime")
        {
            if (property.name == "ownerRotationDeadzoneDegree")
                drawProperty.displayName = "Rot Dead";
            else if (property.name == "animatedMotionDeadzone")
                drawProperty.displayName = "Anim Dead";
            else if (property.name == "idleOutputAlphaScale")
                drawProperty.displayName = "Idle Out";
            else if (property.name == "idleOutputSmoothAlpha")
                drawProperty.displayName = "Idle Smooth";
            else if (property.name == "outputDeadzoneDegree")
                drawProperty.displayName = "Out Dead";
            else if (property.name == "sleepVelocityThreshold")
                drawProperty.displayName = "Sleep Vel";
            else if (property.name == "solverIterations")
                drawProperty.displayName = "Solver";
            else if (property.name == "targetFrameRate")
                drawProperty.displayName = "FPS";
        }

        if (Inspector::Draw_ReflectionProperty(reflectionInfo->className, reflectionTarget, drawProperty))
        {
            modifier->On_RuntimeReflectionChanged(model, property);
            return true;
        }

        return false;
    }

    void Draw_ModifierRuntimeProperties(
        AnimationModifier* modifier,
        ModelCom* model,
        const ClassReflectionInfo* reflectionInfo,
        void* reflectionTarget)
    {
        const bool isChain = reflectionInfo->className == "ChainModifierRuntime";
        const bool isPoseFollow = reflectionInfo->className == "PoseFollowChainModifierRuntime";
        if (!isChain && !isPoseFollow)
        {
            for (const PropertyInfo& property : reflectionInfo->properties)
            {
                Draw_ModifierRuntimeProperty(modifier, model, reflectionInfo, reflectionTarget, property);
                ImGui::Spacing();
            }
            return;
        }

        const ModifierPropertySection chainSections[] =
        {
            { "Motion Input / Limit", "Inertia adds motion; Move Limit clamps that input." },
            { "Damping / Stability", "Damping keeps residual velocity; deadzone/reset limit small jitter and teleports." },
            { "Restore / Output", "Restore pulls toward pose; Root/Tip Out decides how much sim writes to bones." },
            { "Shape / Bend", "Stiffness and bend constraints decide how much the chain keeps its shape." },
            { "Forces", "Gravity and wind are continuous forces even when character motion is small." },
            { "Collision", "Collision changes particle constraints, not the authored bone pose." },
            { "Other", "" },
        };

        const ModifierPropertySection poseFollowSections[] =
        {
            { "Motion Input / Gates", "Owner/Anim/Wind add external motion; deadzones and Move Limit gate that input." },
            { "Damping / Settle", "Damping keeps velocity; settle/sleep values calm idle motion." },
            { "Follow / Shape", "Follow restores pose direction; bend/surface limit how far it can fold." },
            { "Output / Idle", "Output controls how much sim writes back; idle values reduce no-motion flutter." },
            { "Collision", "Capsule push-out uses these radius/debug values." },
            { "Runtime", "Substep, target FPS, and teleport reset controls." },
            { "Other", "" },
        };

        const ModifierPropertySection* sections = isChain ? chainSections : poseFollowSections;
        const size_t sectionCount = isChain
            ? sizeof(chainSections) / sizeof(chainSections[0])
            : sizeof(poseFollowSections) / sizeof(poseFollowSections[0]);

        for (size_t sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
        {
            const ModifierPropertySection& section = sections[sectionIndex];
            bool hasSectionProperty = false;
            for (const PropertyInfo& property : reflectionInfo->properties)
            {
                const char* propertySection = Resolve_ModifierPropertySection(reflectionInfo->className, property.name);
                if (propertySection != nullptr && std::strcmp(propertySection, section.title) == 0)
                {
                    hasSectionProperty = true;
                    break;
                }
            }

            if (!hasSectionProperty)
                continue;

            if (ImGui::TreeNodeEx(section.title, ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (section.help != nullptr && section.help[0] != '\0')
                    ImGui::TextDisabled("%s", section.help);

                for (const PropertyInfo& property : reflectionInfo->properties)
                {
                    const char* propertySection = Resolve_ModifierPropertySection(reflectionInfo->className, property.name);
                    if (propertySection == nullptr || std::strcmp(propertySection, section.title) != 0)
                        continue;

                    Draw_ModifierRuntimeProperty(modifier, model, reflectionInfo, reflectionTarget, property);
                    ImGui::Spacing();
                }

                ImGui::TreePop();
            }
        }
    }
}

Animation_View::Animation_View()
    : Editor_Window(L"Animation")
{
    _notifyEditor = make_unique<AnimationNotifyEditor>(*this);
    _sequencer = make_unique<AnimSequencerAdapter>(this);
    Set_Open(false);
}

Animation_View::~Animation_View() = default;

void Animation_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    if (EDITOR->IsPlaying())
        return;

    if (!Is_Visible())
        return;

    if (Should_DeferToCompanionPreview())
        return;

    if (_previewCamera && _isPreviewHovered)
        _previewCamera->Priority_Update(timeDelta);

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    Validate_SelectedBoneSelection();

    if (_isSectionPreviewPlaying)
    {
        const AnimNotifyData* animationData = Get_CurrentAnimation();
        if (animationData == nullptr)
        {
            _isSectionPreviewPlaying = false;
            return;
        }

        const float startSec = Get_PlaySectionStartSec(animationData);
        const float endSec = Get_PlaySectionEndSec(animationData);

        if (playbackModel->Get_CurrentAnimationTimeSec() + 0.0001f >= endSec)
        {
            playbackModel->Set_CurrentAnimationTimeSec(startSec);
            playbackModel->Play_Animation(0.f);
            playbackModel->Set_AnimationPaused(false);
            _previewPlayEffectIncludeStartEdge = true;
            _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
            return;
        }
    }

    if (!playbackModel->Is_AnimationPaused())
    {
        if (playbackModel->Is_AnimationFinished())
        {
            // ??硫명뀊 ?????? ?熬곣뫗????ルㅎ臾????ル봾鍮띸춯濡ル뾼???瑜곷さ??嶺뚯빖留?????곕뻣 ??戮곗굚??怨댄맋 ???깅쾳 ??????怨쀬Ŧ ??琉우꽑?띠럾?嶺뚯솘? ??袁⑹벟 ??類ｋ펲.
            Sync_PreviewAnimationSelection();
        }

        _sequencerCurrentFrame = static_cast<int32>(std::round(playbackModel->Get_CurrentAnimationTimeSec() * static_cast<float>(Get_CurrentAnimationFps())));
    }
}

void Animation_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (EDITOR->IsPlaying())
        return;

    if (!Is_Visible())
        return;

    if (Should_DeferToCompanionPreview())
        return;

    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    if (previewRenderObject == nullptr || _previewCamera == nullptr)
        return;

    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);
    if (_previewRenderTarget == nullptr)
        return;

    if (_showPreviewSkyBackdrop)
        Ensure_PreviewSkyBackdrop();

    const Matrix savedView = *GAME->Get_Transform(D3DTS::View);
    const Matrix savedProj = *GAME->Get_Transform(D3DTS::Proj);
    const Shared<Camera> savedActiveCamera = GAME->Get_ActiveCamera();
    const uint32 savedViewportWidth = GAME->Get_ViewportWidth();
    const uint32 savedViewportHeight = GAME->Get_ViewportHeight();
    vector<LIGHT_DESC> savedLightDescs{};
    GAME->Get_LightDescs(savedLightDescs);
    const bool wasPlayerInputBlocked = GAME->Is_PlayerInputBlocked();

    EDITOR->Apply_PreviewViewportSizeImmediate(_previewWidth, _previewHeight);

    GAME->Bind_BackBuffer();
    if (FAILED(GAME->Clear_Buffers(Vec4(0.f, 0.f, 0.f, 1.f))))
    {
        EDITOR->Apply_PreviewViewportSizeImmediate(savedViewportWidth, savedViewportHeight);
        return;
    }

    const Matrix previewView = _previewCamera->Get_ViewMatrix();
    const float aspect = static_cast<float>(_previewWidth) / static_cast<float>(max(1u, _previewHeight));
    const Matrix previewProj = XMMatrixPerspectiveFovLH(_previewCamera->Get_FovY(), aspect, _previewCamera->Get_Near(), _previewCamera->Get_Far());

    GAME->Change_ActiveCamera(_previewCamera);
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(previewView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(previewProj));
    GAME->UpdateFrustum(previewView, previewProj);
    GAME->Update_SoundListenerFromActiveCamera();

    vector<LIGHT_DESC> previewLightDescs{};
    previewLightDescs.reserve(2);

    LIGHT_DESC keyLight{};
    keyLight.type = LightType::Directional;
    keyLight.direction = Vec4(0.35f, -1.f, 0.45f, 0.f);
    keyLight.diffuse = Vec4(0.55f, 0.55f, 0.55f, 1.f);
    keyLight.ambient = Vec4(0.14f, 0.14f, 0.14f, 1.f);
    keyLight.specular = Vec4(0.18f, 0.18f, 0.18f, 1.f);
    previewLightDescs.push_back(keyLight);

    LIGHT_DESC fillLight{};
    fillLight.type = LightType::Directional;
    fillLight.direction = Vec4(-0.4f, -0.25f, -0.8f, 0.f);
    fillLight.diffuse = Vec4(0.18f, 0.18f, 0.2f, 1.f);
    fillLight.ambient = Vec4(0.04f, 0.04f, 0.04f, 1.f);
    fillLight.specular = Vec4(0.05f, 0.05f, 0.05f, 1.f);
    previewLightDescs.push_back(fillLight);

    GAME->Set_Lights(previewLightDescs);
    GAME->Set_PlayerInputBlocked(true);

    const float dt = min(ImGui::GetIO().DeltaTime, 1.f / 30.f);
    float previewEffectTimeDelta = dt;
    Lock_PreviewObjectToOrigin(previewRenderObject);
    if (_showPreviewSkyBackdrop)
        Update_PreviewSkyBackdrop(dt);

    if (_sourceMode == SourceMode::LiveModel)
    {
        const float previousTimeSec = _previewModelCom ? _previewModelCom->Get_CurrentAnimationTimeSec() : 0.f;
        const PreviewWorldTimeScaleState worldTimeScaleState = Resolve_PreviewWorldTimeScaleState(previousTimeSec);
        const float previewTimeDelta = dt * worldTimeScaleState.worldTimeScale;
        previewEffectTimeDelta = worldTimeScaleState.scaleEffects ? previewTimeDelta : dt;

        GAME->Backup_RenderGroup();
        if (_showPreviewSkyBackdrop)
            Late_Update_PreviewSkyBackdrop(dt);

        Update_PreviewAnimSpeedStates(previewTimeDelta); // ANS_AnimSpeed??sampling ?熬곣뫖????⑤챷???怨룻뒍 ?熬곣뫗??preview ??繹????쒖┣???꾩룆?餓??꾩룇瑗???類ｋ펲.
        Update_PreviewRootControllerStates(previewTimeDelta);
        PreviewObjectHelpers::Update_PreviewObject(previewRenderObject, 0.f, false);
        Update_ModifierPreviewMotionTest(previewTimeDelta);
        PreviewObjectHelpers::Update_PreviewObject(previewRenderObject, previewTimeDelta, false, true);
        const float currentTimeSec = _previewModelCom ? _previewModelCom->Get_CurrentAnimationTimeSec() : previousTimeSec;
        Update_PreviewPlayEffectNotifies(previousTimeSec, currentTimeSec, previewEffectTimeDelta);
        Update_PreviewScreenFxCueStates(dt);
        PreviewObjectHelpers::Tick_AttachedPreviewEffects(previewRenderObject, previewEffectTimeDelta);
        Update_PreviewSLDodgeMoveStates(previewTimeDelta);
        Update_PreviewWeaponRotateStates(previewTimeDelta); // preview 전용 WeaponRotate를 먼저 반영하고 무기 world를 다시 맞춘 뒤 trail sample을 계산한다.
        Update_PreviewWeaponEchoStates(previewTimeDelta);
        Update_PreviewSourceHistoryEffect(previewEffectTimeDelta);
        Update_PreviewTrailEffect(previewEffectTimeDelta);
    }
    else
    {
        GAME->Backup_RenderGroup();
        if (_showPreviewSkyBackdrop)
            Late_Update_PreviewSkyBackdrop(dt);
    }

    for (PreviewTrailRuntime& runtime : _previewTrailRuntimes)
    {
        if (runtime.effect && runtime.isPlaying)
            runtime.effect->Late_Update(previewEffectTimeDelta); // preview????????戮?뎽 trail runtime嶺뚮씭???Late_Update????⑤벡夷????逾??
    }

    for (PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
    {
        if (runtime.effect && runtime.isPlaying)
            runtime.effect->Late_Update(previewEffectTimeDelta);
    }

    GAME->Draw_RendererOnly(true);
    GAME->Restore_RenderGroup();
    Ensure_PreviewGridResources();
    Draw_SelectedBoneMarker();
    Draw_TrailAnchorPreview();
    Draw_ModifierChainDebug();
    Draw_ModifierCollisionDebug();
    Draw_PreviewGrid();
    EDITOR->Capture_BackBuffer_ToRenderTarget(_previewRenderTarget);
    GAME->Set_Lights(savedLightDescs);
    GAME->Set_PlayerInputBlocked(wasPlayerInputBlocked);

    GAME->Bind_BackBuffer();
    if (savedActiveCamera != nullptr)
        GAME->Change_ActiveCamera(savedActiveCamera);
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
    GAME->UpdateFrustum(savedView, savedProj);
    GAME->Update_SoundListenerFromActiveCamera();
    EDITOR->Apply_PreviewViewportSizeImmediate(savedViewportWidth, savedViewportHeight);
}

void Animation_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (_pendingDockId != 0)
    {
        ImGui::DockBuilderDockWindow(windowName.c_str(), _pendingDockId);
        ImGui::SetNextWindowDockID(_pendingDockId, ImGuiCond_Always);
    }
    if (_requestFocusOnOpen)
        ImGui::SetNextWindowFocus();

    _isFocused = false;
    _isHovered = false;
    _isVisible = false;

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isVisible = true;
        _pendingDockId = 0;
        _requestFocusOnOpen = false;
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        if (_isFocused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false))
        {
            Shared<ModelCom> playbackModel = Get_PlaybackModel();
            if (playbackModel != nullptr)
            {
                if (playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
                else
                {
                    playbackModel->Set_AnimationPaused(true);
                    _isSectionPreviewPlaying = false;
                }
            }
        }

        Handle_ClipboardShortcuts();

        Draw_Header();
        ImGui::Separator();

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float minTopSectionHeight = 240.f;
        const float maxTopSectionHeight = max(minTopSectionHeight, availableHeight - 260.f);
        _topSectionHeight = std::clamp(_topSectionHeight, minTopSectionHeight, maxTopSectionHeight);

        if (ImGui::BeginChild("AnimationTopSection", ImVec2(0.f, _topSectionHeight), false))
        {
            if (ImGui::BeginTable("AnimationViewTopLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("ClipBrowser", ImGuiTableColumnFlags_WidthFixed, 320.f);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("SelectedDetail", ImGuiTableColumnFlags_WidthFixed, 380.f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                Draw_AnimationSelector();

                ImGui::TableSetColumnIndex(1);
                Draw_PreviewPanel();

                ImGui::TableSetColumnIndex(2);
                Draw_SelectedDetail();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        const ImVec2 splitterPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("AnimationTopBottomSplitter", ImVec2(-FLT_MIN, 6.f));
        if (ImGui::IsItemActive())
            _topSectionHeight = std::clamp(_topSectionHeight + ImGui::GetIO().MouseDelta.y, minTopSectionHeight, maxTopSectionHeight);

        const bool splitterHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitterPos,
            ImVec2(splitterPos.x + ImGui::GetContentRegionAvail().x, splitterPos.y + 6.f),
            splitterHovered ? IM_COL32(110, 110, 118, 255) : IM_COL32(72, 72, 78, 255),
            2.f);

        Draw_PlaybackToolbar();
        ImGui::Separator();

        if (ImGui::BeginTable("AnimationViewBottomLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("EventLists", ImGuiTableColumnFlags_WidthFixed, 250.f);
            ImGui::TableSetupColumn("Sequencer", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("TrackPanel", ImGuiTableColumnFlags_WidthFixed, 260.f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Draw_EventListPanel();

            ImGui::TableSetColumnIndex(1);
            Draw_Sequencer();

            ImGui::TableSetColumnIndex(2);
            Draw_TrackSection();

            ImGui::EndTable();
        }

        Draw_CreateNotifyPopup();
        Draw_CreateNotifyStatePopup();
        Draw_SequencerContextMenu();

        if (_hasPendingNotifyUndoSnapshot &&
            !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemActive())
        {
            Commit_PendingNotifyUndo();
        }
    }

    Set_Open(isOpen);
    ImGui::End();

    if (!isOpen)
    {
        Reset_PreviewAnimSpeedStates();
        Reset_PreviewPlayEffectStates();
        Reset_PreviewScreenFxCueStates();
        Reset_PreviewSourceHistoryEffect();
        Reset_PreviewRootControllerStates();
        Reset_PreviewWeaponRotateStates();
        Reset_PreviewTrailEffect();
    }
}

bool Animation_View::CanSave() const
{
    return !_modelGuid.empty() && !_currentAnimationName.empty();
}

void Animation_View::Save()
{
    Save_NotifyAsset();
    Save_TrailAnchorAsset();
}

void Animation_View::Open_FromAsset(const wstring& modelPath, const string& modelGuid, const string& preferredAnimationName, uint32 preferredDockId)
{
    Cancel_NotifyTrailBonePick();
    Reset_PreviewTrailEffect();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewWeaponEchoStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewScreenFxCueStates();
    _skillPhaseContext = {};
    Reset_PreviewAnimSpeedStates();
    Reset_PreviewClone();

    _sourceMode = SourceMode::None;
    _sourceModelPath = modelPath;
    _liveModelCom.reset();
    _liveModelTargets.clear();
    _selectedLiveModelTargetIndex = -1;
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = true;

    _modelGuid = modelGuid;
    _currentAnimationName = preferredAnimationName;
    Clear_SelectedNotifySelection();
    Clear_SelectedStateSelection();
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerFirstFrame = 0;
    _sequencerVisibleFrameCount = 0;
    _isDirty = false;
    _animationSearchBuffer[0] = '\0';
    _boneSearchBuffer[0] = '\0';
    _selectedBoneIndex = -1;
    Clear_NotifyUndoHistory();

    Refresh_Source();
    Load_NotifyAsset();
    Load_TrailAnchorAsset();
    Set_Open(true);
}

void Animation_View::Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName, uint32 preferredDockId, bool focusOnOpen)
{
    Cancel_NotifyTrailBonePick();
    const wstring sourceModelPath = modelCom ? modelCom->Get_SourceModelPath() : wstring{};

    Reset_PreviewTrailEffect();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewWeaponEchoStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewScreenFxCueStates();
    _skillPhaseContext = {};
    Reset_PreviewAnimSpeedStates();
    Reset_PreviewClone();

    _sourceMode = SourceMode::LiveModel;
    _liveModelCom = modelCom;
    _sourceModelPath = sourceModelPath;
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = focusOnOpen;

    _modelGuid = _sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _currentAnimationName = preferredAnimationName;
    Clear_SelectedNotifySelection();
    Clear_SelectedStateSelection();
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerFirstFrame = 0;
    _sequencerVisibleFrameCount = 0;
    _isDirty = false;
    _animationSearchBuffer[0] = '\0';
    _boneSearchBuffer[0] = '\0';
    _selectedBoneIndex = -1;
    Clear_NotifyUndoHistory();

    Refresh_LiveModelTargets();
    Refresh_Source();

    Rebuild_PreviewClone();
    Load_NotifyAsset();
    Load_TrailAnchorAsset();
    Ensure_PreviewCamera();
    Fit_PreviewCameraToModel();
    Sync_PreviewAnimationSelection(true);
    EDITOR->Set_StickyPreviewOwner(PreviewOwner::Animation);

    Set_Open(true);
}

void Animation_View::Clear_LivePreviewBinding(const Shared<GameObject>& previewRoot)
{
    if (_sourceMode != SourceMode::LiveModel)
        return;

    if (previewRoot != nullptr)
    {
        const Shared<GameObject> currentPreviewRoot =
            _liveModelCom ? Resolve_RootPreviewObject(_liveModelCom->Get_Owner()) : nullptr;
        if (currentPreviewRoot != previewRoot)
            return;
    }

    Reset_PreviewTrailEffect();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewWeaponEchoStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewScreenFxCueStates();
    _skillPhaseContext = {};
    Reset_PreviewAnimSpeedStates();
    Reset_PreviewClone();

    _sourceMode = SourceMode::None;
    _sourceModelPath.clear();
    _liveModelCom.reset();
    _liveModelTargets.clear();
    _selectedLiveModelTargetIndex = -1;
    _modelGuid.clear();
    _currentAnimationName.clear();
    _availableAnimations.clear();
    Clear_SelectedNotifySelection();
    Clear_SelectedStateSelection();
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerCurrentFrame = 0;
    _sequencerSelectedRow = -1;
    _sequencerFirstFrame = 0;
    _sequencerVisibleFrameCount = 0;
    _selectedBoneIndex = -1;
    Clear_NotifyUndoHistory();
}

void Animation_View::Refresh_LivePreviewBinding(const Shared<GameObject>& previewRoot)
{
    if (_sourceMode != SourceMode::LiveModel || _liveModelCom == nullptr)
        return;

    if (previewRoot != nullptr)
    {
        const Shared<GameObject> currentPreviewRoot = Resolve_RootPreviewObject(_liveModelCom->Get_Owner());
        if (currentPreviewRoot != previewRoot)
            return;
    }

    const Shared<ModelCom> previousPlaybackModel = Get_PlaybackModel();
    const float currentTimeSec = previousPlaybackModel != nullptr
        ? previousPlaybackModel->Get_CurrentAnimationTimeSec()
        : 0.f;

    Reset_PreviewTrailEffect();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewWeaponEchoStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewScreenFxCueStates();
    Reset_PreviewAnimSpeedStates();

    Refresh_LiveModelTargets();
    Rebuild_PreviewClone(); // Prefab_View의 Change Weapon Preview 파트 변경을 Animation View 복제본에 다시 태운다.
    Sync_PreviewAnimationSelection(false);

    if (Shared<ModelCom> playbackModel = Get_PlaybackModel())
    {
        playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
        playbackModel->Play_Animation(0.f);
        _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
    }
}

void Animation_View::Rebuild_PreviewClone()
{
    Reset_PreviewClone();

    if (_liveModelCom == nullptr)
        return;

    const Shared<GameObject> liveOwnerObject = _liveModelCom->Get_Owner();
    const Shared<GameObject> liveRootObject = Resolve_RootPreviewObject(liveOwnerObject);
    if (liveOwnerObject == nullptr || liveRootObject == nullptr)
        return;

    if (!PreviewObjectHelpers::Try_BuildGameObjectKey(liveRootObject, liveOwnerObject, _previewOwnerObjectKey))
        return;

    _previewRootObject = GAME->Instantiate_GameObjectJson(liveRootObject->To_Json());
    if (_previewRootObject == nullptr)
        return;

    PreviewObjectHelpers::Disable_PreviewAnimNotifyExecution(_previewRootObject);
    PreviewObjectHelpers::Prime_PreviewObject(_previewRootObject);

    const Shared<Client::Player_BattleBase> liveBattlePlayer = dynamic_pointer_cast<Client::Player_BattleBase>(liveRootObject);
    const Shared<Client::Player_BattleBase> previewBattlePlayer = dynamic_pointer_cast<Client::Player_BattleBase>(_previewRootObject);
    if (liveBattlePlayer != nullptr && previewBattlePlayer != nullptr)
    {
        const string previewWeaponId = liveBattlePlayer->Get_ActiveWeaponPreviewId();
        if (!previewWeaponId.empty())
            previewBattlePlayer->Apply_WeaponPreviewById(previewWeaponId); // PB To_Json은 Part_Weapon*을 제외하므로 복제본에 현재 프리뷰 무기를 다시 붙인다.
    }

    Lock_PreviewObjectToOrigin(_previewRootObject);

    const Shared<GameObject> previewOwnerObject =
        PreviewObjectHelpers::Find_GameObjectByKey(_previewRootObject, _previewOwnerObjectKey);
    _previewModelCom =
        PreviewObjectHelpers::Find_ModelComponentBySourcePath(previewOwnerObject, _sourceModelPath);
}

void Animation_View::Reset_PreviewClone()
{
    Reset_PreviewAnimSpeedStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewScreenFxCueStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponEchoStates();
    _previewRootObject.reset();
    _previewModelCom.reset();
    _previewOwnerObjectKey.clear();
}

int32 Animation_View::Get_CurrentAnimationFps() const
{
    if (const AnimNotifyData* animationData = Get_CurrentAnimation())
        return max(1, animationData->displayFps);

    return 60;
}

void Animation_View::Refresh_PreviewTrailAnchorAsset()
{
    Reset_PreviewTrailEffect(); // ?熬곣뫗????戮?뎽 runtime???깅굵 ?????彛????깅쾳 preview frame????trail???????partTag ?リ옇????怨쀬Ŧ ???곕뻣 ??諛댁뎽??類ｋ펲.
    Reset_PreviewSourceHistoryEffect();
}

float Animation_View::Get_CurrentAnimationLengthSec() const
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel != nullptr)
        return playbackModel->Get_CurrentAnimationDurationSec();

    return Resolve_AnimNotifyDataLengthSec(Get_CurrentAnimation());
}

int32 Animation_View::Get_CurrentAnimationFrameMax() const
{
    return max(1, static_cast<int32>(std::round(Get_CurrentAnimationLengthSec() * static_cast<float>(Get_CurrentAnimationFps()))));
}

void Animation_View::Refresh_Source()
{
    _availableAnimations.clear();

    if (_sourceMode == SourceMode::LiveModel)
    {
        Refresh_LiveAnimationList();
    }
    else
    {
        Refresh_AssetAnimationList();
    }

    if (_currentAnimationName.empty() && !_availableAnimations.empty())
    {
        _currentAnimationName = _availableAnimations.front().displayName;
    }
    else if (!_currentAnimationName.empty())
    {
        if (const AnimationEntry* foundEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName))
        {
            _currentAnimationName = foundEntry->displayName;
        }
    }
}

void Animation_View::Refresh_AssetAnimationList()
{
    const vector<AnimationNameUtils::AssetAnimationInfo> assetInfos =
        AnimationNameUtils::Collect_AssetAnimationInfos(_sourceModelPath);
    _availableAnimations.reserve(assetInfos.size());

    for (const auto& assetInfo : assetInfos)
    {
        AnimationEntry entry{};
        entry.name = assetInfo.stem;
        entry.displayName = assetInfo.displayName;
        entry.playbackName = assetInfo.stem;
        _availableAnimations.push_back(move(entry));
    }
}

void Animation_View::Refresh_LiveAnimationList()
{
    if (_liveModelCom == nullptr)
        return;

    const uint32 numAnimations = static_cast<uint32>(_liveModelCom->Get_NumAnimations());
    const vector<AnimationNameUtils::AssetAnimationInfo> assetInfos =
        AnimationNameUtils::Collect_AssetAnimationInfos(_liveModelCom->Get_SourceModelPath());
    vector<string> playbackNames{};
    playbackNames.reserve(numAnimations);

    for (uint32 i = 0; i < numAnimations; ++i)
    {
        if (const char* animationName = _liveModelCom->Get_AnimationName(i))
            playbackNames.push_back(animationName);
    }

    if (!assetInfos.empty())
    {
        vector<bool> usedPlayback(playbackNames.size(), false);
        _availableAnimations.reserve(assetInfos.size() + playbackNames.size());

        for (const auto& assetInfo : assetInfos)
        {
            AnimationEntry entry{};
            entry.name = assetInfo.stem;
            entry.displayName = assetInfo.displayName;
            entry.playbackName = assetInfo.stem;

            const int32 matchedPlaybackIndex =
                AnimationNameUtils::Find_BestPlaybackMatchIndex(assetInfo, playbackNames, usedPlayback);
            if (matchedPlaybackIndex >= 0)
            {
                entry.playbackName = playbackNames[matchedPlaybackIndex];
                usedPlayback[matchedPlaybackIndex] = true;
            }

            _availableAnimations.push_back(move(entry));
        }

        for (int32 i = 0; i < static_cast<int32>(playbackNames.size()); ++i)
        {
            if (i < static_cast<int32>(usedPlayback.size()) && usedPlayback[i])
                continue;

            const string& playbackName = playbackNames[i];

            AnimationEntry entry{};
            entry.name = playbackName;
            entry.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackName);
            entry.playbackName = playbackName;
            _availableAnimations.push_back(move(entry));
        }

        return;
    }

    for (const string& playbackName : playbackNames)
    {
        AnimationEntry entry{};
        entry.name = playbackName;
        entry.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackName);
        entry.playbackName = playbackName;
        _availableAnimations.push_back(move(entry));
    }
}

void Animation_View::Refresh_LiveModelTargets()
{
    wstring previousObjectKey{};
    wstring previousComponentTag{};
    wstring previousSourceModelPath{};
    if (_selectedLiveModelTargetIndex >= 0 &&
        _selectedLiveModelTargetIndex < static_cast<int32>(_liveModelTargets.size()))
    {
        const LiveModelTarget& previousTarget = _liveModelTargets[_selectedLiveModelTargetIndex];
        previousObjectKey = previousTarget.objectKey;
        previousComponentTag = previousTarget.componentTag;
        previousSourceModelPath = previousTarget.sourceModelPath;
    }

    _liveModelTargets.clear();
    _selectedLiveModelTargetIndex = -1;

    if (_sourceMode != SourceMode::LiveModel || _liveModelCom == nullptr)
        return;

    const Shared<GameObject> liveRootObject = Resolve_RootPreviewObject(_liveModelCom->Get_Owner());
    if (liveRootObject == nullptr)
        return;

    function<void(const Shared<GameObject>&, const wstring&)> collectRecursive =
        [&](const Shared<GameObject>& gameObject, const wstring& objectKey)
    {
        if (gameObject == nullptr)
            return;

        gameObject->Visit_Components([&](const wstring& componentTag, const Shared<Component>& component)
        {
            Shared<ModelCom> modelCom = dynamic_pointer_cast<ModelCom>(component);
            if (modelCom == nullptr)
                return;

            LiveModelTarget target{};
            target.objectKey = objectKey;
            target.componentTag = componentTag;
            target.sourceModelPath = modelCom->Get_SourceModelPath();
            target.model = modelCom;

            string label = String::ToString(objectKey.empty() ? L"Root" : objectKey);
            if (!componentTag.empty())
                label += " / " + String::ToString(componentTag);

            const ClassReflectionInfo& ownerInfo = gameObject->Get_ReflectionInfo();
            if (!ownerInfo.displayName.empty())
                label += "  [" + ownerInfo.displayName + "]";
            else if (!ownerInfo.className.empty())
                label += "  [" + ownerInfo.className + "]";

            vector<string> badges{};
            if (modelCom->Get_NumAnimations() > 0)
                badges.push_back("Anim " + to_string(modelCom->Get_NumAnimations()));
            if (!modelCom->Get_Modifiers().empty())
                badges.push_back("Modifier " + to_string(modelCom->Get_Modifiers().size()));

            if (!badges.empty())
            {
                label += "  (";
                for (size_t i = 0; i < badges.size(); ++i)
                {
                    if (i > 0)
                        label += ", ";
                    label += badges[i];
                }
                label += ")";
            }

            target.label = move(label);

            const bool matchesPrevious =
                !previousObjectKey.empty() &&
                target.objectKey == previousObjectKey &&
                target.componentTag == previousComponentTag &&
                target.sourceModelPath == previousSourceModelPath;

            if (matchesPrevious || (_selectedLiveModelTargetIndex < 0 && modelCom == _liveModelCom))
                _selectedLiveModelTargetIndex = static_cast<int32>(_liveModelTargets.size());

            _liveModelTargets.push_back(move(target));
        });

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            if (partObject == nullptr)
                continue;

            const wstring childKey = objectKey + L"/" + partTag;
            collectRecursive(partObject, childKey);
        }
    };

    collectRecursive(liveRootObject, L"Root");
}

void Animation_View::Draw_LiveModelTargetSelector()
{
    if (_sourceMode != SourceMode::LiveModel || _liveModelTargets.size() <= 1)
        return;

    if (_selectedLiveModelTargetIndex < 0 ||
        _selectedLiveModelTargetIndex >= static_cast<int32>(_liveModelTargets.size()) ||
        _liveModelTargets[_selectedLiveModelTargetIndex].model == nullptr)
    {
        Refresh_LiveModelTargets();
    }

    if (_liveModelTargets.size() <= 1)
        return;

    const char* previewLabel = "(select live model)";
    if (_selectedLiveModelTargetIndex >= 0 &&
        _selectedLiveModelTargetIndex < static_cast<int32>(_liveModelTargets.size()))
    {
        previewLabel = _liveModelTargets[_selectedLiveModelTargetIndex].label.c_str();
    }

    ImGui::SeparatorText("Modifier Model");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##LiveModelTarget", previewLabel))
    {
        for (int32 i = 0; i < static_cast<int32>(_liveModelTargets.size()); ++i)
        {
            const LiveModelTarget& target = _liveModelTargets[i];
            const bool isSelected = (i == _selectedLiveModelTargetIndex);
            if (ImGui::Selectable(target.label.c_str(), isSelected))
                Switch_LiveModelTarget(static_cast<size_t>(i));

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

void Animation_View::Switch_LiveModelTarget(size_t targetIndex)
{
    if (targetIndex >= _liveModelTargets.size())
        return;

    const Shared<ModelCom> selectedModel = _liveModelTargets[targetIndex].model;
    if (selectedModel == nullptr)
        return;

    Cancel_NotifyTrailBonePick();
    _selectedLiveModelTargetIndex = static_cast<int32>(targetIndex);
}

Shared<ModelCom> Animation_View::Resolve_LiveModelTargetModel(const LiveModelTarget& target) const
{
    Shared<ModelCom> targetModel = target.model;

    if (_previewRootObject != nullptr)
    {
        const Shared<GameObject> previewOwner =
            PreviewObjectHelpers::Find_GameObjectByKey(_previewRootObject, target.objectKey);
        Shared<ModelCom> previewModel{};
        if (previewOwner != nullptr && !target.componentTag.empty())
            previewModel = previewOwner->Get_Component<ModelCom>(target.componentTag);

        if (previewModel == nullptr)
            previewModel = PreviewObjectHelpers::Find_ModelComponentBySourcePath(previewOwner, target.sourceModelPath);

        if (previewModel != nullptr)
            targetModel = previewModel;
    }

    return targetModel;
}

Shared<ModelCom> Animation_View::Resolve_ModifierCollisionModel(
    const Shared<ModelCom>& modifierModel,
    const vector<string>& requiredBoneNames) const
{
    if (requiredBoneNames.empty())
        return nullptr;

    Shared<ModelCom> bestModel{};
    int32 bestMatchCount = 0;
    const auto testCandidate = [&](const Shared<ModelCom>& candidate)
    {
        if (candidate == nullptr)
            return;

        int32 matchCount = 0;
        for (const string& boneName : requiredBoneNames)
        {
            if (candidate->Get_BoneIndex(boneName) >= 0)
                ++matchCount;
        }

        if (matchCount > bestMatchCount)
        {
            bestMatchCount = matchCount;
            bestModel = candidate;
        }
    };

    testCandidate(modifierModel);

    if (_sourceMode == SourceMode::LiveModel)
    {
        for (const LiveModelTarget& target : _liveModelTargets)
            testCandidate(Resolve_LiveModelTargetModel(target));
    }

    return bestMatchCount > 0 ? bestModel : nullptr;
}

void Animation_View::Sync_ModifierCollisionSource(const Shared<ModelCom>& modifierModel) const
{
    if (modifierModel == nullptr)
        return;

    const Shared<GameObject> modifierOwner = modifierModel->Get_Owner();
    const Shared<PartObject> modifierPart = dynamic_pointer_cast<PartObject>(modifierOwner);
    if (modifierPart == nullptr)
        return;

    for (const Shared<AnimationModifier>& modifier : modifierModel->Get_Modifiers())
    {
        if (auto* poseFollowModifier = dynamic_cast<PoseFollowChainModifier*>(modifier.get()))
        {
            auto* desc = static_cast<PoseFollowChainModifier::PoseFollowChainModifierDesc*>(
                poseFollowModifier->Get_RuntimeReflectionTarget());
            if (desc == nullptr || desc->physicsAssetPath.empty())
                continue;

            const ModelCom* collisionModel = desc->collisionModel;
            Shared<PartObject> collisionPart{};
            if (collisionModel != nullptr)
            {
                collisionPart = dynamic_pointer_cast<PartObject>(collisionModel->Get_Owner());
            }

            if (collisionModel == nullptr || collisionPart == nullptr)
            {
                vector<string> requiredBoneNames = desc->includedCollisionBones;
                if (requiredBoneNames.empty())
                {
                    for (const auto& capsule : ModelPhysicsParser::LoadCapsulesFromJSON(desc->physicsAssetPath))
                        requiredBoneNames.push_back(capsule.boneName);
                }

                const Shared<ModelCom> resolvedCollisionModel = Resolve_ModifierCollisionModel(modifierModel, requiredBoneNames);
                if (resolvedCollisionModel == nullptr)
                    continue;

                collisionModel = resolvedCollisionModel.get();
                collisionPart = dynamic_pointer_cast<PartObject>(resolvedCollisionModel->Get_Owner());
                if (collisionPart == nullptr)
                    continue;
            }

            desc->collisionModel = collisionModel;
            desc->collisionOwnerWorldMatrix = &collisionPart->Get_CombinedWorldMatrix();
            desc->ownerWorldMatrix = &modifierPart->Get_CombinedWorldMatrix();
            if (desc->collisionPrimitiveScale >= 0.999f || fabsf(desc->collisionPrimitiveScale - 0.08f) <= 0.001f)
                desc->collisionPrimitiveScale = 0.025f;
            if (fabsf(desc->particleCollisionRadius - 0.75f) <= 0.001f)
                desc->particleCollisionRadius = 0.25f;
            if (fabsf(desc->collisionRadiusPadding - 0.25f) <= 0.001f)
                desc->collisionRadiusPadding = 0.05f;
            if (desc->collisionOutputAlpha <= 0.001f)
                desc->collisionOutputAlpha = 0.55f;
            desc->enableCollision = true;
            continue;
        }

        if (auto* physicsModifier = dynamic_cast<PhysicsModifier*>(modifier.get()))
        {
            auto* desc = static_cast<PhysicsModifier::PhysicsModifierDesc*>(
                physicsModifier->Get_RuntimeReflectionTarget());
            if (desc == nullptr || desc->physicsAssetPath.empty())
                continue;

            const ModelCom* collisionModel = desc->collisionModel;
            Shared<PartObject> collisionPart{};
            if (collisionModel != nullptr)
            {
                collisionPart = dynamic_pointer_cast<PartObject>(collisionModel->Get_Owner());
            }

            if (collisionModel == nullptr || collisionPart == nullptr)
            {
                vector<string> requiredBoneNames = desc->includedCollisionBones;
                if (requiredBoneNames.empty())
                {
                    for (const auto& capsule : ModelPhysicsParser::LoadCapsulesFromJSON(desc->physicsAssetPath))
                        requiredBoneNames.push_back(capsule.boneName);
                }

                const Shared<ModelCom> resolvedCollisionModel = Resolve_ModifierCollisionModel(modifierModel, requiredBoneNames);
                if (resolvedCollisionModel == nullptr)
                    continue;

                collisionModel = resolvedCollisionModel.get();
                collisionPart = dynamic_pointer_cast<PartObject>(resolvedCollisionModel->Get_Owner());
                if (collisionPart == nullptr)
                    continue;
            }

            desc->collisionModel = collisionModel;
            desc->collisionOwnerWorldMatrix = &collisionPart->Get_CombinedWorldMatrix();
            desc->ownerWorldMatrix = &modifierPart->Get_CombinedWorldMatrix();
            
        }
    }
}

void Animation_View::Load_NotifyAsset()
{
    _asset = {};
    _asset.modelGuid = _modelGuid;

    AnimNotify_Parser::Load_ModelAsset(_modelGuid, _asset);

    if (_asset.modelGuid.empty())
        _asset.modelGuid = _modelGuid;

    if (_currentAnimationName.empty())
        return;

    if (const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName))
    {
        Find_OrNormalize_NotifyAnimation(_asset, *currentEntry);
        return;
    }

    AnimNotify_Parser::Get_OrAddAnimation(_asset, _currentAnimationName);
}

void Animation_View::Load_TrailAnchorAsset()
{
    _trailAnchorAsset = {};

    if (_modelGuid.empty())
        return;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(_modelGuid);
    (void)Client::TrailAnchor_Parser::Load_FromFile(filePath.wstring(), _trailAnchorAsset);

    if (_trailAnchorAsset.modelGuid.empty())
        _trailAnchorAsset.modelGuid = _modelGuid;
}

bool Animation_View::Load_PreviewTrailAnchorAsset(
    const string& partTag,
    PreviewTrailRuntime& outRuntime)
{
    outRuntime.trailAnchorAsset = {};
    outRuntime.modelGuid.clear();

    if (outRuntime.trailAnchorSource == Client::ANS_Trail::TrailAnchorSource::NotifyBonePair)
    {
        outRuntime.trailAnchorAsset = {};
        outRuntime.trailAnchorAsset.modelGuid.clear();
        outRuntime.trailAnchorAsset.base.boneName = outRuntime.baseBoneName;
        outRuntime.trailAnchorAsset.base.localPosition = outRuntime.baseLocalOffset;
        outRuntime.trailAnchorAsset.tip.boneName = outRuntime.tipBoneName;
        outRuntime.trailAnchorAsset.tip.localPosition = outRuntime.tipLocalOffset;
        return true;
    }

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewTrailPartTag = String::ToWString(partTag);
    PartObject* trailPart = ownerContainer->Get_PartObject(previewTrailPartTag);
    if (Resolve_PreviewTrailPartModel(trailPart) == nullptr)
        return false;

    const Shared<ModelCom> weaponModel = Resolve_PreviewTrailPartModel(trailPart);
    if (weaponModel == nullptr)
        return false;

    const wstring& sourceModelPath = weaponModel->Get_SourceModelPath();
    if (sourceModelPath.empty())
        return false;

    outRuntime.modelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
    if (outRuntime.modelGuid.empty())
        return false;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(outRuntime.modelGuid);
    (void)Client::TrailAnchor_Parser::Load_FromFile(filePath.wstring(), outRuntime.trailAnchorAsset);

    if (outRuntime.trailAnchorAsset.modelGuid.empty())
        outRuntime.trailAnchorAsset.modelGuid = outRuntime.modelGuid;

    return !outRuntime.trailAnchorAsset.modelGuid.empty();
}

void Animation_View::Save_NotifyAsset()
{
    if (_modelGuid.empty())
        return;

    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const float animationLengthSec = Get_CurrentAnimationLengthSec();
    AnimNotify_Parser::Sort_And_Clamp(*animationData, animationLengthSec);

    if (AnimNotify_Parser::Save_AnimationToSplitFile(_modelGuid, *animationData, animationLengthSec))
    {
        Refresh_OpenModelNotifyAssets();
        _isDirty = false;
        NOTIFY("Saved notify asset: {}", _currentAnimationName);
    }
    else if (EDITOR->Get_Notification())
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(NotifyType::Warning, "Failed to save notify asset: {}", _currentAnimationName);
    }
}

void Animation_View::Save_TrailAnchorAsset() const
{
    if (_modelGuid.empty())
        return;

    Client::TrailAnchorAsset asset = _trailAnchorAsset;
    asset.modelGuid = _modelGuid;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(_modelGuid);
    (void)Client::TrailAnchor_Parser::Save_ToFile(filePath.wstring(), asset);
}

void Animation_View::Refresh_OpenModelNotifyAssets() const
{
    if (_liveModelCom != nullptr)
        _liveModelCom->Invalidate_AnimNotifyAsset();

    if (_sourceModelPath.empty())
        return;

    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);
            Invalidate_AnimNotifyAssetsForObject(gameObject, _sourceModelPath);
        });
}

void Animation_View::Draw_Header()
{
    const char* sourceLabel = "None";
    if (_sourceMode == SourceMode::LiveModel)
        sourceLabel = "LiveModel";
    else if (!_sourceModelPath.empty())
        sourceLabel = "Asset";

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    const char* previewName = currentEntry ? currentEntry->displayName.c_str() : "(select animation)";

    if (ImGui::Button("Save"))
        Save_NotifyAsset();
    ImGui::SameLine();
    ImGui::Text("Model");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _modelGuid.empty() ? "(none)" : _modelGuid.c_str());
    ImGui::SameLine();
    ImGui::Text("| Source");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", sourceLabel);
    ImGui::SameLine();
    ImGui::Text("| Animation");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", previewName);

}

void Animation_View::Draw_AnimationSelector()
{
    if (!ImGui::BeginTabBar("AnimationLeftTabs"))
        return;

    ImGuiTabItemFlags animationClipsFlags = 0;
    if (_hasRequestedAnimationLeftTab && _requestedAnimationLeftTab == AnimationLeftTab::AnimationClips)
        animationClipsFlags |= ImGuiTabItemFlags_SetSelected;

    if (ImGui::BeginTabItem("Animation Clips", nullptr, animationClipsFlags))
    {
        _animationLeftTab = AnimationLeftTab::AnimationClips;
        if (_hasRequestedAnimationLeftTab && _requestedAnimationLeftTab == AnimationLeftTab::AnimationClips)
            _hasRequestedAnimationLeftTab = false;

        ImGui::Text("Animation Clips");
        ImGui::Separator();
        ImGui::InputTextWithHint("##AnimationClipSearch", "Search animation...", _animationSearchBuffer, IM_ARRAYSIZE(_animationSearchBuffer));
        const string searchTextLower = String::ToLowerCopy(string(_animationSearchBuffer));

        if (ImGui::BeginChild("AnimationClipBrowserPanel", ImVec2(0.f, 0.f), true))
        {
            if (_availableAnimations.empty())
            {
                ImGui::TextDisabled("No clips found.");
            }
            else
            {
                for (const AnimationEntry& entry : _availableAnimations)
                {
                    if (!searchTextLower.empty())
                    {
                        const string displayNameLower = String::ToLowerCopy(entry.displayName);
                        const string nameLower = String::ToLowerCopy(entry.name);
                        const string playbackNameLower = String::ToLowerCopy(entry.playbackName);

                        if (displayNameLower.find(searchTextLower) == string::npos &&
                            nameLower.find(searchTextLower) == string::npos &&
                            playbackNameLower.find(searchTextLower) == string::npos)
                        {
                            continue;
                        }
                    }

                    const bool isSelected =
                        (_currentAnimationName == entry.displayName ||
                         _currentAnimationName == entry.name ||
                         _currentAnimationName == entry.playbackName);
                    const string label = Make_AnimationComboLabel(entry);

                    if (ImGui::Selectable(label.c_str(), isSelected))
                    {
                        Cancel_NotifyTrailBonePick();
                        _currentAnimationName = entry.displayName;
                        Find_OrNormalize_NotifyAnimation(_asset, entry);
                        if (!Sync_SkillPhasePreviewContextForAnimation(entry))
                            Clear_SkillPhasePreviewContext();
                        Clear_SelectedNotifySelection();
                        Clear_SelectedStateSelection();
                        _selectedNotifyTrackIndex = 0;
                        _selectedNotifyStateTrackIndex = 0;
                        _sequencerCurrentFrame = 0;
                        _sequencerSelectedRow = -1;
                        _sequencerFirstFrame = 0;
                        _sequencerVisibleFrameCount = 0;
                        Clear_NotifyUndoHistory();

                        Sync_PreviewAnimationSelection();

                        Shared<ModelCom> playbackModel = Get_PlaybackModel();
                        if (_sourceMode == SourceMode::LiveModel && playbackModel != nullptr)
                        {
                            const int32 animationIndex = Find_AnimationIndex(playbackModel, entry.playbackName);
                            if (animationIndex >= 0)
                            {
                                playbackModel->Set_Animation(static_cast<uint32>(animationIndex), true);
                                playbackModel->Set_AnimationPaused(false);
                                playbackModel->Set_CurrentAnimationTimeSec(0.f);
                                playbackModel->Refresh_CurrentAnimationPose();
                            }
                        }
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGuiTabItemFlags boneTreeFlags = 0;
    if (_hasRequestedAnimationLeftTab && _requestedAnimationLeftTab == AnimationLeftTab::BoneTree)
        boneTreeFlags |= ImGuiTabItemFlags_SetSelected;

    if (ImGui::BeginTabItem("Bone Tree", nullptr, boneTreeFlags))
    {
        _animationLeftTab = AnimationLeftTab::BoneTree;
        if (_hasRequestedAnimationLeftTab && _requestedAnimationLeftTab == AnimationLeftTab::BoneTree)
            _hasRequestedAnimationLeftTab = false;

        Draw_BoneTree();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

void Animation_View::Draw_BoneTree()
{
    ImGui::Text("Bone Tree");
    ImGui::Separator();

    if (Is_NotifyTrailBonePicking() && Find_NotifyTrailPickTarget() == nullptr)
        Cancel_NotifyTrailBonePick();

    if (Is_NotifyTrailBonePicking())
    {
        const bool isBaseStep =
            _notifyTrailBonePickMode == NotifyTrailBonePickMode::Base ||
            _notifyTrailBonePickMode == NotifyTrailBonePickMode::PairBase;
        const bool isPairMode =
            _notifyTrailBonePickMode == NotifyTrailBonePickMode::PairBase ||
            _notifyTrailBonePickMode == NotifyTrailBonePickMode::PairTip;

        ImGui::TextDisabled(
            "%s",
            isPairMode
                ? (isBaseStep ? "Picking Base Bone (1/2)" : "Picking Tip Bone (2/2)")
                : (isBaseStep ? "Picking Base Bone" : "Picking Tip Bone"));
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel##NotifyTrailBonePickCancel") ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            Cancel_NotifyTrailBonePick();
        }
        ImGui::Separator();
    }

    ImGui::InputTextWithHint("##BoneSearch", "Search bone...", _boneSearchBuffer, IM_ARRAYSIZE(_boneSearchBuffer));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 10.f);

    if (!ImGui::BeginChild("AnimationBoneTreePanel", ImVec2(0.f, 0.f), true))
    {
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
    {
        ImGui::TextDisabled("Bone tree is available in live preview mode.");
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    Validate_SelectedBoneSelection();

    const uint32 numBones = static_cast<uint32>(playbackModel->Get_NumBones());
    if (numBones == 0)
    {
        ImGui::TextDisabled("No bones found.");
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    vector<vector<uint32>> children(numBones);
    vector<uint32> rootBones{};
    rootBones.reserve(numBones);

    for (uint32 boneIndex = 0; boneIndex < numBones; ++boneIndex)
    {
        const int32 parentIndex = playbackModel->Get_BoneParentIndex(boneIndex);
        if (parentIndex >= 0 && parentIndex < static_cast<int32>(numBones))
            children[parentIndex].push_back(boneIndex);
        else
            rootBones.push_back(boneIndex);
    }

    const string filterLower = String::ToLowerCopy(string(_boneSearchBuffer));

    const auto branchMatches =
        [&](const auto& self, uint32 boneIndex) -> bool
    {
        const string boneNameLower = String::ToLowerCopy(playbackModel->Get_BoneName(boneIndex));
        if (filterLower.empty() || boneNameLower.find(filterLower) != string::npos)
            return true;

        for (uint32 childIndex : children[boneIndex])
        {
            if (self(self, childIndex))
                return true;
        }

        return false;
    };

    const auto renderNode =
        [&](const auto& self, uint32 boneIndex) -> void
    {
        if (!filterLower.empty() && !branchMatches(branchMatches, boneIndex))
            return;

        const bool isSelected = (_selectedBoneIndex == static_cast<int32>(boneIndex));
        const bool forceOpen = !filterLower.empty() && !children[boneIndex].empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children[boneIndex].empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (forceOpen)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        else if (!children[boneIndex].empty())
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        const string nodeLabel =
            playbackModel->Get_BoneName(boneIndex) + " [" + to_string(boneIndex) + "]";
        const string popupId = "AnimationBoneContextMenu_" + to_string(boneIndex);
        ImGui::PushID(static_cast<int>(boneIndex));
        const bool opened = ImGui::TreeNodeEx("BoneNode", flags, "%s", nodeLabel.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            _selectedBoneIndex = static_cast<int32>(boneIndex);
            if (!Consume_NotifyTrailBonePick(boneIndex))
            {
                Clear_SelectedNotifySelection();
                Clear_SelectedStateSelection();
            }
        }

        if (ImGui::BeginPopupContextItem(popupId.c_str()))
        {
            if (ImGui::MenuItem("Copy Bone Name"))
            {
                ImGui::SetClipboardText(playbackModel->Get_BoneName(boneIndex).c_str());
                _selectedBoneIndex = static_cast<int32>(boneIndex);
                Clear_SelectedNotifySelection();
                Clear_SelectedStateSelection();
            }

            if (ImGui::MenuItem("Set As Trail Base"))
            {
                _selectedBoneIndex = static_cast<int32>(boneIndex);
                Clear_SelectedNotifySelection();
                Clear_SelectedStateSelection();
                Assign_SelectedBoneToTrailAnchor(playbackModel, TrailAnchorSlot::Base);
            }

            if (ImGui::MenuItem("Set As Trail Tip"))
            {
                _selectedBoneIndex = static_cast<int32>(boneIndex);
                Clear_SelectedNotifySelection();
                Clear_SelectedStateSelection();
                Assign_SelectedBoneToTrailAnchor(playbackModel, TrailAnchorSlot::Tip);
            }

            ImGui::EndPopup();
        }

        if (!opened)
        {
            ImGui::PopID();
            return;
        }

        for (uint32 childIndex : children[boneIndex])
            self(self, childIndex);

        ImGui::TreePop();
        ImGui::PopID();
    };

    for (uint32 rootBoneIndex : rootBones)
        renderNode(renderNode, rootBoneIndex);

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

float Animation_View::Get_PlaySectionStartSec(const AnimNotifyData* animationData) const
{
    if (animationData == nullptr)
        return 0.f;

    return std::clamp(animationData->playSectionStartSec, 0.f, max(0.f, Get_CurrentAnimationLengthSec()));
}

float Animation_View::Get_PlaySectionEndSec(const AnimNotifyData* animationData) const
{
    const float animationLengthSec = max(0.f, Get_CurrentAnimationLengthSec());
    if (animationData == nullptr)
        return animationLengthSec;

    const float startSec = Get_PlaySectionStartSec(animationData);
    const float endSec = animationData->playSectionEndSec >= 0.f
        ? animationData->playSectionEndSec
        : animationLengthSec;

    return std::clamp(endSec, startSec, animationLengthSec);
}

void Animation_View::Clamp_PlaySection(AnimNotifyData* animationData)
{
    if (animationData == nullptr)
        return;

    const float animationLengthSec = max(0.f, Get_CurrentAnimationLengthSec());
    animationData->playSectionStartSec = std::clamp(animationData->playSectionStartSec, 0.f, animationLengthSec);

    if (animationData->playSectionEndSec >= 0.f)
        animationData->playSectionEndSec = std::clamp(animationData->playSectionEndSec, animationData->playSectionStartSec, animationLengthSec);
}

void Animation_View::Play_CurrentSection(const Shared<ModelCom>& playbackModel)
{
    if (playbackModel == nullptr)
        return;

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    const float startSec = Get_PlaySectionStartSec(animationData);
    const float endSec = Get_PlaySectionEndSec(animationData);

    if (endSec <= startSec + 0.0001f)
    {
        playbackModel->Set_AnimationPaused(true);
        playbackModel->Set_CurrentAnimationTimeSec(startSec);
        playbackModel->Play_Animation(0.f);
        _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
        _isSectionPreviewPlaying = false;
        return;
    }

    const float currentSec = playbackModel->Get_CurrentAnimationTimeSec();
    if (currentSec < startSec - 0.0001f || currentSec >= endSec - 0.0001f)
    {
        playbackModel->Set_CurrentAnimationTimeSec(startSec);
        playbackModel->Play_Animation(0.f);
        _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
    }

    playbackModel->Set_AnimationPaused(false);
    _previewPlayEffectIncludeStartEdge = true;
    _isSectionPreviewPlaying = true;
}

void Animation_View::Stop_CurrentSection(const Shared<ModelCom>& playbackModel)
{
    if (playbackModel == nullptr)
        return;

    const float startSec = Get_PlaySectionStartSec(Get_CurrentAnimation());
    playbackModel->Set_AnimationPaused(true);
    playbackModel->Set_CurrentAnimationTimeSec(startSec);
    playbackModel->Play_Animation(0.f);
    _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
    _isSectionPreviewPlaying = false;
}

void Animation_View::Draw_PlaybackToolbar()
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const bool hasPlayback = (playbackModel != nullptr);
    const float animationLengthSec = Get_CurrentAnimationLengthSec();
    const float sliderMaxTimeSec = max(0.01f, animationLengthSec);
    const ImVec2 playbackButtonSize(58.f, 0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 4.f));

    if (hasPlayback)
    {
        if (ImGui::Button(playbackModel->Is_AnimationPaused() ? "Play" : "Pause", playbackButtonSize))
        {
            if (playbackModel->Is_AnimationPaused())
                Play_CurrentSection(playbackModel);
            else
            {
                playbackModel->Set_AnimationPaused(true);
                _isSectionPreviewPlaying = false;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Stop", playbackButtonSize))
            Stop_CurrentSection(playbackModel);

        ImGui::SameLine();
        ImGui::TextDisabled("Frame %d / %d", _sequencerCurrentFrame, Get_CurrentAnimationFrameMax());
        ImGui::SameLine(0.f, 12.f);
        ImGui::Checkbox("Grid", &_showPreviewGrid);

        Draw_SkillPhasePreviewControls();

        ImGui::SameLine();
        ImGui::TextDisabled("Time");
        ImGui::SameLine();

        float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
        ImGui::SetNextItemWidth(260.f);
        if (ImGui::SliderFloat("##AnimScrub", &currentTimeSec, 0.f, sliderMaxTimeSec, "%.3f s"))
        {
            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
            playbackModel->Play_Animation(0.f); // ??蹂?뜟?? ??μ쪠???嶺?notify/state嶺??熬곣뫗???熬곣뫚???リ옇????怨쀬Ŧ ???곕뻣 ?????類ｋ펲.
            _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
            _isSectionPreviewPlaying = false;
        }

        AnimNotifyData* animationData = Get_CurrentAnimation();
        if (animationData != nullptr)
        {
            const float sectionLengthSec = max(0.f, animationLengthSec);
            float sectionStartSec = Get_PlaySectionStartSec(animationData);
            float sectionEndSec = Get_PlaySectionEndSec(animationData);
            const float selectedSectionLengthSec = max(0.f, sectionEndSec - sectionStartSec);

            ImGui::Spacing();
            ImGui::PushID("PlaySectionControls");
            ImGui::TextDisabled("Section");
            ImGui::SameLine(0.f, 12.f);

            ImGui::BeginGroup();
            ImGui::TextDisabled("Start");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(74.f);
            const NotifyUndoSnapshot sectionStartUndoSnapshot = Capture_NotifyUndoSnapshot();
            if (ImGui::DragFloat("##SectionStartSec", &sectionStartSec, 0.01f, 0.f, sectionLengthSec, "%.3f"))
            {
                if (!_hasPendingNotifyUndoSnapshot)
                {
                    _pendingNotifyUndoSnapshot = sectionStartUndoSnapshot;
                    _hasPendingNotifyUndoSnapshot = true;
                }
                animationData->playSectionStartSec = std::clamp(sectionStartSec, 0.f, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
                Commit_PendingNotifyUndo();

            ImGui::SameLine();

            if (ImGui::Button("Set##SectionStartSet"))
            {
                const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
                animationData->playSectionStartSec = std::clamp(playbackModel->Get_CurrentAnimationTimeSec(), 0.f, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;
                Commit_NotifyUndoSnapshot(undoSnapshot);

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            ImGui::EndGroup();

            ImGui::SameLine(0.f, 12.f);

            ImGui::BeginGroup();
            ImGui::TextDisabled("End");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(74.f);
            const NotifyUndoSnapshot sectionEndUndoSnapshot = Capture_NotifyUndoSnapshot();
            if (ImGui::DragFloat("##SectionEndSec", &sectionEndSec, 0.01f, 0.f, sectionLengthSec, "%.3f"))
            {
                if (!_hasPendingNotifyUndoSnapshot)
                {
                    _pendingNotifyUndoSnapshot = sectionEndUndoSnapshot;
                    _hasPendingNotifyUndoSnapshot = true;
                }
                animationData->playSectionEndSec = std::clamp(sectionEndSec, animationData->playSectionStartSec, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
                Commit_PendingNotifyUndo();

            ImGui::SameLine();

            if (ImGui::Button("Set##SectionEndSet"))
            {
                const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
                animationData->playSectionEndSec = std::clamp(playbackModel->Get_CurrentAnimationTimeSec(), animationData->playSectionStartSec, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;
                Commit_NotifyUndoSnapshot(undoSnapshot);

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            ImGui::EndGroup();

            ImGui::SameLine(0.f, 12.f);
            ImGui::TextDisabled("Len %.3fs", selectedSectionLengthSec);
            ImGui::PopID();

        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("Play", playbackButtonSize);
        ImGui::SameLine();
        ImGui::Button("Stop", playbackButtonSize);
        ImGui::SameLine();
        ImGui::TextDisabled("Frame %d", _sequencerCurrentFrame);
        ImGui::SameLine(0.f, 12.f);
        ImGui::TextDisabled("Max Frame %d", Get_CurrentAnimationFrameMax());
        ImGui::SameLine(0.f, 12.f);
        ImGui::Checkbox("Grid", &_showPreviewGrid);
        ImGui::SameLine();

        float dummyTimeSec = 0.f;
        ImGui::SetNextItemWidth(220.f);
        ImGui::SliderFloat("##AnimScrub", &dummyTimeSec, 0.f, sliderMaxTimeSec, "%.3f s");
        ImGui::EndDisabled();
    }

    ImGui::PopStyleVar();
}

void Animation_View::Draw_SkillPhasePreviewControls()
{
    if (_sourceMode != SourceMode::LiveModel)
        return;

    const vector<string> skillIds = CLIENT->Get_AllSkillIds();
    if (skillIds.empty())
        return;

    auto makePhaseLabel = [](const Client::BattleActionPhase& phase)
    {
        char label[256]{};
        sprintf_s(
            label,
            "P%u %s x%.2f",
            phase.phaseIndex,
            phase.animationKey.c_str(),
            phase.playRate > 0.f ? phase.playRate : 1.f);
        return string(label);
    };

    ImGui::SameLine(0.f, 12.f);
    ImGui::TextDisabled("Skill");
    ImGui::SameLine();

    const char* skillPreview = _skillPhaseContext.enabled
        ? _skillPhaseContext.skillKey.c_str()
        : "Select Skill";

    ImGui::SetNextItemWidth(190.f);
    if (ImGui::BeginCombo("##SkillPhasePreviewSkill", skillPreview))
    {
        for (const string& skillId : skillIds)
        {
            const Client::Skill_Data* skillData = CLIENT->Get_Skill(skillId);
            const bool hasPhases = skillData != nullptr && !skillData->AnimPhases.empty();
            const bool isSelected = _skillPhaseContext.enabled && _skillPhaseContext.skillKey == skillId;

            if (!hasPhases)
                ImGui::BeginDisabled();

            if (ImGui::Selectable(skillId.c_str(), isSelected) && hasPhases)
                Set_SkillPhasePreviewContext(skillId, skillData->AnimPhases.front().phaseIndex);

            if (!hasPhases)
                ImGui::EndDisabled();

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    const Client::Skill_Data* selectedSkill = _skillPhaseContext.enabled
        ? CLIENT->Get_Skill(_skillPhaseContext.skillKey)
        : nullptr;
    if (selectedSkill != nullptr && !selectedSkill->AnimPhases.empty())
    {
        const Client::BattleActionPhase* selectedPhase = nullptr;
        for (const Client::BattleActionPhase& phase : selectedSkill->AnimPhases)
        {
            if (phase.phaseIndex == _skillPhaseContext.phaseIndex)
            {
                selectedPhase = &phase;
                break;
            }
        }

        const string phasePreview = selectedPhase != nullptr
            ? makePhaseLabel(*selectedPhase)
            : string("Select Phase");

        ImGui::SameLine();
        ImGui::SetNextItemWidth(230.f);
        if (ImGui::BeginCombo("##SkillPhasePreviewPhase", phasePreview.c_str()))
        {
            for (const Client::BattleActionPhase& phase : selectedSkill->AnimPhases)
            {
                const string phaseLabel = makePhaseLabel(phase);
                const bool isSelected = _skillPhaseContext.phaseIndex == phase.phaseIndex;
                if (ImGui::Selectable(phaseLabel.c_str(), isSelected))
                    Set_SkillPhasePreviewContext(_skillPhaseContext.skillKey, phase.phaseIndex);

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
    }

    if (_skillPhaseContext.enabled)
    {
        ImGui::SameLine();
        if (ImGui::Button("Clear##SkillPhasePreview"))
            Clear_SkillPhasePreviewContext();
    }
}

void Animation_View::Draw_PreviewPanel()
{
    ImGui::BeginChild("AnimationPreviewPanel", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::Checkbox("SkyBox", &_showPreviewSkyBackdrop);
    ImGui::SameLine();
    ImGui::Checkbox("사운드", &_previewSoundEnabled);
    ImGui::Separator();

    ImVec2 previewSize = ImGui::GetContentRegionAvail();

    if (previewSize.x < 1.f)
        previewSize.x = 1.f;
    if (previewSize.y < 1.f)
        previewSize.y = 1.f;

    _previewWidth = max(1u, static_cast<uint32>(previewSize.x));
    _previewHeight = max(1u, static_cast<uint32>(previewSize.y));

    const ImVec2 previewPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(
        "AnimationPreviewCanvas",
        previewSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    _isPreviewHovered = ImGui::IsItemHovered();
    _isPreviewActive = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    Shared<ModelCom> playbackModel = Get_PlaybackModel();

    ID3D11ShaderResourceView* previewSRV =
        _previewRenderTarget != nullptr ? _previewRenderTarget->Get_SRV() : nullptr;
    if (previewRenderObject != nullptr && playbackModel != nullptr && previewSRV != nullptr)
    {
        ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSRV));
        drawList->AddImage(
            textureId,
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y));

    }
    else
    {
        drawList->AddRectFilled(
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y),
            IM_COL32(34, 38, 44, 255));

        drawList->AddRect(
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y),
            IM_COL32(85, 95, 110, 255));

        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewPos.y + 12.f),
            IM_COL32(210, 216, 224, 255),
            "Animation preview is not ready.");
    }

    ImGui::EndChild();
}

void Animation_View::Draw_Sequencer()
{
    _notifyEditor->Draw_Sequencer();
}

void Animation_View::Draw_EventListPanel()
{
    _notifyEditor->Draw_EventListPanel();
}

void Animation_View::Draw_NotifyList()
{
    _notifyEditor->Draw_NotifyList();
}

void Animation_View::Draw_StateList()
{
    _notifyEditor->Draw_StateList();
}

void Animation_View::Draw_TrackSection()
{
    _notifyEditor->Draw_TrackSection();
}

void Animation_View::Add_NotifyTrack(const string& trackName)
{
    _notifyEditor->Add_NotifyTrack(trackName);
}

void Animation_View::Add_NotifyStateTrack(const string& trackName)
{
    _notifyEditor->Add_NotifyStateTrack(trackName);
}

void Animation_View::Add_Notify_AtFrame(int32 frame, const string& typeName)
{
    _notifyEditor->Add_Notify_AtFrame(frame, typeName);
}

void Animation_View::Add_State_ByFrameRange(int32 startFrame, int32 endFrame, const string& typeName)
{
    _notifyEditor->Add_State_ByFrameRange(startFrame, endFrame, typeName);
}

void Animation_View::Delete_SelectedNotify()
{
    _notifyEditor->Delete_SelectedNotify();
}

void Animation_View::Delete_SelectedState()
{
    _notifyEditor->Delete_SelectedState();
}

void Animation_View::Delete_SelectedNotifyTrack()
{
    _notifyEditor->Delete_SelectedNotifyTrack();
}

void Animation_View::Delete_SelectedNotifyStateTrack()
{
    _notifyEditor->Delete_SelectedNotifyStateTrack();
}

Animation_View::NotifyUndoSnapshot Animation_View::Capture_NotifyUndoSnapshot() const
{
    return _notifyEditor->Capture_NotifyUndoSnapshot();
}

void Animation_View::Restore_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot)
{
    _notifyEditor->Restore_NotifyUndoSnapshot(snapshot);
}

void Animation_View::Commit_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot)
{
    _notifyEditor->Commit_NotifyUndoSnapshot(snapshot);
}

void Animation_View::Begin_PendingNotifyUndo()
{
    _notifyEditor->Begin_PendingNotifyUndo();
}

void Animation_View::Commit_PendingNotifyUndo()
{
    _notifyEditor->Commit_PendingNotifyUndo();
}

void Animation_View::Clear_NotifyUndoHistory()
{
    _notifyEditor->Clear_NotifyUndoHistory();
}

void Animation_View::Undo_NotifyEdit()
{
    _notifyEditor->Undo_NotifyEdit();
}

void Animation_View::Redo_NotifyEdit()
{
    _notifyEditor->Redo_NotifyEdit();
}

void Animation_View::Handle_ClipboardShortcuts()
{
    _notifyEditor->Handle_ClipboardShortcuts();
}

void Animation_View::Move_SelectedNotifiesByFrame(int32 frameDelta)
{
    _notifyEditor->Move_SelectedNotifiesByFrame(frameDelta);
}

void Animation_View::Move_SelectedStateByFrame(int32 frameDelta)
{
    _notifyEditor->Move_SelectedStateByFrame(frameDelta);
}

void Animation_View::Copy_SelectedNotify()
{
    _notifyEditor->Copy_SelectedNotify();
}

void Animation_View::Copy_SelectedState()
{
    _notifyEditor->Copy_SelectedState();
}

void Animation_View::Paste_CopiedNotify()
{
    _notifyEditor->Paste_CopiedNotify();
}

void Animation_View::Paste_CopiedState()
{
    _notifyEditor->Paste_CopiedState();
}

void Animation_View::Begin_SequencerContextMenu(int32 frame, bool hasTrackContext, bool isStateTrack, int32 trackIndex)
{
    _notifyEditor->Begin_SequencerContextMenu(frame, hasTrackContext, isStateTrack, trackIndex);
}

bool Animation_View::Try_GetSequencerTrackContext(const ImVec2& sequencerRectMin, const ImVec2& mousePos, bool& outIsStateTrack, int32& outTrackIndex) const
{
    return _notifyEditor->Try_GetSequencerTrackContext(sequencerRectMin, mousePos, outIsStateTrack, outTrackIndex);
}

void Animation_View::Draw_CreateNotifyPopup()
{
    _notifyEditor->Draw_CreateNotifyPopup();
}

void Animation_View::Draw_CreateNotifyStatePopup()
{
    _notifyEditor->Draw_CreateNotifyStatePopup();
}

void Animation_View::Draw_SequencerContextMenu()
{
    _notifyEditor->Draw_SequencerContextMenu();
}

AnimNotifyEventEntry* Animation_View::Find_SelectedNotify()
{
    return _notifyEditor->Find_SelectedNotify();
}

AnimNotifyStateEntry* Animation_View::Find_SelectedState()
{
    return _notifyEditor->Find_SelectedState();
}

void Animation_View::Clear_SelectedNotifySelection()
{
    _notifyEditor->Clear_SelectedNotifySelection();
}

void Animation_View::Clear_SelectedStateSelection()
{
    _notifyEditor->Clear_SelectedStateSelection();
}

bool Animation_View::Is_NotifySelected(int32 notifyId) const
{
    return _notifyEditor->Is_NotifySelected(notifyId);
}

bool Animation_View::Is_StateSelected(int32 stateId) const
{
    return _notifyEditor->Is_StateSelected(stateId);
}

AnimNotifyStateEntry* Animation_View::Find_NotifyStateById(int32 stateId)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return nullptr;

    for (AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.id == stateId)
            return &entry;
    }

    return nullptr;
}

Client::ANS_Trail* Animation_View::Find_NotifyTrailPickTarget(int32* outStateTrackIndex)
{
    if (outStateTrackIndex != nullptr)
        *outStateTrackIndex = 0;

    AnimNotifyStateEntry* entry = Find_NotifyStateById(_notifyTrailBonePickStateId);
    if (entry == nullptr || entry->notifyState == nullptr)
        return nullptr;

    if (outStateTrackIndex != nullptr)
        *outStateTrackIndex = entry->trackIndex;

    Client::ANS_Trail* trailNotifyState = dynamic_cast<Client::ANS_Trail*>(entry->notifyState.get());
    if (trailNotifyState == nullptr ||
        trailNotifyState->Get_TrailAnchorSource() != Client::ANS_Trail::TrailAnchorSource::NotifyBonePair)
    {
        return nullptr;
    }

    return trailNotifyState;
}

void Animation_View::Begin_NotifyTrailBonePick(NotifyTrailBonePickMode mode, int32 stateId)
{
    _notifyTrailBonePickMode = mode;
    _notifyTrailBonePickStateId = stateId;
    _notifyTrailBonePickReturnTab = _animationLeftTab;
    _requestedAnimationLeftTab = AnimationLeftTab::BoneTree;
    _hasRequestedAnimationLeftTab = true;
}

void Animation_View::Cancel_NotifyTrailBonePick()
{
    _notifyTrailBonePickMode = NotifyTrailBonePickMode::None;
    _notifyTrailBonePickStateId = -1;
}

void Animation_View::Restore_NotifyTrailPickSelection()
{
    int32 stateTrackIndex = 0;
    if (Find_NotifyTrailPickTarget(&stateTrackIndex) == nullptr)
        return;

    Clear_SelectedNotifySelection();
    _selectedStateIds.clear();
    _selectedStateIds.insert(_notifyTrailBonePickStateId);
    _selectedStateId = _notifyTrailBonePickStateId;
    _selectedNotifyStateTrackIndex = stateTrackIndex;
}

bool Animation_View::Consume_NotifyTrailBonePick(uint32 boneIndex)
{
    if (!Is_NotifyTrailBonePicking())
        return false;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr || boneIndex >= playbackModel->Get_NumBones())
    {
        Cancel_NotifyTrailBonePick();
        return true;
    }

    Client::ANS_Trail* trailNotifyState = Find_NotifyTrailPickTarget();
    if (trailNotifyState == nullptr)
    {
        Cancel_NotifyTrailBonePick();
        return true;
    }

    const string boneName = playbackModel->Get_BoneName(boneIndex);
    Begin_PendingNotifyUndo();

    const bool assignBase =
        _notifyTrailBonePickMode == NotifyTrailBonePickMode::Base ||
        _notifyTrailBonePickMode == NotifyTrailBonePickMode::PairBase;

    if (assignBase)
    {
        trailNotifyState->Set_BaseBoneName(boneName);
        trailNotifyState->Set_BaseLocalOffset(Vec3::Zero);
    }
    else
    {
        trailNotifyState->Set_TipBoneName(boneName);
        trailNotifyState->Set_TipLocalOffset(Vec3::Zero);
    }

    _isDirty = true;
    Commit_PendingNotifyUndo();
    Restore_NotifyTrailPickSelection();

    if (_notifyTrailBonePickMode == NotifyTrailBonePickMode::PairBase)
    {
        _notifyTrailBonePickMode = NotifyTrailBonePickMode::PairTip;
        _requestedAnimationLeftTab = AnimationLeftTab::BoneTree;
        _hasRequestedAnimationLeftTab = true;
        return true;
    }

    _requestedAnimationLeftTab = _notifyTrailBonePickReturnTab;
    _hasRequestedAnimationLeftTab = true;
    Cancel_NotifyTrailBonePick();
    return true;
}

void Animation_View::Draw_SelectedDetail()
{
    ImGui::Text("Selected Detail");
    ImGui::Separator();

    if (!ImGui::BeginChild("AnimationSelectedDetailPanel", ImVec2(0.f, 0.f), true))
    {
        ImGui::EndChild();
        return;
    }

    const float detailMaxTimeSec = max(0.01f, Get_CurrentAnimationLengthSec());
    UNREFERENCED_PARAMETER(detailMaxTimeSec);
    Shared<ModelCom> playbackModel = Get_PlaybackModel();

    if (AnimNotifyEventEntry* notify = Find_SelectedNotify())
    {
        if (notify->notify != nullptr)
        {
            const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
            if (Inspector::Draw_ReflectionSection("SelectedNotify", notify->notify.get(), notify->notify->Get_ReflectionInfo()))
            {
                if (ImGui::IsAnyItemActive())
                {
                    if (!_hasPendingNotifyUndoSnapshot)
                    {
                        _pendingNotifyUndoSnapshot = undoSnapshot;
                        _hasPendingNotifyUndoSnapshot = true;
                    }
                }
                else
                {
                    Commit_NotifyUndoSnapshot(undoSnapshot);
                }

                _isDirty = true;
            }
        }
    }
    else if (AnimNotifyStateEntry* notifyState = Find_SelectedState())
    {
        if (notifyState->notifyState != nullptr)
        {
            const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
            if (Inspector::Draw_ReflectionSection("SelectedNotifyState", notifyState->notifyState.get(), notifyState->notifyState->Get_ReflectionInfo()))
            {
                if (ImGui::IsAnyItemActive())
                {
                    if (!_hasPendingNotifyUndoSnapshot)
                    {
                        _pendingNotifyUndoSnapshot = undoSnapshot;
                        _hasPendingNotifyUndoSnapshot = true;
                    }
                }
                else
                {
                    Commit_NotifyUndoSnapshot(undoSnapshot);
                }

                _isDirty = true;
            }

            if (Client::ANS_Trail* trailNotifyState = dynamic_cast<Client::ANS_Trail*>(notifyState->notifyState.get()))
            {
                Draw_NotifyTrailAnchorPanel(*trailNotifyState);
                Draw_TrailOffsetPanel(*trailNotifyState, *notifyState);
            }
        }
    }
    else if (playbackModel != nullptr && _selectedBoneIndex >= 0)
    {
        Draw_SelectedBoneDetail(playbackModel);
    }
    else
    {
        ImGui::TextDisabled("No selection.");
        ImGui::Spacing();
        ImGui::TextWrapped("Select a Notify, NotifyState, or Bone.");
    }

    Draw_ModifierRuntimePanel(playbackModel);

    ImGui::EndChild();
}

void Animation_View::Draw_NotifyTrailAnchorPanel(Client::ANS_Trail& trailNotifyState)
{
    const AnimNotifyStateEntry* selectedState = Find_SelectedState();
    const int32 selectedStateId = selectedState != nullptr ? selectedState->id : -1;

    auto drawPickButton = [&](const char* label, NotifyTrailBonePickMode mode, bool highlighted) -> void
        {
            const ImVec4 highlightColor(0.95f, 0.45f, 0.12f, 1.f);
            const ImVec4 highlightHoverColor(1.0f, 0.55f, 0.2f, 1.f);
            const ImVec4 highlightActiveColor(0.85f, 0.36f, 0.08f, 1.f);

            if (highlighted)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, highlightColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, highlightHoverColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, highlightActiveColor);
            }

            ImGui::BeginDisabled(selectedStateId < 0);
            if (ImGui::Button(label))
                Begin_NotifyTrailBonePick(mode, selectedStateId);
            ImGui::EndDisabled();

            if (highlighted)
                ImGui::PopStyleColor(3);
        };

    auto drawBoneRow = [&](const char* label, const string& boneName, const char* buttonLabel, NotifyTrailBonePickMode mode, bool missing) -> void
        {
            const ImVec4 missingColor(0.28f, 0.16f, 0.08f, 1.f);
            const float rowHeight = ImGui::GetFrameHeightWithSpacing() * 1.75f;

            if (missing)
                ImGui::PushStyleColor(ImGuiCol_ChildBg, missingColor);

            ImGui::BeginChild(label, ImVec2(0.f, rowHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextUnformatted(label);
            ImGui::SameLine(90.f);
            ImGui::TextDisabled("%s", boneName.empty() ? "(None)" : boneName.c_str());

            const float buttonWidth = ImGui::CalcTextSize(buttonLabel).x + ImGui::GetStyle().FramePadding.x * 2.f;
            const float buttonX = max(90.f, ImGui::GetWindowContentRegionMax().x - buttonWidth);
            ImGui::SameLine(buttonX);
            drawPickButton(buttonLabel, mode, false);
            ImGui::EndChild();

            if (missing)
                ImGui::PopStyleColor();
        };

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Notify Trail Anchor");

    Client::ANS_Trail::TrailAnchorSource source = trailNotifyState.Get_TrailAnchorSource();
    int32 selectedSource = source == Client::ANS_Trail::TrailAnchorSource::NotifyBonePair ? 1 : 0;
    const char* sourceNames[] = { "Model Default", "Notify Bone Pair" };
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::Combo("Source##NotifyTrailAnchorSource", &selectedSource, sourceNames, IM_ARRAYSIZE(sourceNames)))
    {
        Begin_PendingNotifyUndo();
        trailNotifyState.Set_TrailAnchorSource(
            selectedSource == 1
                ? Client::ANS_Trail::TrailAnchorSource::NotifyBonePair
                : Client::ANS_Trail::TrailAnchorSource::ModelDefault);
        _isDirty = true;
        Commit_PendingNotifyUndo();
    }

    source = trailNotifyState.Get_TrailAnchorSource();
    if (source != Client::ANS_Trail::TrailAnchorSource::NotifyBonePair)
    {
        if (Is_NotifyTrailBonePicking() && _notifyTrailBonePickStateId == selectedStateId)
            Cancel_NotifyTrailBonePick();

        ImGui::TextDisabled("Using model-level trail anchor.");
        return;
    }

    const bool baseMissing = trailNotifyState.Get_BaseBoneName().empty();
    const bool tipMissing = trailNotifyState.Get_TipBoneName().empty();
    const bool bothMissing = baseMissing && tipMissing;

    drawPickButton("Pick Pair##NotifyTrailPickPair", NotifyTrailBonePickMode::PairBase, bothMissing);
    ImGui::SameLine();
    ImGui::BeginDisabled(bothMissing);
    if (ImGui::Button("Clear##NotifyTrailClear"))
    {
        Begin_PendingNotifyUndo();
        trailNotifyState.Clear_NotifyBonePair();
        _isDirty = true;
        Commit_PendingNotifyUndo();
    }
    ImGui::EndDisabled();

    drawBoneRow(
        "Base Bone",
        trailNotifyState.Get_BaseBoneName(),
        "Pick Base##NotifyTrailPickBase",
        NotifyTrailBonePickMode::Base,
        baseMissing);

    drawBoneRow(
        "Tip Bone",
        trailNotifyState.Get_TipBoneName(),
        "Pick Tip##NotifyTrailPickTip",
        NotifyTrailBonePickMode::Tip,
        tipMissing);

    Vec3 baseLocal = trailNotifyState.Get_BaseLocalOffset();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat3("Base Local##NotifyTrailBaseLocal", &baseLocal.x, 0.5f))
    {
        Begin_PendingNotifyUndo();
        trailNotifyState.Set_BaseLocalOffset(baseLocal);
        _isDirty = true;
        if (!ImGui::IsAnyItemActive())
            Commit_PendingNotifyUndo();
    }

    Vec3 tipLocal = trailNotifyState.Get_TipLocalOffset();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat3("Tip Local##NotifyTrailTipLocal", &tipLocal.x, 0.5f))
    {
        Begin_PendingNotifyUndo();
        trailNotifyState.Set_TipLocalOffset(tipLocal);
        _isDirty = true;
        if (!ImGui::IsAnyItemActive())
            Commit_PendingNotifyUndo();
    }
}

void Animation_View::Draw_TrailOffsetPanel(Client::ANS_Trail& trailNotifyState, const AnimNotifyStateEntry& notifyStateEntry)
{
    auto applyTipExtendCurve = [&](const Client::ANS_Trail::TipExtendCurve& curve, bool commitNow = false)
        {
            Begin_PendingNotifyUndo();
            trailNotifyState.Set_TipExtendCurve(curve);
            _isDirty = true;
            if (commitNow || !ImGui::IsAnyItemActive())
                Commit_PendingNotifyUndo();
        };

    auto applyBaseLocalOffsetCurve = [&](const Client::ANS_Trail::LocalOffsetCurve& curve, bool commitNow = false)
        {
            Begin_PendingNotifyUndo();
            trailNotifyState.Set_BaseLocalOffsetCurve(curve);
            _isDirty = true;
            if (commitNow || !ImGui::IsAnyItemActive())
                Commit_PendingNotifyUndo();
        };

    auto applyTipLocalOffsetCurve = [&](const Client::ANS_Trail::LocalOffsetCurve& curve, bool commitNow = false)
        {
            Begin_PendingNotifyUndo();
            trailNotifyState.Set_TipLocalOffsetCurve(curve);
            _isDirty = true;
            if (commitNow || !ImGui::IsAnyItemActive())
                Commit_PendingNotifyUndo();
        };

    const Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const float playheadTimeSec = playbackModel != nullptr
        ? playbackModel->Get_CurrentAnimationTimeSec()
        : static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(max(1, Get_CurrentAnimationFps()));
    const float currentPhase = std::clamp(
        (playheadTimeSec - notifyStateEntry.startSec) / max(0.0001f, notifyStateEntry.durationSec),
        0.f,
        1.f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Trail Offset");

    int32 selectedMode = static_cast<int32>(Resolve_TrailOffsetMode(trailNotifyState));
    const char* modeNames[] = { "None", "Tip Extend", "Advanced" };
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::Combo("Mode", &selectedMode, modeNames, IM_ARRAYSIZE(modeNames)))
    {
        Begin_PendingNotifyUndo();
        const TrailOffsetMode newMode = static_cast<TrailOffsetMode>(selectedMode);
        if (newMode == TrailOffsetMode::None)
        {
            trailNotifyState.Set_TipExtendCurve({});
            trailNotifyState.Set_BaseLocalOffsetCurve({});
            trailNotifyState.Set_TipLocalOffsetCurve({});
        }
        else if (newMode == TrailOffsetMode::TipExtend)
        {
            trailNotifyState.Set_TipExtendCurve(Make_DefaultTipExtendCurve());
            trailNotifyState.Set_BaseLocalOffsetCurve({});
            trailNotifyState.Set_TipLocalOffsetCurve({});
        }
        else
        {
            trailNotifyState.Set_TipLocalOffsetCurve(Make_DefaultLocalOffsetCurve(true));
        }

        _isDirty = true;
        Commit_PendingNotifyUndo();
    }

    const TrailOffsetMode mode = static_cast<TrailOffsetMode>(selectedMode);
    if (mode == TrailOffsetMode::None)
        return;

    if (mode == TrailOffsetMode::TipExtend)
    {
        Client::ANS_Trail::TipExtendCurve curve = trailNotifyState.Get_TipExtendCurve();
        if (!curve.enabled || curve.keys.empty())
        {
            curve = Make_DefaultTipExtendCurve();
            applyTipExtendCurve(curve, true);
        }

        float startValue = Evaluate_TipExtendCurve(curve, 0.f);
        float peakPhase = 0.5f;
        float peakValue = Evaluate_TipExtendCurve(curve, peakPhase);
        float endValue = Evaluate_TipExtendCurve(curve, 1.f);
        Client::ANS_Trail::TipExtendInterpolation interpolation = Client::ANS_Trail::TipExtendInterpolation::Linear;

        for (const Client::ANS_Trail::TipExtendKey& key : curve.keys)
        {
            if (key.phase > kTrailOffsetDuplicatePhaseEpsilon && key.phase < 1.f - kTrailOffsetDuplicatePhaseEpsilon)
            {
                peakPhase = key.phase;
                peakValue = key.value;
                interpolation = key.interpolation;
                break;
            }
        }

        bool simpleChanged = false;
        ImGui::SetNextItemWidth(120.f);
        simpleChanged |= ImGui::DragFloat("Start", &startValue, 1.f, -10000.f, 10000.f, "%.1f");
        ImGui::SetNextItemWidth(120.f);
        simpleChanged |= ImGui::DragFloat("Peak", &peakValue, 1.f, -10000.f, 10000.f, "%.1f");
        ImGui::SetNextItemWidth(120.f);
        simpleChanged |= ImGui::DragFloat("End", &endValue, 1.f, -10000.f, 10000.f, "%.1f");
        ImGui::SetNextItemWidth(120.f);
        simpleChanged |= ImGui::DragFloat("Peak @", &peakPhase, 0.005f, 0.f, 1.f, "%.3f");
        interpolation = Draw_TrailOffsetInterpolationCombo("Interp", interpolation, simpleChanged);

        if (simpleChanged)
        {
            peakPhase = std::clamp(peakPhase, 0.f, 1.f);
            Client::ANS_Trail::TipExtendCurve simpleCurve{};
            simpleCurve.enabled = true;
            simpleCurve.keys = {
                { 0.f, startValue, interpolation },
                { peakPhase, peakValue, interpolation },
                { 1.f, endValue, interpolation }
            };
            applyTipExtendCurve(simpleCurve);
            curve = trailNotifyState.Get_TipExtendCurve();
        }

        if (ImGui::Button("+ Current"))
        {
            Client::ANS_Trail::TipExtendCurve editedCurve = curve;
            editedCurve.enabled = true;
            Upsert_TipExtendKey(editedCurve, currentPhase, Evaluate_TipExtendCurve(curve, currentPhase));
            applyTipExtendCurve(editedCurve, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
            applyTipExtendCurve(Make_DefaultTipExtendCurve(), true);

        Client::ANS_Trail::TipExtendCurve listCurve = trailNotifyState.Get_TipExtendCurve();
        if (ImGui::TreeNodeEx("Tip Keys", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (size_t keyIndex = 0; keyIndex < listCurve.keys.size();)
            {
                Client::ANS_Trail::TipExtendKey& key = listCurve.keys[keyIndex];
                bool keyChanged = false;
                bool eraseKey = false;

                ImGui::PushID(static_cast<int32>(keyIndex));
                ImGui::SetNextItemWidth(70.f);
                keyChanged |= ImGui::DragFloat("P", &key.phase, 0.005f, 0.f, 1.f, "%.3f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.f);
                keyChanged |= ImGui::DragFloat("V", &key.value, 1.f, -10000.f, 10000.f, "%.1f");
                ImGui::SameLine();
                key.interpolation = Draw_TrailOffsetInterpolationCombo("##Interp", key.interpolation, keyChanged);
                ImGui::SameLine();
                eraseKey = ImGui::SmallButton("X");
                ImGui::PopID();

                if (eraseKey)
                {
                    listCurve.keys.erase(listCurve.keys.begin() + static_cast<ptrdiff_t>(keyIndex));
                    applyTipExtendCurve(listCurve, true);
                    continue;
                }

                if (keyChanged)
                    applyTipExtendCurve(listCurve);

                ++keyIndex;
            }

            ImGui::TreePop();
        }
    }
    else if (mode == TrailOffsetMode::Advanced)
    {
        auto drawLocalCurve = [&](const char* label, bool isBaseCurve)
            {
                Client::ANS_Trail::LocalOffsetCurve curve = isBaseCurve
                    ? trailNotifyState.Get_BaseLocalOffsetCurve()
                    : trailNotifyState.Get_TipLocalOffsetCurve();

                if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
                    return;

                bool enabled = curve.enabled;
                if (ImGui::Checkbox("Enabled", &enabled))
                {
                    curve.enabled = enabled;
                    if (curve.enabled && curve.keys.empty())
                        curve = Make_DefaultLocalOffsetCurve(true);
                    if (!curve.enabled)
                        curve.keys.clear();

                    if (isBaseCurve)
                        applyBaseLocalOffsetCurve(curve, true);
                    else
                        applyTipLocalOffsetCurve(curve, true);
                }

                if (curve.enabled || !curve.keys.empty())
                {
                    if (ImGui::Button("+ Current"))
                    {
                        curve.enabled = true;
                        Upsert_LocalOffsetKey(curve, currentPhase, Evaluate_LocalOffsetCurve(curve, currentPhase));
                        if (isBaseCurve)
                            applyBaseLocalOffsetCurve(curve, true);
                        else
                            applyTipLocalOffsetCurve(curve, true);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("+ Key"))
                    {
                        curve.enabled = true;
                        Upsert_LocalOffsetKey(curve, 0.5f, Evaluate_LocalOffsetCurve(curve, 0.5f));
                        if (isBaseCurve)
                            applyBaseLocalOffsetCurve(curve, true);
                        else
                            applyTipLocalOffsetCurve(curve, true);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset"))
                    {
                        curve = {};
                        if (isBaseCurve)
                            applyBaseLocalOffsetCurve(curve, true);
                        else
                            applyTipLocalOffsetCurve(curve, true);
                    }

                    for (size_t keyIndex = 0; keyIndex < curve.keys.size();)
                    {
                        Client::ANS_Trail::LocalOffsetKey& key = curve.keys[keyIndex];
                        bool keyChanged = false;
                        bool eraseKey = false;

                        ImGui::PushID(static_cast<int32>(keyIndex));
                        ImGui::SetNextItemWidth(70.f);
                        keyChanged |= ImGui::DragFloat("P", &key.phase, 0.005f, 0.f, 1.f, "%.3f");
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        keyChanged |= ImGui::DragFloat3("XYZ", &key.value.x, 0.5f, -10000.f, 10000.f, "%.1f");
                        key.interpolation = Draw_TrailOffsetInterpolationCombo("Interp", key.interpolation, keyChanged);
                        ImGui::SameLine();
                        eraseKey = ImGui::SmallButton("X");
                        ImGui::PopID();

                        if (eraseKey)
                        {
                            curve.keys.erase(curve.keys.begin() + static_cast<ptrdiff_t>(keyIndex));
                            if (isBaseCurve)
                                applyBaseLocalOffsetCurve(curve, true);
                            else
                                applyTipLocalOffsetCurve(curve, true);
                            continue;
                        }

                        if (keyChanged)
                        {
                            if (isBaseCurve)
                                applyBaseLocalOffsetCurve(curve);
                            else
                                applyTipLocalOffsetCurve(curve);
                        }

                        ++keyIndex;
                    }
                }

                ImGui::TreePop();
            };

        drawLocalCurve("Base Local", true);
        drawLocalCurve("Tip Local", false);
    }

    if (_hasPendingNotifyUndoSnapshot && !ImGui::IsAnyItemActive())
        Commit_PendingNotifyUndo();
}

void Animation_View::Draw_ModifierRuntimePanel(const Shared<ModelCom>& playbackModel)
{
    ImGui::Separator();
    ImGui::TextUnformatted("Animation Modifiers");

    Draw_LiveModelTargetSelector();

    Shared<ModelCom> modifierModel = Get_ModifierRuntimeModel();
    if (modifierModel == nullptr)
        modifierModel = playbackModel;

    if (modifierModel == nullptr)
    {
        ImGui::TextDisabled("No playback model.");
        return;
    }

    Sync_ModifierCollisionSource(modifierModel);

    const auto& modifiers = modifierModel->Get_Modifiers();
    bool hasEditableModifier = false;

    for (const Shared<AnimationModifier>& modifier : modifiers)
    {
        if (modifier == nullptr)
            continue;

        const ClassReflectionInfo* reflectionInfo = modifier->Get_RuntimeReflectionInfo();
        void* reflectionTarget = modifier->Get_RuntimeReflectionTarget();
        if (reflectionInfo == nullptr || reflectionTarget == nullptr || reflectionInfo->properties.empty())
            continue;

        hasEditableModifier = true;

        const string tag = String::ToString(modifier->Get_ModifierTag());
        const string typeName = reflectionInfo->displayName.empty() ? reflectionInfo->className : reflectionInfo->displayName;
        const string headerLabel = (tag.empty() ? typeName : tag + "  [" + typeName + "]");

        ImGui::PushID(modifier.get());
        if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool active = modifier->Is_Active();
            if (ImGui::Checkbox("Active", &active))
                modifier->Set_Active(active);

            ImGui::SameLine();
            float strengthAlpha = modifier->Get_StrengthAlpha();
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::DragFloat("Strength", &strengthAlpha, 0.01f, 0.f, 1.f))
                modifier->Set_StrengthAlpha(std::clamp(strengthAlpha, 0.f, 1.f));

            if (modifier->Can_RuntimeResetPose())
            {
                if (ImGui::Button("Reset Pose"))
                    modifier->RuntimeResetPose(modifierModel.get());
            }

            if (modifier->Can_RuntimeRebuild())
            {
                ImGui::SameLine();
                if (ImGui::Button("Rebuild Chains"))
                    modifier->RuntimeRebuild(modifierModel.get());
            }

            ImGui::Spacing();
            Draw_ModifierRuntimeProperties(modifier.get(), modifierModel.get(), reflectionInfo, reflectionTarget);

            if (auto* chainModifier = dynamic_cast<ChainModifier*>(modifier.get()))
            {
                const size_t groupCount = chainModifier->Get_RuntimeGroupCount();
                if (groupCount > 0 && ImGui::TreeNode("Group Root Offsets"))
                {
                    ImGui::TextDisabled("Root rotation applies while dragging. Use Rebuild only if bind rest must refresh.");
                    for (size_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
                    {
                        ImGui::PushID(static_cast<int>(groupIndex));

                        const string rootBoneName = chainModifier->Get_RuntimeGroupRootBoneName(groupIndex);
                        ImGui::TextWrapped("%s", rootBoneName.empty() ? "(empty root)" : rootBoneName.c_str());

                        Vec3 offsetDegrees = chainModifier->Get_RuntimeGroupRootRotationOffset(groupIndex);
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragFloat3("Rot", &offsetDegrees.x, 0.25f, -180.f, 180.f))
                            chainModifier->Set_RuntimeGroupRootRotationOffset(groupIndex, offsetDegrees);

                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Preview Motion Test"))
                {
                    auto* chainDesc = static_cast<ChainModifier::ChainModifierDesc*>(reflectionTarget);

                    if (ImGui::Checkbox("Enable Test", &_modifierPreviewMotionTestEnabled))
                    {
                        if (!_modifierPreviewMotionTestEnabled)
                        {
                            _modifierPreviewMotionTestTag.clear();
                            chainModifier->Set_RuntimeTestOwnerMotion(false, Vec3::Zero);
                        }
                        else
                        {
                            _modifierPreviewMotionTestTag = modifier->Get_ModifierTag();
                        }
                    }

                    if (chainDesc != nullptr && chainDesc->motion.maxMovementOffset <= 0.0001f)
                    {
                        ImGui::TextColored(ImVec4(1.f, 0.68f, 0.25f, 1.f), "Move Limit is 0, so test motion is clamped.");
                    }

                    ImGui::TextDisabled("Test adds preview owner motion only; modifier values are unchanged.");
                    if (ImGui::Button("Run Motion Test"))
                    {
                        _modifierPreviewMotionTestEnabled = true;
                        _modifierPreviewMotionTestTag = modifier->Get_ModifierTag();
                        float visibleDelta = 1.f;
                        if (chainDesc != nullptr)
                        {
                            const float resetLimit = chainDesc->runtime.resetDistance > 0.1f ? chainDesc->runtime.resetDistance * 0.8f : 1.f;
                            const float ownerScale = std::max(chainDesc->motion.ownerMovementInertiaScale, 0.001f);
                            visibleDelta = std::clamp(0.05f / ownerScale, 0.25f, resetLimit);
                        }

                        _modifierPreviewMotionTestDelta = visibleDelta;
                        _modifierPreviewMotionTestFrequency = 2.f;
                        _modifierPreviewMotionTestDirection = Vec3(1.f, 0.f, 0.f);
                    }

                    if (chainDesc != nullptr)
                    {
                        const float estimatedMotion = _modifierPreviewMotionTestDelta * chainDesc->motion.ownerMovementInertiaScale;
                        ImGui::TextDisabled("Effective owner input: %.3f", estimatedMotion);
                    }
                    ImGui::TextDisabled("Applied modifiers: %d", _modifierPreviewMotionTestAppliedCount);

                    ImGui::TextDisabled("Direction");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::DragFloat3("##PreviewMotionTestDir", &_modifierPreviewMotionTestDirection.x, 0.01f, -1.f, 1.f);

                    ImGui::TextDisabled("Delta per frame");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::DragFloat("##PreviewMotionTestDelta", &_modifierPreviewMotionTestDelta, 0.001f, 0.f, 10.f, "%.3f");

                    ImGui::Checkbox("Oscillate", &_modifierPreviewMotionTestOscillate);
                    if (_modifierPreviewMotionTestOscillate)
                    {
                        ImGui::TextDisabled("Frequency");
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::DragFloat("##PreviewMotionTestHz", &_modifierPreviewMotionTestFrequency, 0.05f, 0.f, 20.f, "%.2f");
                    }

                    if (ImGui::Button("Stop Test"))
                    {
                        _modifierPreviewMotionTestEnabled = false;
                        _modifierPreviewMotionTestTag.clear();
                        chainModifier->Set_RuntimeTestOwnerMotion(false, Vec3::Zero);
                    }

                    ImGui::TreePop();
                }
            }

            if (auto* poseFollowModifier = dynamic_cast<PoseFollowChainModifier*>(modifier.get()))
            {
                bool debugThisModifier = _showModifierChainDebug
                    && _modifierChainDebugTag == modifier->Get_ModifierTag();
                if (ImGui::Checkbox("Debug Chains", &debugThisModifier))
                {
                    _showModifierChainDebug = debugThisModifier;
                    _modifierChainDebugTag = debugThisModifier ? modifier->Get_ModifierTag() : wstring{};
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Blue: pose, green/yellow: output, red: no output");

                if (ImGui::TreeNode("Preview Motion Test"))
                {
                    auto* poseDesc = static_cast<PoseFollowChainModifier::PoseFollowChainModifierDesc*>(reflectionTarget);

                    if (ImGui::Checkbox("Enable Test", &_modifierPreviewMotionTestEnabled))
                    {
                        if (!_modifierPreviewMotionTestEnabled)
                        {
                            _modifierPreviewMotionTestTag.clear();
                            poseFollowModifier->Set_RuntimeTestOwnerMotion(false, Vec3::Zero);
                        }
                        else
                        {
                            _modifierPreviewMotionTestTag = modifier->Get_ModifierTag();
                        }
                    }

                    if (poseDesc != nullptr && poseDesc->maxExternalOffset <= 0.0001f)
                    {
                        ImGui::TextColored(ImVec4(1.f, 0.68f, 0.25f, 1.f), "Move Limit is 0, so test motion is clamped.");
                    }

                    ImGui::TextDisabled("Test adds preview owner motion only; modifier values are unchanged.");
                    if (ImGui::Button("Run Motion Test"))
                    {
                        _modifierPreviewMotionTestEnabled = true;
                        _modifierPreviewMotionTestTag = modifier->Get_ModifierTag();
                        float visibleDelta = 1.f;
                        if (poseDesc != nullptr)
                        {
                            const float resetLimit = poseDesc->resetDistance > 0.1f ? poseDesc->resetDistance * 0.8f : 1.f;
                            const float ownerScale = std::max(poseDesc->ownerMotionScale, 0.001f);
                            visibleDelta = std::clamp(0.05f / ownerScale, 0.25f, resetLimit);
                        }

                        _modifierPreviewMotionTestDelta = visibleDelta;
                        _modifierPreviewMotionTestFrequency = 2.f;
                        _modifierPreviewMotionTestDirection = Vec3(1.f, 0.f, 0.f);
                    }

                    if (poseDesc != nullptr)
                    {
                        const float estimatedMotion = _modifierPreviewMotionTestDelta * poseDesc->ownerMotionScale;
                        ImGui::TextDisabled("Effective owner input: %.3f", estimatedMotion);
                    }
                    ImGui::TextDisabled("Applied modifiers: %d", _modifierPreviewMotionTestAppliedCount);

                    ImGui::TextDisabled("Direction");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::DragFloat3("##PreviewMotionTestDir", &_modifierPreviewMotionTestDirection.x, 0.01f, -1.f, 1.f);

                    ImGui::TextDisabled("Delta per frame");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::DragFloat("##PreviewMotionTestDelta", &_modifierPreviewMotionTestDelta, 0.001f, 0.f, 10.f, "%.3f");

                    ImGui::Checkbox("Oscillate", &_modifierPreviewMotionTestOscillate);
                    if (_modifierPreviewMotionTestOscillate)
                    {
                        ImGui::TextDisabled("Frequency");
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::DragFloat("##PreviewMotionTestHz", &_modifierPreviewMotionTestFrequency, 0.05f, 0.f, 20.f, "%.2f");
                    }

                    if (ImGui::Button("Stop Test"))
                    {
                        _modifierPreviewMotionTestEnabled = false;
                        _modifierPreviewMotionTestTag.clear();
                        poseFollowModifier->Set_RuntimeTestOwnerMotion(false, Vec3::Zero);
                    }

                    ImGui::TreePop();
                }

                const size_t groupCount = poseFollowModifier->Get_RuntimeGroupCount();
                if (groupCount > 0 && ImGui::TreeNode("Group Multipliers"))
                {
                    ImGui::TextDisabled("X: Wind, Y: Motion, Z: Follow");
                    for (size_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
                    {
                        ImGui::PushID(static_cast<int>(groupIndex));

                        const string rootBoneName = poseFollowModifier->Get_RuntimeGroupRootBoneName(groupIndex);
                        ImGui::TextWrapped("%s", rootBoneName.empty() ? "(empty root)" : rootBoneName.c_str());

                        Vec3 multipliers = poseFollowModifier->Get_RuntimeGroupMultipliers(groupIndex);
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragFloat3("Mul", &multipliers.x, 0.01f, 0.f, 10.f))
                            poseFollowModifier->Set_RuntimeGroupMultipliers(groupIndex, multipliers);

                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }

        }
        ImGui::PopID();
    }

    auto drawSimonHairPhysics = [&](Client::Mon_Simon_Hair* simonHair, const char* label)
    {
        if (simonHair == nullptr)
            return;

        if (const Shared<GameObject> previewRootObject = Get_PreviewRenderObject())
        {
            if (const Shared<ContainerObject> previewContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject))
            {
                if (auto* bodyPart = dynamic_cast<PartObject*>(previewContainer->Get_PartObject(L"Part_Body")))
                {
                    const Shared<ModelCom> bodyModel = bodyPart->Get_Component<ModelCom>();
                    simonHair->Set_RuntimeCollisionSource(
                        bodyModel.get(),
                        &bodyPart->Get_CombinedWorldMatrix());
                }
            }
        }

        const ClassReflectionInfo* reflectionInfo = simonHair->Get_RuntimeReflectionInfo();
        void* reflectionTarget = simonHair->Get_RuntimeReflectionTarget();
        if (reflectionInfo == nullptr || reflectionTarget == nullptr || reflectionInfo->properties.empty())
            return;

        hasEditableModifier = true;

        ImGui::PushID(simonHair);
        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Reset Pose"))
                simonHair->RuntimeResetPose();

            ImGui::SameLine();
            if (ImGui::Button("Rebuild Physics"))
                simonHair->RuntimeRebuild();

            ImGui::Spacing();
            for (const PropertyInfo& property : reflectionInfo->properties)
            {
                if (Inspector::Draw_ReflectionProperty(reflectionInfo->className, reflectionTarget, property))
                    simonHair->On_RuntimeReflectionChanged(property);
                ImGui::Spacing();
            }
        }
        ImGui::PopID();
    };

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    drawSimonHairPhysics(dynamic_cast<Client::Mon_Simon_Hair*>(previewRootObject.get()), "Simon Hair Physics");
    if (const Shared<ContainerObject> previewContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject))
    {
        drawSimonHairPhysics(
            dynamic_cast<Client::Mon_Simon_Hair*>(previewContainer->Get_PartObject(L"Part_Hair")),
            "Part_Hair  [Simon Hair Physics]");
    }

    if (!hasEditableModifier)
        ImGui::TextDisabled("No ChainModifier, PoseFollowChainModifier, PhysicsModifier, or Simon Hair physics.");
}

void Animation_View::Update_ModifierPreviewMotionTest(float timeDelta)
{
    if (!_modifierPreviewMotionTestEnabled)
    {
        _modifierPreviewMotionTestAppliedCount = 0;
        _modifierPreviewMotionTestPrevOffset = Vec3::Zero;
        _modifierPreviewMotionTestHasPrevOffset = false;
        return;
    }

    Shared<ModelCom> selectedModel = Get_ModifierRuntimeModel();

    vector<Shared<ModelCom>> modifierModels{};
    auto addModifierModel = [&](const Shared<ModelCom>& model)
    {
        if (model == nullptr)
            return;

        const auto exists = std::find_if(
            modifierModels.begin(),
            modifierModels.end(),
            [&](const Shared<ModelCom>& existing)
            {
                return existing.get() == model.get();
            });

        if (exists == modifierModels.end())
            modifierModels.push_back(model);
    };

    if (_sourceMode == SourceMode::LiveModel && !_liveModelTargets.empty())
    {
        for (const LiveModelTarget& target : _liveModelTargets)
        {
            Shared<ModelCom> targetModel = Resolve_LiveModelTargetModel(target);
            addModifierModel(targetModel);
        }
    }
    else
    {
        addModifierModel(Get_PlaybackModel());
    }

    addModifierModel(selectedModel);

    _modifierPreviewMotionTestAppliedCount = 0;

    Vec3 testDelta = Vec3::Zero;
    if (_modifierPreviewMotionTestEnabled)
    {
        Vec3 direction = _modifierPreviewMotionTestDirection;
        const float directionLength = direction.Length();
        if (directionLength > 0.0001f)
            direction /= directionLength;
        else
            direction = Vec3(1.f, 0.f, 0.f);

        if (_modifierPreviewMotionTestOscillate)
        {
            constexpr float kTwoPi = 6.28318530718f;
            const float frequency = std::max(0.f, _modifierPreviewMotionTestFrequency);
            const float wave = static_cast<float>(std::sin(ImGui::GetTime() * kTwoPi * frequency));
            const Vec3 currentOffset = direction * (_modifierPreviewMotionTestDelta * wave);
            if (_modifierPreviewMotionTestHasPrevOffset)
                testDelta = currentOffset - _modifierPreviewMotionTestPrevOffset;
            _modifierPreviewMotionTestPrevOffset = currentOffset;
            _modifierPreviewMotionTestHasPrevOffset = true;
        }
        else
        {
            UNREFERENCED_PARAMETER(timeDelta);
            _modifierPreviewMotionTestPrevOffset = Vec3::Zero;
            _modifierPreviewMotionTestHasPrevOffset = false;
            testDelta = direction * _modifierPreviewMotionTestDelta;
        }
    }

    for (const Shared<ModelCom>& model : modifierModels)
    {
        if (model == nullptr)
            continue;

        Sync_ModifierCollisionSource(model);

        for (const Shared<AnimationModifier>& modifier : model->Get_Modifiers())
        {
            if (auto* chainModifier = dynamic_cast<ChainModifier*>(modifier.get()))
            {
                const bool tagMatches =
                    _modifierPreviewMotionTestTag.empty() ||
                    chainModifier->Get_ModifierTag() == _modifierPreviewMotionTestTag;
                const bool applyToModifier = _modifierPreviewMotionTestEnabled && tagMatches;
                chainModifier->Set_RuntimeTestOwnerMotion(
                    applyToModifier,
                    applyToModifier ? testDelta : Vec3::Zero);
                if (applyToModifier)
                    ++_modifierPreviewMotionTestAppliedCount;
            }

            if (auto* poseFollowModifier = dynamic_cast<PoseFollowChainModifier*>(modifier.get()))
            {
                const bool tagMatches =
                    _modifierPreviewMotionTestTag.empty() ||
                    poseFollowModifier->Get_ModifierTag() == _modifierPreviewMotionTestTag;
                const bool applyToModifier = _modifierPreviewMotionTestEnabled && tagMatches;
                poseFollowModifier->Set_RuntimeTestOwnerMotion(
                    applyToModifier,
                    applyToModifier ? testDelta : Vec3::Zero);
                if (applyToModifier)
                    ++_modifierPreviewMotionTestAppliedCount;
            }

        }
    }
}

void Animation_View::Draw_SelectedBoneDetail(const Shared<ModelCom>& playbackModel)
{
    if (playbackModel == nullptr || _selectedBoneIndex < 0)
    {
        ImGui::TextDisabled("No bone selected.");
        return;
    }

    const uint32 boneIndex = static_cast<uint32>(_selectedBoneIndex);
    if (boneIndex >= playbackModel->Get_NumBones())
    {
        ImGui::TextDisabled("Selected bone is out of range.");
        return;
    }

    const int32 parentIndex = playbackModel->Get_BoneParentIndex(boneIndex);

    ImGui::Text("Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", playbackModel->Get_BoneName(boneIndex).c_str());

    ImGui::Spacing();
    ImGui::Text("Index");
    ImGui::SameLine();
    ImGui::TextDisabled("%d", _selectedBoneIndex);

    ImGui::Text("Parent");
    ImGui::SameLine();
    if (parentIndex >= 0 && parentIndex < static_cast<int32>(playbackModel->Get_NumBones()))
    {
        ImGui::TextDisabled(
            "%s [%d]",
            playbackModel->Get_BoneName(static_cast<uint32>(parentIndex)).c_str(),
            parentIndex);
    }
    else
    {
        ImGui::TextDisabled("(Root)");
    }

    ImGui::Spacing();
    Draw_TrailAnchorPanel();
}

void Animation_View::Draw_TrailAnchorPanel()
{
    ImGui::Separator();
    ImGui::Text("Trail Anchors");

    ImGui::Text("Base Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _trailAnchorAsset.base.boneName.empty() ? "(None)" : _trailAnchorAsset.base.boneName.c_str());

    ImGui::Text("Tip Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _trailAnchorAsset.tip.boneName.empty() ? "(None)" : _trailAnchorAsset.tip.boneName.c_str());

    Vec3 baseLocal = _trailAnchorAsset.base.localPosition;
    if (ImGui::DragFloat3("Base Local", &baseLocal.x, 0.5f))
    {
        _trailAnchorAsset.base.localPosition = baseLocal;
        MarkDirty();
    }

    Vec3 tipLocal = _trailAnchorAsset.tip.localPosition;
    if (ImGui::DragFloat3("Tip Local", &tipLocal.x, 0.5f))
    {
        _trailAnchorAsset.tip.localPosition = tipLocal;
        MarkDirty();
    }
}

Shared<ModelCom> Animation_View::Get_PlaybackModel() const
{
    if (_previewModelCom != nullptr)
        return _previewModelCom;

    return _liveModelCom;
}

Shared<ModelCom> Animation_View::Get_ModifierRuntimeModel() const
{
    if (_sourceMode != SourceMode::LiveModel ||
        _selectedLiveModelTargetIndex < 0 ||
        _selectedLiveModelTargetIndex >= static_cast<int32>(_liveModelTargets.size()))
    {
        return Get_PlaybackModel();
    }

    const LiveModelTarget& target = _liveModelTargets[_selectedLiveModelTargetIndex];

    if (_previewRootObject != nullptr)
    {
        Shared<ModelCom> previewModel = Resolve_LiveModelTargetModel(target);
        if (previewModel != nullptr)
        {
            return previewModel;
        }
    }

    return target.model != nullptr ? target.model : Get_PlaybackModel();
}

bool Animation_View::Try_GetTrailSample(EffectTrailSample& outSample) const
{
    for (const PreviewTrailRuntime& runtime : _previewTrailRuntimes)
    {
        if (runtime.stateId != _previewTrailProviderStateId)
            continue;

        return Try_GetTrailSampleForRuntime(runtime, outSample); // ?熬곣뫗??update 繞벿살탳??runtime??partTag/anchor??sample????ｌ뫒亦??類ｋ펲.
    }

    return false;
}

bool Animation_View::Try_GetSourcePointSample(EffectSourcePointSample& outSample) const
{
    for (const PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
    {
        if (runtime.stateId != _previewSourceHistoryProviderStateId)
            continue;

        return Try_GetSourcePointSampleForRuntime(runtime, outSample);
    }

    return false;
}

Weapon* Animation_View::Resolve_PreviewWeapon(const wstring& partTag) const
{
    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return nullptr;

    return dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(partTag));
}

bool Animation_View::Try_GetTrailSampleForRuntime(
    const PreviewTrailRuntime& runtime,
    EffectTrailSample& outSample) const
{
    const Client::TrailAnchorAsset sampleAnchorAsset =
        Client::ANS_Trail::Make_OffsetTrailAnchorAsset(
            runtime.trailAnchorAsset,
            runtime.tipExtendCurve,
            runtime.baseLocalOffsetCurve,
            runtime.tipLocalOffsetCurve,
            runtime.offsetPhase);

    Vec3 baseWorld{};
    Vec3 tipWorld{};
    if (!Try_ResolvePreviewTrailAnchorWorldPosition(runtime.partTag, sampleAnchorAsset.base, baseWorld))
        return false;
    if (!Try_ResolvePreviewTrailAnchorWorldPosition(runtime.partTag, sampleAnchorAsset.tip, tipWorld))
        return false;

    outSample.baseWorldPosition = baseWorld;
    outSample.tipWorldPosition = tipWorld;
    return true;
}

bool Animation_View::Try_GetSourcePointSampleForRuntime(
    const PreviewSourceHistoryRuntime& runtime,
    EffectSourcePointSample& outSample) const
{
    if (runtime.sourceBindingMode == 1)
        return Try_ResolvePreviewSocketWorldPosition(
            runtime.partTag,
            runtime.socketName,
            runtime.sourceHistoryLocalOffset,
            outSample.worldPosition);

    if (runtime.sourceBindingMode == 3)
    {
        Vec3 baseWorld{};
        Vec3 tipWorld{};
        if (!Try_ResolvePreviewTrailAnchorWorldPosition(runtime.partTag, runtime.trailAnchorAsset.base, baseWorld))
            return false;
        if (!Try_ResolvePreviewTrailAnchorWorldPosition(runtime.partTag, runtime.trailAnchorAsset.tip, tipWorld))
            return false;

        if (runtime.sourceHistoryAnchorName == "Base")
        {
            outSample.worldPosition = baseWorld + runtime.sourceHistoryLocalOffset;
            return true;
        }

        if (runtime.sourceHistoryAnchorName == "Mid")
        {
            outSample.worldPosition = ((baseWorld + tipWorld) * 0.5f) + runtime.sourceHistoryLocalOffset;
            return true;
        }

        outSample.worldPosition = tipWorld + runtime.sourceHistoryLocalOffset;
        return true;
    }

    return Try_ResolvePreviewWeaponAnchorWorldPosition(
        runtime.partTag,
        runtime.sourceHistoryAnchorName,
        runtime.sourceHistoryLocalOffset,
        outSample.worldPosition);
}

void Animation_View::Update_PreviewWeaponRotateStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _liveModelCom == nullptr)
    {
        Reset_PreviewWeaponRotateStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewWeaponRotateStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    if (playbackModel == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewWeaponRotateStates();
        return;
    }

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
    context.previousTimeSec = max(0.f, context.currentTimeSec - max(0.f, timeDelta));
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    vector<wstring> touchedPartTags{};

    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_WeaponRotate")
            continue;
        if (context.currentTimeSec < entry.startSec)
            continue;
        if (context.currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStateIds.push_back(entry.id);

        const json payload = entry.notifyState->To_Json();
        const string partTagString = payload.value("_partTag", string{ "Part_WeaponL" });
        const wstring partTag = String::ToWString(partTagString);

        AnimNotifyContext stateContext = context;
        stateContext.notifyStateDurationSec = entry.durationSec;
        stateContext.notifyStateProgress =
            entry.durationSec > 0.f
            ? clamp((context.currentTimeSec - entry.startSec) / entry.durationSec, 0.f, 1.f)
            : 1.f;

        PreviewWeaponRotateRuntime* runtime = nullptr;
        for (PreviewWeaponRotateRuntime& entryRuntime : _previewWeaponRotateRuntimes)
        {
            if (entryRuntime.stateId == entry.id)
            {
                runtime = &entryRuntime;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewWeaponRotateRuntime newRuntime{};
            newRuntime.stateId = entry.id;
            newRuntime.notifyState = entry.notifyState;
            newRuntime.partTag = partTag;
            _previewWeaponRotateRuntimes.push_back(std::move(newRuntime));
            runtime = &_previewWeaponRotateRuntimes.back();

            runtime->notifyState->On_Begin(stateContext); // preview?????깆젷 notify ???덈뺄 ?????熬곣뫗??_asset ?リ옇????怨쀬Ŧ 嶺뚯쉳???state????戮곗굚??類ｋ펲.
        }
        else
        {
            runtime->partTag = partTag;
            runtime->notifyState->On_Tick(stateContext);
        }

        touchedPartTags.push_back(partTag);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewWeaponRotateRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewWeaponRotateRuntimes[runtimeIndex].stateId;
        const bool stillActive =
            std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();

        if (stillActive)
            continue;

        AnimNotifyContext endContext = context;
        endContext.notifyStateProgress = 1.f;
        endContext.notifyStateDurationSec = 0.f;

        if (_previewWeaponRotateRuntimes[runtimeIndex].notifyState)
            _previewWeaponRotateRuntimes[runtimeIndex].notifyState->On_End(endContext);

        touchedPartTags.push_back(_previewWeaponRotateRuntimes[runtimeIndex].partTag);
        _previewWeaponRotateRuntimes.erase(_previewWeaponRotateRuntimes.begin() + runtimeIndex);
    }

    for (const wstring& partTag : touchedPartTags)
    {
        if (Weapon* weapon = Resolve_PreviewWeapon(partTag))
            weapon->Update(0.f); // notify?띠럾? ?꾩룆????β돦裕뉛쭚????????熬곣뫗???熬곣뫁???combinedWorldMatrix???꾩룆?餓??꾩룇瑗???類ｋ펲.
    }
}

void Animation_View::Reset_PreviewWeaponRotateStates()
{
    if (_previewWeaponRotateRuntimes.empty())
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

    AnimNotifyContext endContext{};
    endContext.owner = previewRootObject.get();
    endContext.model = playbackModel.get();
    endContext.modelGuid = _asset.modelGuid;
    endContext.animationName = _currentAnimationName;
    endContext.notifyStateProgress = 1.f;
    endContext.notifyStateDurationSec = 0.f;
    endContext.isPreview = true;

    vector<wstring> touchedPartTags{};
    for (PreviewWeaponRotateRuntime& runtime : _previewWeaponRotateRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(endContext);

        touchedPartTags.push_back(runtime.partTag);
    }

    _previewWeaponRotateRuntimes.clear();

    for (const wstring& partTag : touchedPartTags)
    {
        if (Weapon* weapon = Resolve_PreviewWeapon(partTag))
            weapon->Update(0.f);
    }
}

void Animation_View::Update_PreviewWeaponEchoStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _liveModelCom == nullptr)
    {
        Reset_PreviewWeaponEchoStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewWeaponEchoStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    if (playbackModel == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewWeaponEchoStates();
        return;
    }

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.previousTimeSec = max(0.f, currentTimeSec - max(0.f, timeDelta));
    context.currentTimeSec = currentTimeSec;
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};

    auto syncPreviewEchoTrail =
        [this, animationData, currentTimeSec, previewRootObject](
            const wstring& echoPartTag,
            float echoDelaySec)
        {
            Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
            if (ownerContainer == nullptr)
                return;

            Mon_Simon_Weapon* simonWeapon =
                dynamic_cast<Mon_Simon_Weapon*>(ownerContainer->Get_PartObject(echoPartTag));
            if (simonWeapon == nullptr)
                return;

            const string echoPartTagString = String::ToString(echoPartTag);
            const float sourceTimeSec = currentTimeSec - max(0.f, echoDelaySec);
            for (const AnimNotifyStateEntry& trailEntry : animationData->notifyStates)
            {
                if (trailEntry.notifyState == nullptr)
                    continue;
                if (trailEntry.notifyState->Get_TypeName() != "ANS_Trail")
                    continue;

                const json trailPayload = trailEntry.notifyState->To_Json();
                const string trailPartTagString = trailPayload.value("_partTag", string{ "Part_Weapon" });
                if (trailPartTagString != echoPartTagString)
                    continue;

                const uint64 notifyToken =
                    kPreviewWeaponEchoTrailTokenMask |
                    static_cast<uint64>(static_cast<uint32>(trailEntry.id));
                const float trailEndSec = trailEntry.startSec + max(0.f, trailEntry.durationSec);
                if (sourceTimeSec > trailEndSec)
                {
                    simonWeapon->End_WeaponEchoTrailEvent(notifyToken, trailEndSec);
                    continue;
                }

                if (sourceTimeSec < trailEntry.startSec)
                    continue;

                wstring resolvedEffectName{};
                const string effectNameValue = trailPayload.value("_effectName", string{});
                if (!Try_ResolvePreviewTrailEffectName(effectNameValue, trailPartTagString, resolvedEffectName))
                    continue;

                const Client::ANS_Trail* trailNotifyState =
                    dynamic_cast<const Client::ANS_Trail*>(trailEntry.notifyState.get());
                if (trailNotifyState == nullptr)
                    continue;

                EffectCom::TrailOffsetRuntimeDesc offsetDesc{};
                const float trailDuration = max(trailEntry.durationSec, 0.0001f);
                offsetDesc.phase = std::clamp((sourceTimeSec - trailEntry.startSec) / trailDuration, 0.f, 1.f);
                offsetDesc.tipExtendCurve = trailNotifyState->Get_TipExtendCurve();
                offsetDesc.baseLocalOffsetCurve = trailNotifyState->Get_BaseLocalOffsetCurve();
                offsetDesc.tipLocalOffsetCurve = trailNotifyState->Get_TipLocalOffsetCurve();

                Mon_Simon_Weapon::WeaponEchoTrailEventDesc trailEventDesc{};
                trailEventDesc.notifyToken = notifyToken;
                trailEventDesc.animationName = _currentAnimationName;
                trailEventDesc.startSec = trailEntry.startSec;
                trailEventDesc.durationSec = max(0.f, trailEntry.durationSec);
                trailEventDesc.sourceEffectName = resolvedEffectName;
                trailEventDesc.layerTag = L"Layer_Effect";
                trailEventDesc.offsetDesc = offsetDesc;
                simonWeapon->Register_WeaponEchoTrailEvent(trailEventDesc);
                simonWeapon->Update_WeaponEchoTrailEvent(notifyToken, sourceTimeSec, offsetDesc);
            }
        };

    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_WeaponEcho")
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStateIds.push_back(entry.id);

        const json payload = entry.notifyState->To_Json();
        const string partTagString = payload.value("_partTag", string{ "Part_Weapon" });
        const wstring partTag = String::ToWString(partTagString);
        const float delaySec = payload.value("_delaySec", 0.2f);
        const string payloadJson = payload.dump();

        AnimNotifyContext stateContext = context;
        stateContext.notifyStateDurationSec = entry.durationSec;
        stateContext.notifyStateProgress =
            entry.durationSec > 0.f
            ? clamp((currentTimeSec - entry.startSec) / entry.durationSec, 0.f, 1.f)
            : 1.f;

        PreviewWeaponEchoRuntime* runtime = nullptr;
        for (PreviewWeaponEchoRuntime& entryRuntime : _previewWeaponEchoRuntimes)
        {
            if (entryRuntime.stateId == entry.id)
            {
                runtime = &entryRuntime;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewWeaponEchoRuntime newRuntime{};
            newRuntime.stateId = entry.id;
            newRuntime.notifyState = entry.notifyState;
            newRuntime.partTag = partTag;
            newRuntime.payloadJson = payloadJson;
            _previewWeaponEchoRuntimes.push_back(std::move(newRuntime));
            runtime = &_previewWeaponEchoRuntimes.back();

            runtime->notifyState->On_Begin(stateContext);
            syncPreviewEchoTrail(partTag, delaySec);
        }
        else if (runtime->notifyState != entry.notifyState || runtime->payloadJson != payloadJson)
        {
            if (runtime->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtime->notifyState->On_End(endContext);
            }

            runtime->notifyState = entry.notifyState;
            runtime->partTag = partTag;
            runtime->payloadJson = payloadJson;
            runtime->notifyState->On_Begin(stateContext);
            syncPreviewEchoTrail(partTag, delaySec);
        }
        else
        {
            runtime->partTag = partTag;
            runtime->notifyState->On_Tick(stateContext);
            syncPreviewEchoTrail(partTag, delaySec);
        }
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewWeaponEchoRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewWeaponEchoRuntimes[runtimeIndex].stateId;
        const bool stillActive =
            std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();

        if (stillActive)
            continue;

        AnimNotifyContext endContext = context;
        endContext.notifyStateProgress = 1.f;
        endContext.notifyStateDurationSec = 0.f;

        if (_previewWeaponEchoRuntimes[runtimeIndex].notifyState)
            _previewWeaponEchoRuntimes[runtimeIndex].notifyState->On_End(endContext);

        _previewWeaponEchoRuntimes.erase(_previewWeaponEchoRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewWeaponEchoStates()
{
    if (_previewWeaponEchoRuntimes.empty())
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

    AnimNotifyContext endContext{};
    endContext.owner = previewRootObject.get();
    endContext.model = playbackModel.get();
    endContext.modelGuid = _asset.modelGuid;
    endContext.animationName = _currentAnimationName;
    endContext.notifyStateProgress = 1.f;
    endContext.notifyStateDurationSec = 0.f;
    endContext.isPreview = true;

    for (PreviewWeaponEchoRuntime& runtime : _previewWeaponEchoRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(endContext);
    }

    _previewWeaponEchoRuntimes.clear();
}

void Animation_View::Update_PreviewSLDodgeMoveStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewSLDodgeMoveStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewSLDodgeMoveStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    if (playbackModel == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewSLDodgeMoveStates();
        return;
    }

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = currentTimeSec;
    context.previousTimeSec = max(0.f, currentTimeSec - max(0.f, timeDelta));
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_SL_DodgeMove")
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

        auto runtimeIt = std::find_if(
            _previewSLDodgeMoveRuntimes.begin(),
            _previewSLDodgeMoveRuntimes.end(),
            [&](const PreviewSLDodgeMoveRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewSLDodgeMoveRuntimes.end())
        {
            PreviewSLDodgeMoveRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewSLDodgeMoveRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext);
            entry.notifyState->On_Tick(stateContext);
            continue;
        }

        if (runtimeIt->notifyState != entry.notifyState ||
            runtimeIt->payloadJson != payloadJson)
        {
            if (runtimeIt->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtimeIt->notifyState->On_End(endContext);
            }

            runtimeIt->notifyState = entry.notifyState;
            runtimeIt->payloadJson = payloadJson;
            entry.notifyState->On_Begin(stateContext);
        }

        entry.notifyState->On_Tick(stateContext);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewSLDodgeMoveRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewSLDodgeMoveRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewSLDodgeMoveRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            endContext.notifyStateDurationSec = 0.f;
            _previewSLDodgeMoveRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewSLDodgeMoveRuntimes.erase(_previewSLDodgeMoveRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewSLDodgeMoveStates()
{
    if (_previewSLDodgeMoveRuntimes.empty())
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.notifyStateDurationSec = 0.f;
    context.isPreview = true;

    for (PreviewSLDodgeMoveRuntime& runtime : _previewSLDodgeMoveRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(context);
    }

    _previewSLDodgeMoveRuntimes.clear();
}

void Animation_View::Update_PreviewRootControllerStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewRootControllerStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewRootControllerStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    if (playbackModel == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewRootControllerStates();
        return;
    }

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = currentTimeSec;
    context.previousTimeSec = max(0.f, currentTimeSec - max(0.f, timeDelta));
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_RootController")
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

        auto runtimeIt = std::find_if(
            _previewRootControllerRuntimes.begin(),
            _previewRootControllerRuntimes.end(),
            [&](const PreviewRootControllerRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewRootControllerRuntimes.end())
        {
            PreviewRootControllerRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewRootControllerRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext);
            entry.notifyState->On_Tick(stateContext);
            continue;
        }

        if (runtimeIt->notifyState != entry.notifyState ||
            runtimeIt->payloadJson != payloadJson)
        {
            if (runtimeIt->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtimeIt->notifyState->On_End(endContext);
            }

            runtimeIt->notifyState = entry.notifyState;
            runtimeIt->payloadJson = payloadJson;
            entry.notifyState->On_Begin(stateContext);
        }

        entry.notifyState->On_Tick(stateContext);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewRootControllerRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewRootControllerRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewRootControllerRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            endContext.notifyStateDurationSec = 0.f;
            _previewRootControllerRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewRootControllerRuntimes.erase(_previewRootControllerRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewRootControllerStates()
{
    if (_previewRootControllerRuntimes.empty())
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.notifyStateDurationSec = 0.f;
    context.isPreview = true;

    for (PreviewRootControllerRuntime& runtime : _previewRootControllerRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(context);
    }

    _previewRootControllerRuntimes.clear();
}

void Animation_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
    width = max(1u, width);
    height = max(1u, height);

    if (_previewRenderTarget == nullptr)
    {
        _previewRenderTarget = RenderTarget::Create(
            L"RenderTarget",
            GAME->Get_Device(),
            GAME->Get_Context(),
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4(0.15f, 0.15f, 0.18f, 1.f),
            true);
        return;
    }

    _previewRenderTarget->Resize(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4(0.15f, 0.15f, 0.18f, 1.f),
        true);
}

void Animation_View::Ensure_PreviewCamera()
{
    if (_previewCamera != nullptr)
        return;

    _previewCamera = Editor_Camera_Free::Create(GAME->Get_Device(), GAME->Get_Context());
    if (_previewCamera == nullptr)
        return;

    Editor_Camera_Free::EDITOR_CAMERA_DESC desc = Build_PreviewCameraDesc(Get_PreviewRenderObject());

    if (FAILED(_previewCamera->Initialize(&desc)))
    {
        _previewCamera = nullptr;
        return;
    }

    _previewCamera->Apply_EditorDesc(desc);
    Apply_PreviewCameraTransform(_previewCamera);
}

void Animation_View::Ensure_PreviewSkyBackdrop()
{
    if (_previewSkyBox != nullptr)
        return;

    Shared<Client::SkyBoxFlow> skyBoxPrototype = Client::SkyBoxFlow::Create(GAME->Get_Device(), GAME->Get_Context());
    if (skyBoxPrototype == nullptr)
        return;

    Client::SkyBoxFlow::SKYBOXFLOW_DESC skyBoxDesc{};
    _previewSkyBox = dynamic_pointer_cast<Client::SkyBoxFlow>(skyBoxPrototype->Clone(&skyBoxDesc));
    if (_previewSkyBox != nullptr)
        _previewSkyBox->Set_Visible(true);

    _previewSkyCloudCards.clear();

    Shared<Client::SkyCloudCard> skyCloudPrototype = Client::SkyCloudCard::Create(GAME->Get_Device(), GAME->Get_Context());
    if (skyCloudPrototype == nullptr)
        return;

    Client::SkyCloudCard::SKYCLOUDCARD_DESC cloudDesc{};
    cloudDesc.modelTag = TEXT("Model_Cloud_Mesh");
    cloudDesc.shaderTag = TEXT("Shader_SkyCloudCard");
    cloudDesc.cloudTextureTag = TEXT("Texture_T_Cloud_13_H");
    cloudDesc.position = kPreviewSkyCloudLocalOffset;
    cloudDesc.rotation = Vec3(-4.763f, -119.267f, -349.155f);
    cloudDesc.scale = Vec3(1.5f, 0.9f, 0.9f);

    Shared<Client::SkyCloudCard> skyCloudCard =
        dynamic_pointer_cast<Client::SkyCloudCard>(skyCloudPrototype->Clone(&cloudDesc));
    if (skyCloudCard != nullptr)
    {
        skyCloudCard->Set_Visible(true);
        _previewSkyCloudCards.push_back(skyCloudCard);
    }
}

void Animation_View::Update_PreviewSkyBackdrop(float timeDelta)
{
    if (_previewSkyBox == nullptr)
        return;

    if (_previewCamera != nullptr)
    {
        Shared<TransformCom> cameraTransform = _previewCamera->Get_Transform();
        if (cameraTransform != nullptr)
        {
            const Vec3 cameraPos = cameraTransform->Get_WorldPosition();
            for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
            {
                if (skyCloudCard == nullptr)
                    continue;

                Shared<TransformCom> cloudTransform = skyCloudCard->Get_Transform();
                if (cloudTransform != nullptr)
                    cloudTransform->Set_WorldPosition(cameraPos + kPreviewSkyCloudLocalOffset);
            }
        }
    }

    if (!_previewSkyBackdropBegunPlay)
    {
        _previewSkyBox->Try_BeginPlay();
        for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
        {
            if (skyCloudCard != nullptr)
                skyCloudCard->Try_BeginPlay();
        }
        _previewSkyBackdropBegunPlay = true;
    }

    _previewSkyBox->Set_Visible(true);
    _previewSkyBox->Priority_Update(timeDelta);
    _previewSkyBox->Update(timeDelta);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard == nullptr)
            continue;

        skyCloudCard->Set_Visible(true);
        skyCloudCard->Priority_Update(timeDelta);
        skyCloudCard->Update(timeDelta);
    }
}

void Animation_View::Late_Update_PreviewSkyBackdrop(float timeDelta)
{
    if (_previewSkyBox != nullptr)
        _previewSkyBox->Late_Update(timeDelta);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard != nullptr)
            skyCloudCard->Late_Update(timeDelta);
    }
}

Shared<GameObject> Animation_View::Get_PreviewRenderObject() const
{
    if (_previewRootObject != nullptr)
        return _previewRootObject;

    if (_liveModelCom != nullptr)
        return Resolve_RootPreviewObject(_liveModelCom->Get_Owner());

    return nullptr;
}

void Animation_View::Fit_PreviewCameraToModel()
{
    if (_previewCamera == nullptr)
        return;

    _previewCamera->Apply_EditorDesc(Build_PreviewCameraDesc(Get_PreviewRenderObject()));
    Apply_PreviewCameraTransform(_previewCamera);
}

void Animation_View::Ensure_PreviewGridResources()
{
    if (_previewGridBatch && _previewGridEffect && _previewGridInputLayout && _previewGridDepthState && _previewOverlayDepthState)
        return;

    static constexpr D3D11_INPUT_ELEMENT_DESC kGridInputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    _previewGridBatch = make_shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(GAME->Get_Context().Get());
    _previewGridEffect = make_shared<DirectX::BasicEffect>(GAME->Get_Device().Get());

    if (_previewGridBatch == nullptr || _previewGridEffect == nullptr)
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        return;
    }

    _previewGridEffect->SetVertexColorEnabled(true);

    const void* shaderByteCode = nullptr;
    size_t shaderByteCodeLength = 0;
    _previewGridEffect->GetVertexShaderBytecode(&shaderByteCode, &shaderByteCodeLength);

    if (FAILED(GAME->Get_Device()->CreateInputLayout(
        kGridInputElements,
        static_cast<UINT>(_countof(kGridInputElements)),
        shaderByteCode,
        shaderByteCodeLength,
        _previewGridInputLayout.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    if (FAILED(GAME->Get_Device()->CreateDepthStencilState(&depthDesc, _previewGridDepthState.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        _previewGridDepthState.Reset();
        _previewOverlayDepthState.Reset();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC overlayDepthDesc{};
    overlayDepthDesc.DepthEnable = FALSE;
    overlayDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    overlayDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    overlayDepthDesc.StencilEnable = FALSE;

    if (FAILED(GAME->Get_Device()->CreateDepthStencilState(&overlayDepthDesc, _previewOverlayDepthState.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        _previewGridDepthState.Reset();
        _previewOverlayDepthState.Reset();
    }
}

void Animation_View::Draw_PreviewGrid() const
{
    if (!_showPreviewGrid)
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewGridDepthState)
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewGridDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    _previewGridBatch->Begin();

    const Vec3 gridCenter = Vec3::Zero;

    constexpr int gridDivs = 40;
    constexpr float gridHalfExtent = 20.f;
    const float step = (gridHalfExtent * 2.f) / static_cast<float>(gridDivs);
    const float y = 0.f;

    const XMVECTOR gridColor = XMVectorSet(0.58f, 0.58f, 0.62f, 1.f);
    const XMVECTOR xAxisColor = XMVectorSet(0.95f, 0.32f, 0.32f, 1.f);
    const XMVECTOR zAxisColor = XMVectorSet(0.28f, 0.72f, 1.f, 1.f);

    for (int i = 0; i <= gridDivs; ++i)
    {
        const float offset = -gridHalfExtent + step * static_cast<float>(i);

        const XMVECTOR colorX = (fabsf(offset) < 0.0001f) ? zAxisColor : gridColor;
        const XMVECTOR colorZ = (fabsf(offset) < 0.0001f) ? xAxisColor : gridColor;

        DirectX::VertexPositionColor lineXStart(
            Vec3(gridCenter.x - gridHalfExtent, y, gridCenter.z + offset),
            colorX);
        DirectX::VertexPositionColor lineXEnd(
            Vec3(gridCenter.x + gridHalfExtent, y, gridCenter.z + offset),
            colorX);
        _previewGridBatch->DrawLine(lineXStart, lineXEnd);

        DirectX::VertexPositionColor lineZStart(
            Vec3(gridCenter.x + offset, y, gridCenter.z - gridHalfExtent),
            colorZ);
        DirectX::VertexPositionColor lineZEnd(
            Vec3(gridCenter.x + offset, y, gridCenter.z + gridHalfExtent),
            colorZ);
        _previewGridBatch->DrawLine(lineZStart, lineZEnd);
    }

    _previewGridBatch->End();
}

void Animation_View::Draw_SelectedBoneMarker() const
{
    if (_selectedBoneIndex < 0)
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    const uint32 boneIndex = static_cast<uint32>(_selectedBoneIndex);
    if (boneIndex >= playbackModel->Get_NumBones())
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
        return;

    const Matrix* boneMatrix = playbackModel->Get_BoneMatrixPtr(boneIndex);
    if (boneMatrix == nullptr)
        return;

    Shared<GameObject> ownerObject = playbackModel->Get_Owner();
    Shared<TransformCom> ownerTransform = ownerObject ? ownerObject->Get_Transform() : nullptr;
    const Matrix ownerWorldMatrix = ownerTransform ? ownerTransform->Get_WorldMatrix() : Matrix::Identity;
    const Matrix worldBoneMatrix = *boneMatrix * ownerWorldMatrix;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewOverlayDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    const Vec3 bonePosition(worldBoneMatrix._41, worldBoneMatrix._42, worldBoneMatrix._43);

    Vec3 rightAxis(worldBoneMatrix._11, worldBoneMatrix._12, worldBoneMatrix._13);
    Vec3 upAxis(worldBoneMatrix._21, worldBoneMatrix._22, worldBoneMatrix._23);
    Vec3 forwardAxis(worldBoneMatrix._31, worldBoneMatrix._32, worldBoneMatrix._33);

    if (rightAxis.LengthSquared() <= 0.000001f)
        rightAxis = Vec3::Right;
    else
        rightAxis.Normalize();

    if (upAxis.LengthSquared() <= 0.000001f)
        upAxis = Vec3::Up;
    else
        upAxis.Normalize();

    if (forwardAxis.LengthSquared() <= 0.000001f)
        forwardAxis = Vec3::Forward;
    else
        forwardAxis.Normalize();

    constexpr float markerRadius = 0.09f;
    constexpr float axisLength = 0.28f;
    constexpr int markerSegments = 24;

    _previewGridBatch->Begin();

    const auto drawAxisLine =
        [&](const Vec3& direction, const XMVECTORF32& color) -> void
    {
        DirectX::VertexPositionColor lineStart(bonePosition, color);
        DirectX::VertexPositionColor lineEnd(bonePosition + direction, color);
        _previewGridBatch->DrawLine(lineStart, lineEnd);
    };

    const auto drawRing =
        [&](const Vec3& axisA, const Vec3& axisB, const XMVECTORF32& color) -> void
    {
        for (int segmentIndex = 0; segmentIndex < markerSegments; ++segmentIndex)
        {
            const float t0 = XM_2PI * static_cast<float>(segmentIndex) / static_cast<float>(markerSegments);
            const float t1 = XM_2PI * static_cast<float>(segmentIndex + 1) / static_cast<float>(markerSegments);

            const Vec3 point0 = bonePosition +
                axisA * (cosf(t0) * markerRadius) +
                axisB * (sinf(t0) * markerRadius);
            const Vec3 point1 = bonePosition +
                axisA * (cosf(t1) * markerRadius) +
                axisB * (sinf(t1) * markerRadius);

            DirectX::VertexPositionColor lineStart(point0, color);
            DirectX::VertexPositionColor lineEnd(point1, color);
            _previewGridBatch->DrawLine(lineStart, lineEnd);
        }
    };

    drawRing(rightAxis, upAxis, DirectX::Colors::Yellow);
    drawRing(rightAxis, forwardAxis, DirectX::Colors::Yellow);
    drawRing(upAxis, forwardAxis, DirectX::Colors::Yellow);
    drawAxisLine(rightAxis * axisLength, DirectX::Colors::Red);
    drawAxisLine(upAxis * axisLength, DirectX::Colors::Lime);
    drawAxisLine(forwardAxis * axisLength, DirectX::Colors::DeepSkyBlue);

    _previewGridBatch->End();
}

void Animation_View::Draw_ModifierChainDebug() const
{
    if (!_showModifierChainDebug)
        return;

    Shared<ModelCom> modifierModel = Get_ModifierRuntimeModel();
    if (modifierModel == nullptr)
        return;

    PoseFollowChainModifier* poseFollowModifier = nullptr;
    for (const Shared<AnimationModifier>& modifier : modifierModel->Get_Modifiers())
    {
        if (modifier == nullptr || modifier->Get_ModifierTag() != _modifierChainDebugTag)
            continue;

        poseFollowModifier = dynamic_cast<PoseFollowChainModifier*>(modifier.get());
        if (poseFollowModifier != nullptr)
            break;
    }

    if (poseFollowModifier == nullptr)
        return;

    vector<PoseFollowChainModifier::RuntimeDebugNode> nodes{};
    poseFollowModifier->Build_RuntimeDebugNodes(modifierModel.get(), nodes);
    if (nodes.empty())
        return;

    Matrix debugCorrection = Matrix::Identity;
    if (const Shared<GameObject> modelOwner = modifierModel->Get_Owner())
    {
        if (const Shared<PartObject> partOwner = dynamic_pointer_cast<PartObject>(modelOwner))
        {
            Matrix sourceWorld = Matrix::Identity;
            if (const Shared<GameObject> parentOwner = partOwner->Get_Owner())
            {
                if (const Shared<TransformCom> parentTransform = parentOwner->Get_Transform())
                    sourceWorld = parentTransform->Get_WorldMatrix();
            }

            debugCorrection = sourceWorld.Invert() * partOwner->Get_CombinedWorldMatrix();
        }
    }

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewOverlayDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    unordered_map<size_t, size_t> nodeLookup{};
    nodeLookup.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const uint64 key = (static_cast<uint64>(nodes[i].groupIndex) << 32) | static_cast<uint32>(nodes[i].nodeIndex);
        nodeLookup.emplace(static_cast<size_t>(key), i);
    }

    const auto findNode =
        [&](size_t groupIndex, int32 nodeIndex) -> const PoseFollowChainModifier::RuntimeDebugNode*
    {
        if (nodeIndex < 0)
            return nullptr;

        const uint64 key = (static_cast<uint64>(groupIndex) << 32) | static_cast<uint32>(nodeIndex);
        const auto it = nodeLookup.find(static_cast<size_t>(key));
        if (it == nodeLookup.end())
            return nullptr;

        return &nodes[it->second];
    };

    const auto drawLine =
        [&](const Vec3& start, const Vec3& end, const XMVECTORF32& color)
    {
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(Vec3::Transform(start, debugCorrection), color),
            DirectX::VertexPositionColor(Vec3::Transform(end, debugCorrection), color));
    };

    const auto drawCross =
        [&](const Vec3& center, float radius, const XMVECTORF32& color)
    {
        drawLine(center + Vec3(-radius, 0.f, 0.f), center + Vec3(radius, 0.f, 0.f), color);
        drawLine(center + Vec3(0.f, -radius, 0.f), center + Vec3(0.f, radius, 0.f), color);
        drawLine(center + Vec3(0.f, 0.f, -radius), center + Vec3(0.f, 0.f, radius), color);
    };

    _previewGridBatch->Begin();

    for (const auto& node : nodes)
    {
        const auto* parent = findNode(node.groupIndex, node.parentNodeIndex);
        if (parent != nullptr)
        {
            drawLine(parent->posePosition, node.posePosition, DirectX::Colors::DeepSkyBlue);

            const float writeAlpha = std::clamp(node.writeAlpha, 0.f, 1.f);
            XMVECTORF32 simulatedColor = DirectX::Colors::Red;
            if (node.isVirtualAnchor)
                simulatedColor = DirectX::Colors::Gray;
            else if (writeAlpha > 0.02f)
                simulatedColor = XMVECTORF32{ { 1.f - writeAlpha, 1.f, 0.10f, 1.f } };

            drawLine(parent->simulatedPosition, node.simulatedPosition, simulatedColor);
        }

        const float pointRadius = node.isRootNode ? 0.055f : 0.035f;
        XMVECTORF32 pointColor = DirectX::Colors::Lime;
        if (node.isVirtualAnchor)
            pointColor = DirectX::Colors::Gray;
        else if (node.writeAlpha <= 0.02f)
            pointColor = DirectX::Colors::Red;
        else if (node.isRootNode)
            pointColor = DirectX::Colors::Orange;

        drawCross(node.simulatedPosition, pointRadius, pointColor);
    }

    _previewGridBatch->End();
}

void Animation_View::Draw_ModifierCollisionDebug() const
{
    Shared<ModelCom> modifierModel = Get_ModifierRuntimeModel();
    if (modifierModel == nullptr)
        return;

    Sync_ModifierCollisionSource(modifierModel);

    struct RuntimeCollisionCapsule
    {
        Vec3 startPoint{ Vec3::Zero };
        Vec3 endPoint{ Vec3::Zero };
        float sourceRadius{ 0.f };
        float collisionRadius{ 0.f };
    };

    struct RuntimeCollisionSegment
    {
        Vec3 startPoint{ Vec3::Zero };
        Vec3 endPoint{ Vec3::Zero };
        bool hasContact{ false };
        bool hasPush{ false };
        bool isSolvedSegment{ false };
        int32 type{ -1 };
    };

    struct RuntimeCollisionSphere
    {
        Vec3 center{ Vec3::Zero };
        float radius{ 0.f };
        bool hasContact{ false };
    };

    vector<RuntimeCollisionCapsule> capsules{};
    vector<RuntimeCollisionSegment> segments{};
    vector<RuntimeCollisionSphere> spheres{};
    for (const Shared<AnimationModifier>& modifier : modifierModel->Get_Modifiers())
    {
        if (auto* poseFollowModifier = dynamic_cast<PoseFollowChainModifier*>(modifier.get()))
        {
            auto* desc = static_cast<PoseFollowChainModifier::PoseFollowChainModifierDesc*>(
                poseFollowModifier->Get_RuntimeReflectionTarget());
            if (desc == nullptr || !desc->debugDrawCapsules)
                continue;

            vector<PoseFollowChainModifier::RuntimeDebugCapsule> poseFollowCapsules{};
            poseFollowModifier->Build_RuntimeDebugCapsules(modifierModel.get(), poseFollowCapsules);
            for (const auto& capsule : poseFollowCapsules)
            {
                capsules.push_back({
                    capsule.startPoint,
                    capsule.endPoint,
                    capsule.sourceRadius,
                    capsule.collisionRadius });
            }
            if (!capsules.empty())
                break;
        }

        if (auto* physicsModifier = dynamic_cast<PhysicsModifier*>(modifier.get()))
        {
            auto* desc = static_cast<PhysicsModifier::PhysicsModifierDesc*>(
                physicsModifier->Get_RuntimeReflectionTarget());
            if (desc == nullptr || !desc->debugDrawCollisionCapsules)
                continue;

            if (desc->debugDrawCollisionCapsules)
            {
                vector<PhysicsModifier::RuntimeDebugCapsule> physicsCapsules{};
                physicsModifier->Build_RuntimeDebugCapsules(physicsCapsules);
                for (const auto& capsule : physicsCapsules)
                {
                    capsules.push_back({
                        capsule.startPoint,
                        capsule.endPoint,
                        capsule.sourceRadius,
                        capsule.collisionRadius });
                }
            }

            if (desc->debugDrawCollisionCapsules)
            {
                vector<PhysicsModifier::RuntimeDebugSegment> physicsSegments{};
                physicsModifier->Build_RuntimeDebugSegments(physicsSegments);
                for (const auto& segment : physicsSegments)
                {
                    segments.push_back({
                        segment.startPoint,
                        segment.endPoint,
                        segment.hasContact,
                        segment.hasPush,
                        false,
                        -1 });
                }
            }

            if (desc->debugDrawCollisionCapsules)
            {
                vector<PhysicsModifier::RuntimeDebugSphere> physicsSpheres{};
                physicsModifier->Build_RuntimeDebugSpheres(physicsSpheres);
                for (const auto& sphere : physicsSpheres)
                {
                    spheres.push_back({
                        sphere.center,
                        sphere.radius,
                        sphere.hasContact });
                }
            }

            if (!capsules.empty() || !segments.empty() || !spheres.empty())
                break;
        }
    }

    if (capsules.empty() && segments.empty() && spheres.empty())
    {
        const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
        auto appendSimonHairDebug = [&](Client::Mon_Simon_Hair* simonHair)
        {
            if (simonHair == nullptr)
                return;

            vector<PhysicsModifier::RuntimeDebugCapsule> simonCapsules{};
            simonHair->Build_RuntimeDebugCapsules(simonCapsules);
            for (const auto& capsule : simonCapsules)
            {
                capsules.push_back({
                    capsule.startPoint,
                    capsule.endPoint,
                    capsule.sourceRadius,
                    capsule.collisionRadius });
            }

            vector<PhysicsModifier::RuntimeDebugSphere> simonSpheres{};
            simonHair->Build_RuntimeDebugSpheres(simonSpheres);
            for (const auto& sphere : simonSpheres)
            {
                spheres.push_back({
                    sphere.center,
                    sphere.radius,
                    sphere.hasContact });
            }

            vector<PhysicsModifier::RuntimeDebugSegment> simonSegments{};
            simonHair->Build_RuntimeDebugSegments(simonSegments);
            for (const auto& segment : simonSegments)
            {
                segments.push_back({
                    segment.startPoint,
                    segment.endPoint,
                    segment.hasContact,
                    segment.hasPush,
                    false,
                    -1 });
            }
        };

        appendSimonHairDebug(dynamic_cast<Client::Mon_Simon_Hair*>(previewRootObject.get()));

        const Shared<ContainerObject> previewContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
        if (previewContainer != nullptr)
        {
            appendSimonHairDebug(dynamic_cast<Client::Mon_Simon_Hair*>(previewContainer->Get_PartObject(L"Part_Hair")));
        }
    }

    if (capsules.empty() && segments.empty() && spheres.empty())
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewOverlayDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    const auto drawLine =
        [&](const Vec3& start, const Vec3& end, const XMVECTORF32& color)
    {
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(start, color),
            DirectX::VertexPositionColor(end, color));
    };

    const auto drawRing =
        [&](const Vec3& center, const Vec3& axisA, const Vec3& axisB, float radius, const XMVECTORF32& color)
    {
        constexpr int segmentCount = 24;
        for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
        {
            const float t0 = XM_2PI * static_cast<float>(segmentIndex) / static_cast<float>(segmentCount);
            const float t1 = XM_2PI * static_cast<float>(segmentIndex + 1) / static_cast<float>(segmentCount);
            const Vec3 point0 = center + axisA * (cosf(t0) * radius) + axisB * (sinf(t0) * radius);
            const Vec3 point1 = center + axisA * (cosf(t1) * radius) + axisB * (sinf(t1) * radius);
            drawLine(point0, point1, color);
        }
    };

    const auto drawCapsule =
        [&](const RuntimeCollisionCapsule& capsule)
    {
        Vec3 axis = capsule.endPoint - capsule.startPoint;
        if (capsule.collisionRadius <= 0.f && capsule.sourceRadius <= 0.f)
            return;

        const auto drawCapsuleRings =
            [&](float radius, const XMVECTORF32& axisColor, const XMVECTORF32& ringColor)
        {
            if (radius <= 0.f)
                return;

            if (axis.LengthSquared() <= 0.000001f)
            {
                drawRing(capsule.startPoint, Vec3::UnitX, Vec3::UnitY, radius, ringColor);
                drawRing(capsule.startPoint, Vec3::UnitX, Vec3::UnitZ, radius, ringColor);
                drawRing(capsule.startPoint, Vec3::UnitY, Vec3::UnitZ, radius, ringColor);
                return;
            }

            Vec3 localAxis = axis;
            localAxis.Normalize();
            Vec3 right = localAxis.Cross(Vec3::UnitY);
            if (right.LengthSquared() <= 0.000001f)
                right = localAxis.Cross(Vec3::UnitX);
            if (right.LengthSquared() <= 0.000001f)
                right = Vec3::UnitX;
            right.Normalize();

            Vec3 up = right.Cross(localAxis);
            if (up.LengthSquared() <= 0.000001f)
                up = Vec3::UnitY;
            else
                up.Normalize();

            drawLine(capsule.startPoint, capsule.endPoint, axisColor);
            drawRing(capsule.startPoint, right, up, radius, ringColor);
            drawRing(capsule.endPoint, right, up, radius, ringColor);
            drawLine(capsule.startPoint + right * radius, capsule.endPoint + right * radius, axisColor);
            drawLine(capsule.startPoint - right * radius, capsule.endPoint - right * radius, axisColor);
            drawLine(capsule.startPoint + up * radius, capsule.endPoint + up * radius, axisColor);
            drawLine(capsule.startPoint - up * radius, capsule.endPoint - up * radius, axisColor);
        };

        if (axis.LengthSquared() <= 0.000001f)
        {
            drawCapsuleRings(capsule.sourceRadius, DirectX::Colors::SlateGray, DirectX::Colors::SlateGray);
            drawCapsuleRings(capsule.collisionRadius, DirectX::Colors::DeepSkyBlue, DirectX::Colors::DeepSkyBlue);
            return;
        }

        drawCapsuleRings(capsule.sourceRadius, DirectX::Colors::SlateGray, DirectX::Colors::SlateGray);
        drawCapsuleRings(capsule.collisionRadius, DirectX::Colors::DeepSkyBlue, DirectX::Colors::DeepSkyBlue);
    };

    _previewGridBatch->Begin();
    for (const auto& capsule : capsules)
        drawCapsule(capsule);
    for (const auto& sphere : spheres)
    {
        const XMVECTORF32 sphereColor = sphere.hasContact ? DirectX::Colors::Yellow : DirectX::Colors::GreenYellow;
        drawRing(sphere.center, Vec3::UnitX, Vec3::UnitY, sphere.radius, sphereColor);
        drawRing(sphere.center, Vec3::UnitX, Vec3::UnitZ, sphere.radius, sphereColor);
        drawRing(sphere.center, Vec3::UnitY, Vec3::UnitZ, sphere.radius, sphereColor);
    }
    for (const auto& segment : segments)
    {
        const XMVECTORF32 color =
            segment.hasContact
            ? DirectX::Colors::Red
            : (segment.hasPush
                ? DirectX::Colors::Orange
                : (segment.isSolvedSegment ? DirectX::Colors::Magenta : DirectX::Colors::Cyan));
        drawLine(
            segment.startPoint,
            segment.endPoint,
            color);
    }
    _previewGridBatch->End();
}

void Animation_View::Draw_TrailAnchorPreview() const
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
        return;

    Vec3 baseWorld{};
    Vec3 tipWorld{};
    if (!Try_ResolveTrailAnchorWorldPosition(playbackModel, _trailAnchorAsset.base, baseWorld))
        return;
    if (!Try_ResolveTrailAnchorWorldPosition(playbackModel, _trailAnchorAsset.tip, tipWorld))
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewOverlayDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    constexpr float markerSize = 0.08f;
    _previewGridBatch->Begin();

    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld, DirectX::Colors::Orange),
        DirectX::VertexPositionColor(tipWorld, DirectX::Colors::Cyan));

    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld + Vec3(-markerSize, 0.f, 0.f), DirectX::Colors::Orange),
        DirectX::VertexPositionColor(baseWorld + Vec3(markerSize, 0.f, 0.f), DirectX::Colors::Orange));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, -markerSize, 0.f), DirectX::Colors::Orange),
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, markerSize, 0.f), DirectX::Colors::Orange));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, 0.f, -markerSize), DirectX::Colors::Orange),
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, 0.f, markerSize), DirectX::Colors::Orange));

    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(tipWorld + Vec3(-markerSize, 0.f, 0.f), DirectX::Colors::Cyan),
        DirectX::VertexPositionColor(tipWorld + Vec3(markerSize, 0.f, 0.f), DirectX::Colors::Cyan));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, -markerSize, 0.f), DirectX::Colors::Cyan),
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, markerSize, 0.f), DirectX::Colors::Cyan));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, 0.f, -markerSize), DirectX::Colors::Cyan),
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, 0.f, markerSize), DirectX::Colors::Cyan));

    _previewGridBatch->End();
}

Animation_View::PreviewWorldTimeScaleState Animation_View::Resolve_PreviewWorldTimeScaleState(float currentTimeSec) const
{
    PreviewWorldTimeScaleState result{};

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return result;

    float selectedStartSec = -1.f;
    int32 selectedId = -1;

    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        float resolvedWorldTimeScale = 1.f;
        bool resolvedScaleEffects = true;
        bool resolved = false;

        if (entry.notifyState->Get_TypeName() == "ANS_WorldTimeScale")
        {
            Shared<Client::ANS_WorldTimeScale> worldTimeScaleState =
                dynamic_pointer_cast<Client::ANS_WorldTimeScale>(entry.notifyState);
            if (worldTimeScaleState == nullptr)
                continue;

            resolvedWorldTimeScale = worldTimeScaleState->Get_WorldTimeScale();
            resolvedScaleEffects = worldTimeScaleState->Should_ScalePreviewEffects();
            resolved = true;
        }
        else if (entry.notifyState->Get_TypeName() == "ANS_JumpCounter")
        {
            Shared<Client::ANS_JumpCounter> jumpCounterState =
                dynamic_pointer_cast<Client::ANS_JumpCounter>(entry.notifyState);
            if (jumpCounterState == nullptr)
                continue;

            resolvedWorldTimeScale = jumpCounterState->Get_JumpCounterWorldTimeScale();
            resolvedScaleEffects = true;
            resolved = true;
        }

        if (!resolved)
            continue;

        if (entry.startSec < selectedStartSec)
            continue;
        if (entry.startSec == selectedStartSec && entry.id < selectedId)
            continue;

        selectedStartSec = entry.startSec;
        selectedId = entry.id;
        result.worldTimeScale = max(0.f, resolvedWorldTimeScale);
        result.scaleEffects = resolvedScaleEffects;
    }

    return result;
}

void Animation_View::Clear_SkillPhasePreviewContext()
{
    if (!_skillPhaseContext.enabled)
        return;

    _skillPhaseContext = {};
    Reset_PreviewAnimSpeedStates();
    Apply_PreviewBaseFrameSpeed();
}

bool Animation_View::Sync_SkillPhasePreviewContextForAnimation(const AnimationEntry& entry)
{
    if (_sourceMode != SourceMode::LiveModel)
        return false;

    const vector<string> skillIds = CLIENT->Get_AllSkillIds();
    for (const string& skillId : skillIds)
    {
        const Client::Skill_Data* skillData = CLIENT->Get_Skill(skillId);
        if (skillData == nullptr)
            continue;

        for (const Client::BattleActionPhase& phase : skillData->AnimPhases)
        {
            if (phase.animationKey.empty())
                continue;

            const bool matchesAnimation =
                phase.animationKey == entry.displayName ||
                phase.animationKey == entry.name ||
                phase.animationKey == entry.playbackName;
            if (!matchesAnimation)
                continue;

            _skillPhaseContext.enabled = true;
            _skillPhaseContext.skillKey = skillId;
            _skillPhaseContext.phaseIndex = phase.phaseIndex;
            _skillPhaseContext.animationKey = phase.animationKey;
            _skillPhaseContext.playRate = phase.playRate > 0.f ? phase.playRate : 1.f;
            Reset_PreviewAnimSpeedStates(); // 직접 클립 선택도 DT_SkillAction phase 배속을 base speed로 삼는다.
            return true;
        }
    }

    return false;
}

bool Animation_View::Set_SkillPhasePreviewContext(const string& skillKey, uint32 phaseIndex)
{
    const Client::Skill_Data* skillData = CLIENT->Get_Skill(skillKey);
    if (skillData == nullptr)
        return false;

    const Client::BattleActionPhase* selectedPhase = nullptr;
    for (const Client::BattleActionPhase& phase : skillData->AnimPhases)
    {
        if (phase.phaseIndex == phaseIndex)
        {
            selectedPhase = &phase;
            break;
        }
    }

    if (selectedPhase == nullptr || selectedPhase->animationKey.empty())
        return false;

    _skillPhaseContext.enabled = true;
    _skillPhaseContext.skillKey = skillKey;
    _skillPhaseContext.phaseIndex = selectedPhase->phaseIndex;
    _skillPhaseContext.animationKey = selectedPhase->animationKey;
    _skillPhaseContext.playRate = selectedPhase->playRate > 0.f ? selectedPhase->playRate : 1.f;

    _currentAnimationName = selectedPhase->animationKey;
    Reset_PreviewAnimSpeedStates(); // 이전 ANS_AnimSpeed 복구 상태를 끊고 새 phase base speed로 맞춘다.
    Sync_PreviewAnimationSelection(false);
    Apply_PreviewBaseFrameSpeed();

    return true;
}

float Animation_View::Resolve_PreviewBaseFrameSpeed() const
{
    if (_skillPhaseContext.enabled)
        return _skillPhaseContext.playRate > 0.f ? _skillPhaseContext.playRate : 1.f;

    return 1.f;
}

void Animation_View::Apply_PreviewBaseFrameSpeed()
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel != nullptr)
        playbackModel->Set_FrameSpeed(Resolve_PreviewBaseFrameSpeed());
}

void Animation_View::Update_PreviewAnimSpeedStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewAnimSpeedStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewAnimSpeedStates();
        return;
    }

    const float currentTimeSec = _previewModelCom->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = _previewModelCom->Get_Owner().get();
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = currentTimeSec;
    context.deltaTime = timeDelta;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_AnimSpeed")
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

        auto runtimeIt = std::find_if(
            _previewAnimSpeedStateRuntimes.begin(),
            _previewAnimSpeedStateRuntimes.end(),
            [&](const PreviewAnimSpeedStateRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewAnimSpeedStateRuntimes.end())
        {
            PreviewAnimSpeedStateRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewAnimSpeedStateRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext); // preview sampling ?熬곣뫖???꾩룄?????誘る닔? ?筌뤿굝???類ｋ펲.
            continue;
        }

        if (runtimeIt->notifyState != entry.notifyState ||
            runtimeIt->payloadJson != payloadJson)
        {
            if (runtimeIt->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtimeIt->notifyState->On_End(endContext);
            }

            runtimeIt->notifyState = entry.notifyState;
            runtimeIt->payloadJson = payloadJson;
            entry.notifyState->On_Begin(stateContext);
        }
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewAnimSpeedStateRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewAnimSpeedStateRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewAnimSpeedStateRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            _previewAnimSpeedStateRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewAnimSpeedStateRuntimes.erase(_previewAnimSpeedStateRuntimes.begin() + runtimeIndex);
    }

    if (_previewAnimSpeedStateRuntimes.empty())
        Apply_PreviewBaseFrameSpeed();
}

void Animation_View::Reset_PreviewAnimSpeedStates()
{
    if (_previewAnimSpeedStateRuntimes.empty())
    {
        Apply_PreviewBaseFrameSpeed();
        return;
    }

    AnimNotifyContext context{};
    context.owner = _previewModelCom ? _previewModelCom->Get_Owner().get() : nullptr;
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.isPreview = true;

    for (PreviewAnimSpeedStateRuntime& runtime : _previewAnimSpeedStateRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(context);
    }

    _previewAnimSpeedStateRuntimes.clear();

    Apply_PreviewBaseFrameSpeed(); // ANS_AnimSpeed 구간이 끝나면 선택된 DT_SkillAction phase 배속으로 복구한다.
}

void Animation_View::Update_PreviewPlayEffectNotifies(float previousTimeSec, float currentTimeSec, float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewPlayEffectStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewPlayEffectStates();
        return;
    }

    const bool wrapped = currentTimeSec + 1e-4f < previousTimeSec;
    const bool timeAdvanced = wrapped || currentTimeSec > previousTimeSec;

    AnimNotifyContext context{};
    context.owner = _previewModelCom->Get_Owner().get();
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.previousTimeSec = previousTimeSec;
    context.currentTimeSec = currentTimeSec;
    context.deltaTime = timeDelta;
    context.isLooping = wrapped;
    context.wrapped = wrapped;
    context.isPreview = true;

    if (timeAdvanced)
    {
        const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
        const bool includeStartEdge = _previewPlayEffectIncludeStartEdge;

        for (const AnimNotifyEventEntry& entry : animationData->notifies)
        {
            if (entry.notify == nullptr)
                continue;
            const string typeName = entry.notify->Get_TypeName();
            if (typeName != "AN_PlayEffect" && typeName != "AN_PlaySound")
                continue;
            if (typeName == "AN_PlaySound" && !_previewSoundEnabled)
                continue;
            if (!Is_PreviewNotifyTimeInRange(entry.timeSec, previousTimeSec, currentTimeSec, wrapped, includeStartEdge))
                continue;

            AnimNotifyContext notifyContext = context;
            if (typeName == "AN_PlayEffect" && previewRootObject != nullptr)
                notifyContext.owner = previewRootObject.get();

            entry.notify->Execute(notifyContext);
        }

        _previewPlayEffectIncludeStartEdge = false;
    }

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        const string typeName = entry.notifyState->Get_TypeName();
        if (typeName != "ANS_PlayEffect" && typeName != "ANS_WeakColor")
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

        auto runtimeIt = std::find_if(
            _previewPlayEffectStateRuntimes.begin(),
            _previewPlayEffectStateRuntimes.end(),
            [&](const PreviewPlayEffectStateRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewPlayEffectStateRuntimes.end())
        {
            PreviewPlayEffectStateRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewPlayEffectStateRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext);
            continue;
        }

        if (runtimeIt->notifyState != entry.notifyState ||
            runtimeIt->payloadJson != payloadJson)
        {
            if (runtimeIt->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtimeIt->notifyState->On_End(endContext);
            }

            runtimeIt->notifyState = entry.notifyState;
            runtimeIt->payloadJson = payloadJson;
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
}

void Animation_View::Reset_PreviewPlayEffectStates()
{
    _previewPlayEffectIncludeStartEdge = false;

    if (_previewPlayEffectStateRuntimes.empty())
        return;

    AnimNotifyContext context{};
    context.owner = _previewModelCom ? _previewModelCom->Get_Owner().get() : nullptr;
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.isPreview = true;

    if (context.owner != nullptr)
    {
        for (PreviewPlayEffectStateRuntime& runtime : _previewPlayEffectStateRuntimes)
        {
            if (runtime.notifyState)
                runtime.notifyState->On_End(context);
        }
    }

    _previewPlayEffectStateRuntimes.clear();
}

void Animation_View::Update_PreviewScreenFxCueStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewScreenFxCueStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewScreenFxCueStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
    {
        Reset_PreviewScreenFxCueStates();
        return;
    }

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = playbackModel->Get_Owner().get();
    context.model = playbackModel.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = currentTimeSec;
    context.previousTimeSec = max(0.f, currentTimeSec - max(0.f, timeDelta));
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_ScreenFxCue")
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

        auto runtimeIt = std::find_if(
            _previewScreenFxCueRuntimes.begin(),
            _previewScreenFxCueRuntimes.end(),
            [&](const PreviewScreenFxCueRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewScreenFxCueRuntimes.end())
        {
            PreviewScreenFxCueRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewScreenFxCueRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext);
            continue;
        }

        if (runtimeIt->notifyState != entry.notifyState ||
            runtimeIt->payloadJson != payloadJson)
        {
            if (runtimeIt->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtimeIt->notifyState->On_End(endContext);
            }

            runtimeIt->notifyState = entry.notifyState;
            runtimeIt->payloadJson = payloadJson;
            entry.notifyState->On_Begin(stateContext);
            continue;
        }

        entry.notifyState->On_Tick(stateContext);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewScreenFxCueRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewScreenFxCueRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewScreenFxCueRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            endContext.notifyStateDurationSec = 0.f;
            _previewScreenFxCueRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewScreenFxCueRuntimes.erase(_previewScreenFxCueRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewScreenFxCueStates()
{
    if (_previewScreenFxCueRuntimes.empty())
        return;

    AnimNotifyContext context{};
    context.owner = _previewModelCom ? _previewModelCom->Get_Owner().get() : nullptr;
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.isPreview = true;

    for (PreviewScreenFxCueRuntime& runtime : _previewScreenFxCueRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(context);
    }

    _previewScreenFxCueRuntimes.clear();
}

void Animation_View::Update_PreviewSourceHistoryEffect(float timeDelta)
{
    const vector<const AnimNotifyStateEntry*> activeStates = Find_ActivePreviewSourceHistoryStates();
    if (activeStates.empty())
    {
        _previewSourceHistoryProviderStateId = -1;

        for (PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
            Begin_PreviewSourceHistoryDrain(runtime);

        for (int32 runtimeIndex = static_cast<int32>(_previewSourceHistoryRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
        {
            if (Tick_PreviewSourceHistoryDrain(_previewSourceHistoryRuntimes[runtimeIndex], timeDelta))
                _previewSourceHistoryRuntimes.erase(_previewSourceHistoryRuntimes.begin() + runtimeIndex);
        }

        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const float currentTimeSec = playbackModel ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;
    vector<int32> activeStateIds{};
    activeStateIds.reserve(activeStates.size());

    for (const AnimNotifyStateEntry* activeState : activeStates)
    {
        if (activeState == nullptr || activeState->notifyState == nullptr)
            continue;

        activeStateIds.push_back(activeState->id);

        const json payload = activeState->notifyState->To_Json();
        const string partTag = payload.value("_partTag", string{ "Part_Weapon" });
        const string effectNameValue = payload.value("_effectName", string{});
        int32 sourceBindingMode = payload.value("_sourceBindingMode", 2);
        if (sourceBindingMode == 0)
            sourceBindingMode = 2;
        const string socketName = payload.value("_socketName", string{});
        string sourceHistoryAnchorName = payload.value("_sourceHistoryAnchorName", string{ "Source" });
        if (sourceHistoryAnchorName.empty())
            sourceHistoryAnchorName = "Source";
        Vec3 sourceHistoryLocalOffset = Vec3::Zero;
        if (payload.contains("_sourceHistoryLocalOffset") &&
            payload["_sourceHistoryLocalOffset"].is_array() &&
            payload["_sourceHistoryLocalOffset"].size() >= 3)
        {
            sourceHistoryLocalOffset.x = payload["_sourceHistoryLocalOffset"][0].get<float>();
            sourceHistoryLocalOffset.y = payload["_sourceHistoryLocalOffset"][1].get<float>();
            sourceHistoryLocalOffset.z = payload["_sourceHistoryLocalOffset"][2].get<float>();
        }
        if (effectNameValue.empty())
            continue;

        const wstring resolvedEffectName = String::ToWString(effectNameValue);

        PreviewSourceHistoryRuntime* runtime = nullptr;
        for (PreviewSourceHistoryRuntime& entry : _previewSourceHistoryRuntimes)
        {
            if (entry.stateId == activeState->id)
            {
                runtime = &entry;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewSourceHistoryRuntime newRuntime{};
            newRuntime.stateId = activeState->id;
            newRuntime.partTag = partTag;
            newRuntime.sourceBindingMode = sourceBindingMode;
            newRuntime.socketName = socketName;
            newRuntime.sourceHistoryAnchorName = sourceHistoryAnchorName;
            newRuntime.sourceHistoryLocalOffset = sourceHistoryLocalOffset;
            _previewSourceHistoryRuntimes.push_back(std::move(newRuntime));
            runtime = &_previewSourceHistoryRuntimes.back();
        }

        if (runtime->isDraining)
        {
            if (runtime->effect)
                runtime->effect->Stop();

            runtime->effect.reset();
            runtime->effectName.clear();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
            runtime->lastTimeSec = -1.f;
            runtime->trailAnchorAsset = {};
        }

        if (runtime->partTag != partTag ||
            runtime->sourceBindingMode != sourceBindingMode ||
            runtime->socketName != socketName ||
            runtime->sourceHistoryAnchorName != sourceHistoryAnchorName ||
            runtime->sourceHistoryLocalOffset != sourceHistoryLocalOffset)
        {
            if (runtime->effect)
                runtime->effect->Stop();

            runtime->partTag = partTag;
            runtime->sourceBindingMode = sourceBindingMode;
            runtime->socketName = socketName;
            runtime->sourceHistoryAnchorName = sourceHistoryAnchorName;
            runtime->sourceHistoryLocalOffset = sourceHistoryLocalOffset;
            runtime->effect.reset();
            runtime->effectName.clear();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
            runtime->lastTimeSec = -1.f;
        }

        if (runtime->sourceBindingMode == 3 && runtime->trailAnchorAsset.modelGuid.empty())
        {
            PreviewTrailRuntime trailRuntime{};
            if (!Load_PreviewTrailAnchorAsset(runtime->partTag, trailRuntime))
                continue;

            runtime->modelGuid = trailRuntime.modelGuid;
            runtime->trailAnchorAsset = trailRuntime.trailAnchorAsset;
        }

        if (!Ensure_PreviewSourceHistoryEffect(*runtime, resolvedEffectName))
            continue;

        const bool timeMovedBackward =
            runtime->lastTimeSec >= 0.f &&
            currentTimeSec + 1e-4f < runtime->lastTimeSec;
        if (timeMovedBackward)
        {
            runtime->effect->Stop();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
        }

        EffectSourcePointSample sourceSample{};
        if (!Try_GetSourcePointSampleForRuntime(*runtime, sourceSample))
        {
            if (runtime->effect && runtime->isPlaying)
                Begin_PreviewSourceHistoryDrain(*runtime);
            continue;
        }

        if (!runtime->isPlaying)
        {
            runtime->effect->Bind_SourcePointSampleProvider(static_pointer_cast<IEffectSourcePointSampleProvider>(GetSharedPtr<Animation_View>()));
            if (FAILED(runtime->effect->Reset()))
                continue;

            const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
            if (effectTransform == nullptr)
                continue;

            effectTransform->Set_WorldPosition(sourceSample.worldPosition);

            if (FAILED(runtime->effect->Play()))
                continue;

            runtime->isPlaying = true;
        }

        const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
        if (effectTransform == nullptr)
            continue;

        effectTransform->Set_WorldPosition(sourceSample.worldPosition);
        _previewSourceHistoryProviderStateId = runtime->stateId;
        runtime->effect->Priority_Update(timeDelta);
        runtime->effect->Update(timeDelta);
        runtime->lastTimeSec = currentTimeSec;
    }

    _previewSourceHistoryProviderStateId = -1;

    for (int32 runtimeIndex = static_cast<int32>(_previewSourceHistoryRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewSourceHistoryRuntimes[runtimeIndex].stateId;
        const bool stillActive =
            std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();

        if (stillActive)
            continue;

        Begin_PreviewSourceHistoryDrain(_previewSourceHistoryRuntimes[runtimeIndex]);
        if (Tick_PreviewSourceHistoryDrain(_previewSourceHistoryRuntimes[runtimeIndex], timeDelta))
            _previewSourceHistoryRuntimes.erase(_previewSourceHistoryRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewSourceHistoryEffect()
{
    for (PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
    {
        if (runtime.effect)
            runtime.effect->Stop();
    }

    _previewSourceHistoryRuntimes.clear();
    _previewSourceHistoryProviderStateId = -1;
}

void Animation_View::Begin_PreviewSourceHistoryDrain(PreviewSourceHistoryRuntime& runtime)
{
    if (runtime.effect == nullptr || runtime.isDraining)
        return;

    runtime.isDraining = true;
    runtime.drainElapsed = 0.f;
    runtime.isPlaying = true;
}

bool Animation_View::Tick_PreviewSourceHistoryDrain(PreviewSourceHistoryRuntime& runtime, float timeDelta)
{
    if (runtime.effect == nullptr)
        return true;

    if (!runtime.isDraining)
        return false;

    runtime.drainElapsed += max(0.f, timeDelta);
    if (runtime.drainElapsed < runtime.drainDuration)
    {
        runtime.effect->Priority_Update(timeDelta);
        runtime.effect->Update(timeDelta);
        runtime.effect->Late_Update(timeDelta);
        return false;
    }

    runtime.effect->Stop();
    return true;
}

vector<const AnimNotifyStateEntry*> Animation_View::Find_ActivePreviewSourceHistoryStates() const
{
    vector<const AnimNotifyStateEntry*> activeStates{};

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (_sourceMode != SourceMode::LiveModel || playbackModel == nullptr)
        return activeStates;

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return activeStates;

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_SourceHistoryEffect")
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStates.push_back(&entry);
    }

    return activeStates;
}

bool Animation_View::Ensure_PreviewSourceHistoryEffect(PreviewSourceHistoryRuntime& runtime, const wstring& effectName)
{
    if (runtime.effect && runtime.effectName == effectName)
        return true;

    if (runtime.effect)
        runtime.effect->Stop();

    runtime.effect.reset();
    runtime.effectName.clear();
    runtime.isPlaying = false;
    runtime.isDraining = false;
    runtime.drainElapsed = 0.f;
    runtime.lastTimeSec = -1.f;

    const Shared<const EffectDefinition> definition = EffectAssetRuntimeLoader::Load_DefinitionByName(effectName);
    if (definition == nullptr)
    {
        LOG_WARN("Preview SourceHistory effect definition load failed. effect={}", String::ToString(effectName));
        return false;
    }

    runtime.drainDuration = Resolve_SourceHistoryDrainDuration(definition);

    const Shared<EffectInstance> prototype = EffectInstance::Create(GAME->Get_Device(), GAME->Get_Context());
    if (prototype == nullptr)
        return false;

    EffectInstanceDesc effectDesc{};
    effectDesc.definition = definition;

    const Shared<GameObject> previewEffectObject = prototype->Clone(&effectDesc);
    runtime.effect = dynamic_pointer_cast<EffectInstance>(previewEffectObject);
    if (runtime.effect == nullptr)
        return false;

    runtime.effect->Bind_SourcePointSampleProvider(static_pointer_cast<IEffectSourcePointSampleProvider>(GetSharedPtr<Animation_View>()));
    CHECK_FAILED(runtime.effect->Try_BeginPlay(), false);
    CHECK_FAILED(runtime.effect->Reset(), false);

    runtime.effectName = effectName;
    return true;
}

void Animation_View::Update_PreviewTrailEffect(float timeDelta)
{
    const vector<const AnimNotifyStateEntry*> activeTrailStates = Find_ActivePreviewTrailStates();
    if (activeTrailStates.empty())
    {
        _previewTrailProviderStateId = -1;

        for (PreviewTrailRuntime& runtime : _previewTrailRuntimes)
            Begin_PreviewTrailDrain(runtime);

        for (int32 runtimeIndex = static_cast<int32>(_previewTrailRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
        {
            if (Tick_PreviewTrailDrain(_previewTrailRuntimes[runtimeIndex], timeDelta))
                _previewTrailRuntimes.erase(_previewTrailRuntimes.begin() + runtimeIndex);
        }

        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const float currentTimeSec = playbackModel ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;
    vector<int32> activeStateIds{};
    activeStateIds.reserve(activeTrailStates.size());

    for (const AnimNotifyStateEntry* activeTrailState : activeTrailStates)
    {
        if (activeTrailState == nullptr || activeTrailState->notifyState == nullptr)
            continue;

        const Client::ANS_Trail* trailNotifyState =
            dynamic_cast<const Client::ANS_Trail*>(activeTrailState->notifyState.get());
        if (trailNotifyState == nullptr)
            continue;

        activeStateIds.push_back(activeTrailState->id);

        const json payload = activeTrailState->notifyState->To_Json();
        const string partTag = payload.value("_partTag", string{ "Part_Weapon" });
        const string effectNameValue = payload.value("_effectName", string{});
        const Client::ANS_Trail::TrailAnchorSource trailAnchorSource = trailNotifyState->Get_TrailAnchorSource();
        const string baseBoneName = trailNotifyState->Get_BaseBoneName();
        const Vec3 baseLocalOffset = trailNotifyState->Get_BaseLocalOffset();
        const string tipBoneName = trailNotifyState->Get_TipBoneName();
        const Vec3 tipLocalOffset = trailNotifyState->Get_TipLocalOffset();
        wstring resolvedEffectName{};
        if (!Try_ResolvePreviewTrailEffectName(effectNameValue, partTag, resolvedEffectName))
            continue;

        PreviewTrailRuntime* runtime = nullptr;
        for (PreviewTrailRuntime& entry : _previewTrailRuntimes)
        {
            if (entry.stateId == activeTrailState->id)
            {
                runtime = &entry;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewTrailRuntime newRuntime{};
            newRuntime.stateId = activeTrailState->id;
            newRuntime.partTag = partTag; // 각 ANS_Trail이 지정한 partTag를 자기 runtime에 고정한다.
            newRuntime.trailAnchorSource = trailAnchorSource;
            newRuntime.baseBoneName = baseBoneName;
            newRuntime.baseLocalOffset = baseLocalOffset;
            newRuntime.tipBoneName = tipBoneName;
            newRuntime.tipLocalOffset = tipLocalOffset;
            _previewTrailRuntimes.push_back(std::move(newRuntime));
            runtime = &_previewTrailRuntimes.back();

            if (!Load_PreviewTrailAnchorAsset(runtime->partTag, *runtime))
                continue;
        }

        if (runtime->isDraining)
        {
            if (runtime->effect)
                runtime->effect->Stop();

            runtime->effect.reset();
            runtime->effectName.clear();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
            runtime->lastTimeSec = -1.f;
        }

        const bool anchorSourceChanged =
            runtime->partTag != partTag ||
            runtime->trailAnchorSource != trailAnchorSource ||
            runtime->baseBoneName != baseBoneName ||
            !Is_SameVec3(runtime->baseLocalOffset, baseLocalOffset) ||
            runtime->tipBoneName != tipBoneName ||
            !Is_SameVec3(runtime->tipLocalOffset, tipLocalOffset);
        if (anchorSourceChanged)
        {
            if (runtime->effect)
                runtime->effect->Stop();

            runtime->partTag = partTag;   // 같은 state라도 partTag가 바뀌면 새 무기 기준으로 anchor를 다시 읽는다.
            runtime->trailAnchorSource = trailAnchorSource;
            runtime->baseBoneName = baseBoneName;
            runtime->baseLocalOffset = baseLocalOffset;
            runtime->tipBoneName = tipBoneName;
            runtime->tipLocalOffset = tipLocalOffset;
            runtime->effect.reset();
            runtime->effectName.clear();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
            runtime->lastTimeSec = -1.f;

            if (!Load_PreviewTrailAnchorAsset(runtime->partTag, *runtime))
                continue;
        }

        if (runtime->trailAnchorSource == Client::ANS_Trail::TrailAnchorSource::ModelDefault &&
            runtime->trailAnchorAsset.modelGuid.empty())
            continue;

        const float stateDuration = max(activeTrailState->durationSec, 0.0001f);
        runtime->offsetPhase = std::clamp((currentTimeSec - activeTrailState->startSec) / stateDuration, 0.f, 1.f);
        runtime->tipExtendCurve = trailNotifyState->Get_TipExtendCurve();
        runtime->baseLocalOffsetCurve = trailNotifyState->Get_BaseLocalOffsetCurve();
        runtime->tipLocalOffsetCurve = trailNotifyState->Get_TipLocalOffsetCurve();

        if (!Ensure_PreviewTrailEffect(*runtime, resolvedEffectName))
            continue;

        const bool timeMovedBackward =
            runtime->lastTimeSec >= 0.f &&
            currentTimeSec + 1e-4f < runtime->lastTimeSec;
        if (timeMovedBackward)
        {
            runtime->effect->Stop(); // scrub??seek??怨쀬Ŧ ??蹂?뜟?????댁Ŧ ?띠럾?嶺??????runtime history嶺??뺢퀗??怨ヂ????곕뻣 ??戮곗굚??類ｋ펲.
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
        }

        EffectTrailSample trailSample{};
        if (!Try_GetTrailSampleForRuntime(*runtime, trailSample))
        {
            if (runtime->effect && runtime->isPlaying)
            {
                Begin_PreviewTrailDrain(*runtime);
            }
            continue;
        }

        if (!runtime->isPlaying)
        {
            runtime->effect->Bind_TrailSampleProvider(static_pointer_cast<IEffectTrailSampleProvider>(GetSharedPtr<Animation_View>())); // preview??ComputeTrailEmitter?띠럾? sample provider???????base/tip???꾩룇猷뉔뇡??history?띠럾? ?癰?逾??
            if (FAILED(runtime->effect->Reset()))
                continue;

            const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
            if (effectTransform == nullptr)
                continue;

            effectTransform->Set_WorldPosition(trailSample.baseWorldPosition);

            if (FAILED(runtime->effect->Play()))
                continue;

            runtime->isPlaying = true;
        }

        const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
        if (effectTransform == nullptr)
            continue;

        effectTransform->Set_WorldPosition(trailSample.baseWorldPosition);
        _previewTrailProviderStateId = runtime->stateId; // ?띠룇?? provider????ㅻ쾴????濡モ뵹, ?熬곣뫗??effect update ?熬곣뫖???????runtime sample??繞벿븐뫒? 嶺뚯솘??筌먐삳┰??
        runtime->effect->Priority_Update(timeDelta);
        runtime->effect->Update(timeDelta);
        runtime->lastTimeSec = currentTimeSec;
    }

    _previewTrailProviderStateId = -1;

    for (int32 runtimeIndex = static_cast<int32>(_previewTrailRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewTrailRuntimes[runtimeIndex].stateId;
        const bool stillActive =
            std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();

        if (stillActive)
            continue;

        Begin_PreviewTrailDrain(_previewTrailRuntimes[runtimeIndex]);
        if (Tick_PreviewTrailDrain(_previewTrailRuntimes[runtimeIndex], timeDelta))
            _previewTrailRuntimes.erase(_previewTrailRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewTrailEffect()
{
    for (PreviewTrailRuntime& runtime : _previewTrailRuntimes)
    {
        if (runtime.effect)
            runtime.effect->Stop();
    }

    _previewTrailRuntimes.clear();
    _previewTrailProviderStateId = -1;
}

void Animation_View::Begin_PreviewTrailDrain(PreviewTrailRuntime& runtime)
{
    if (runtime.effect == nullptr || runtime.isDraining)
        return;

    runtime.isDraining = true;
    runtime.drainElapsed = 0.f;
    runtime.isPlaying = true;
}

bool Animation_View::Tick_PreviewTrailDrain(PreviewTrailRuntime& runtime, float timeDelta)
{
    if (runtime.effect == nullptr)
        return true;

    if (!runtime.isDraining)
        return false;

    runtime.drainElapsed += max(0.f, timeDelta);
    if (runtime.drainElapsed < runtime.drainDuration)
    {
        runtime.effect->Priority_Update(timeDelta);
        runtime.effect->Update(timeDelta);
        runtime.effect->Late_Update(timeDelta);
        return false;
    }

    runtime.effect->Stop();
    return true;
}

vector<const AnimNotifyStateEntry*> Animation_View::Find_ActivePreviewTrailStates() const
{
    vector<const AnimNotifyStateEntry*> activeStates{};

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (_sourceMode != SourceMode::LiveModel || playbackModel == nullptr)
        return activeStates;

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return activeStates;

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_Trail")
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStates.push_back(&entry);
    }

    return activeStates;
}

bool Animation_View::Ensure_PreviewTrailEffect(PreviewTrailRuntime& runtime, const wstring& effectName)
{
    if (runtime.effect && runtime.effectName == effectName)
        return true;

    if (runtime.effect)
        runtime.effect->Stop();

    runtime.effect.reset();
    runtime.effectName.clear();
    runtime.isPlaying = false;
    runtime.isDraining = false;
    runtime.drainElapsed = 0.f;
    runtime.lastTimeSec = -1.f;

    const Shared<const EffectDefinition> definition = EffectAssetRuntimeLoader::Load_DefinitionByName(effectName);
    if (definition == nullptr)
        return false;

    runtime.drainDuration = Resolve_TrailDrainDuration(definition);

    const Shared<EffectInstance> prototype = EffectInstance::Create(GAME->Get_Device(), GAME->Get_Context());
    if (prototype == nullptr)
        return false;

    EffectInstanceDesc effectDesc{};
    effectDesc.definition = definition;

    const Shared<GameObject> previewEffectObject = prototype->Clone(&effectDesc);
    runtime.effect = dynamic_pointer_cast<EffectInstance>(previewEffectObject);
    if (runtime.effect == nullptr)
        return false;

    runtime.effect->Bind_TrailSampleProvider(static_pointer_cast<IEffectTrailSampleProvider>(GetSharedPtr<Animation_View>()));
    CHECK_FAILED(runtime.effect->Try_BeginPlay(), false);
    CHECK_FAILED(runtime.effect->Reset(), false);

    runtime.effectName = effectName;
    return true;
}

bool Animation_View::Try_ResolvePreviewTrailEffectName(
    const string& effectNameOverride,
    const string& partTag,
    wstring& outEffectName) const
{
    outEffectName.clear();

    if (!effectNameOverride.empty())
    {
        outEffectName = String::ToWString(effectNameOverride);
        return true;
    }

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const PartObject* partObject = ownerContainer->Get_PartObject(String::ToWString(partTag));
    outEffectName = Resolve_PreviewDefaultTrailEffectName(partObject);
    return !outEffectName.empty();
}

void Animation_View::Assign_SelectedBoneToTrailAnchor(const Shared<ModelCom>& playbackModel, TrailAnchorSlot slot)
{
    if (playbackModel == nullptr || _selectedBoneIndex < 0)
        return;

    const uint32 boneIndex = static_cast<uint32>(_selectedBoneIndex);
    if (boneIndex >= playbackModel->Get_NumBones())
        return;

    Client::TrailAnchorPointDesc* target = nullptr;
    if (slot == TrailAnchorSlot::Base)
        target = &_trailAnchorAsset.base;
    else
        target = &_trailAnchorAsset.tip;

    target->boneName = playbackModel->Get_BoneName(boneIndex);
    target->localPosition = Vec3::Zero;

    _trailAnchorAsset.modelGuid = _modelGuid;
    MarkDirty();
}

bool Animation_View::Try_ResolveTrailAnchorWorldPosition(
    const Shared<ModelCom>& playbackModel,
    const Client::TrailAnchorPointDesc& anchor,
    Vec3& outWorldPosition) const
{
    if (playbackModel == nullptr || anchor.boneName.empty())
        return false;

    const int32 boneIndex = playbackModel->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = playbackModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    Shared<GameObject> ownerObject = playbackModel->Get_Owner();
    if (Shared<PartObject> ownerPart = dynamic_pointer_cast<PartObject>(ownerObject))
    {
        const Matrix anchorLocalMatrix = playbackModel->Get_ModelType() == ModelType::NonAnim
            ? Matrix::Identity
            : *boneMatrix;
        const Matrix anchorWorldMatrix = anchorLocalMatrix * ownerPart->Get_CombinedWorldMatrix();
        outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
        return true;
    }

    Shared<TransformCom> ownerTransform = ownerObject ? ownerObject->Get_Transform() : nullptr;
    const Matrix ownerWorldMatrix = ownerTransform ? ownerTransform->Get_WorldMatrix() : Matrix::Identity;

    const Matrix anchorLocalMatrix = playbackModel->Get_ModelType() == ModelType::NonAnim
        ? Matrix::Identity
        : *boneMatrix;
    const Matrix anchorWorldMatrix = anchorLocalMatrix * ownerWorldMatrix;
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

bool Animation_View::Try_ResolvePreviewTrailAnchorWorldPosition(
    const string& partTag,
    const Client::TrailAnchorPointDesc& anchor,
    Vec3& outWorldPosition) const
{
    if (anchor.boneName.empty())
        return false;

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewTrailPartTag = String::ToWString(partTag);
    PartObject* trailPart = ownerContainer->Get_PartObject(previewTrailPartTag);
    if (Resolve_PreviewTrailPartModel(trailPart) == nullptr)
        return false;

    const Shared<ModelCom> weaponModel = Resolve_PreviewTrailPartModel(trailPart);
    if (weaponModel == nullptr)
        return false;

    const int32 boneIndex = weaponModel->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = weaponModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorLocalMatrix = weaponModel->Get_ModelType() == ModelType::NonAnim
        ? Matrix::Identity
        : *boneMatrix;
    const Matrix anchorWorldMatrix = anchorLocalMatrix * trailPart->Get_CombinedWorldMatrix();
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

bool Animation_View::Try_ResolvePreviewWeaponAnchorWorldPosition(
    const string& partTag,
    const string& anchorName,
    const Vec3& localOffset,
    Vec3& outWorldPosition) const
{
    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewPartTag = String::ToWString(partTag);
    const PartObject* weaponPart = ownerContainer->Get_PartObject(previewPartTag);

    const Client::WeaponSourceHistoryAnchorDesc* anchor = nullptr;
    Shared<ModelCom> weaponModel = nullptr;

    if (const Weapon* weapon = dynamic_cast<const Weapon*>(weaponPart))
    {
        anchor = weapon->Find_SourceHistoryAnchor(anchorName);
        weaponModel = weapon->Get_Model();
    }
    else if (const Client::Mon_Simon_Weapon* simonWeapon = dynamic_cast<const Client::Mon_Simon_Weapon*>(weaponPart))
    {
        anchor = simonWeapon->Find_SourceHistoryAnchor(anchorName);
        weaponModel = simonWeapon->Get_Model();
    }
    else if (const Client::Mon_Dualliste_Sword_L* swordL = dynamic_cast<const Client::Mon_Dualliste_Sword_L*>(weaponPart))
    {
        anchor = swordL->Find_SourceHistoryAnchor(anchorName);
        weaponModel = swordL->Get_Model();
    }
    else if (const Client::Mon_Dualliste_Sword_R* swordR = dynamic_cast<const Client::Mon_Dualliste_Sword_R*>(weaponPart))
    {
        anchor = swordR->Find_SourceHistoryAnchor(anchorName);
        weaponModel = swordR->Get_Model();
    }
    else if (const Client::Mon_PotatoBag_Weapon* potatoBagWeapon = dynamic_cast<const Client::Mon_PotatoBag_Weapon*>(weaponPart))
    {
        anchor = potatoBagWeapon->Find_SourceHistoryAnchor(anchorName);
        weaponModel = potatoBagWeapon->Get_Model();
    }

    if (anchor == nullptr || anchor->boneName.empty())
        return false;

    if (weaponModel == nullptr)
        return false;

    const int32 boneIndex = weaponModel->Get_BoneIndex(anchor->boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = weaponModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix sourceLocalMatrix = weaponModel->Get_ModelType() == ModelType::NonAnim
        ? Matrix::Identity
        : *boneMatrix;
    const Matrix sourceWorldMatrix = sourceLocalMatrix * weaponPart->Get_CombinedWorldMatrix();
    outWorldPosition = Vec3::Transform(anchor->localPosition + localOffset, sourceWorldMatrix);
    return true;
}

bool Animation_View::Try_ResolvePreviewSocketWorldPosition(
    const string& partTag,
    const string& socketName,
    const Vec3& localOffset,
    Vec3& outWorldPosition) const
{
    if (socketName.empty())
        return false;

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewPartTag = String::ToWString(partTag);
    PartObject* partObject = ownerContainer->Get_PartObject(previewPartTag);
    if (partObject == nullptr)
        return false;

    if (const auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * playablePart->Get_CombinedWorldMatrix();
        outWorldPosition = Vec3::Transform(localOffset, socketWorldMatrix);
        return true;
    }

    if (Resolve_PreviewTrailPartModel(partObject) != nullptr)
    {
        const Matrix* socketMatrix = Resolve_PreviewTrailPartSocketMatrix(partObject, socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * partObject->Get_CombinedWorldMatrix();
        outWorldPosition = Vec3::Transform(localOffset, socketWorldMatrix);
        return true;
    }

    return false;
}

void Animation_View::Validate_SelectedBoneSelection()
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
    {
        _selectedBoneIndex = -1;
        return;
    }

    if (_selectedBoneIndex < 0)
        return;

    if (static_cast<uint32>(_selectedBoneIndex) >= playbackModel->Get_NumBones())
        _selectedBoneIndex = -1;
}

string Animation_View::Normalize_DefaultTrackName(const string& trackName)
{
    if (trackName.empty())
        return trackName;

    if (trackName == "Notify" || trackName == "Notify Track")
        return "Notify Track";

    if (trackName == "Notify State" || trackName == "Notify State Track")
        return "Notify State Track";

    return trackName;
}

string Animation_View::Make_AnimationComboLabel(const AnimationEntry& entry)
{
    return entry.displayName + "##" + entry.name;
}

const Animation_View::AnimationEntry* Animation_View::Find_AnimationEntryByAnyName(
    const vector<AnimationEntry>& entries,
    const string& name)
{
    for (const auto& entry : entries)
    {
        if (entry.name == name || entry.displayName == name || entry.playbackName == name)
            return &entry;
    }

    return nullptr;
}

int32 Animation_View::Find_AnimationIndex(const Shared<ModelCom>& modelCom, const string& animationName)
{
    if (modelCom == nullptr || animationName.empty())
        return -1;

    const uint32 numAnimations = static_cast<uint32>(modelCom->Get_NumAnimations());
    for (uint32 i = 0; i < numAnimations; ++i)
    {
        const char* currentName = modelCom->Get_AnimationName(i);
        if (currentName != nullptr && animationName == currentName)
            return static_cast<int32>(i);
    }

    return -1;
}

float Animation_View::Resolve_AnimNotifyDataLengthSec(const AnimNotifyData* animationData)
{
    if (animationData == nullptr)
        return 5.f;

    float maxTimeSec = 0.f;

    for (const auto& entry : animationData->notifies)
        maxTimeSec = max(maxTimeSec, entry.timeSec);

    for (const auto& entry : animationData->notifyStates)
        maxTimeSec = max(maxTimeSec, entry.startSec + entry.durationSec);

    return max(5.f, maxTimeSec);
}

Shared<GameObject> Animation_View::Resolve_RootPreviewObject(const Shared<GameObject>& gameObject)
{
    Shared<GameObject> rootObject = gameObject;
    while (rootObject != nullptr)
    {
        Shared<GameObject> parentObject = rootObject->Get_Owner();
        if (parentObject == nullptr)
            break;

        rootObject = parentObject;
    }

    return rootObject;
}

void Animation_View::Lock_PreviewObjectToOrigin(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return;

    Shared<TransformCom> transformCom = gameObject->Get_Transform();
    if (transformCom == nullptr)
        return;

    transformCom->Set_WorldPosition(Vec3::Zero);
}

bool Animation_View::Should_DeferToCompanionPreview()
{
    return !EDITOR->Is_ActivePreviewOwner(PreviewOwner::Animation);
}

Editor_Camera_Free::EDITOR_CAMERA_DESC Animation_View::Build_PreviewCameraDesc(const Shared<GameObject>& gameObject)
{
    UNREFERENCED_PARAMETER(gameObject);

    Editor_Camera_Free::EDITOR_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(1.15f, 1.35f, 5.2f, 1.f);
    desc.at = XMFLOAT4(0.25f, 1.0f, 0.f, 1.f);
    desc.fovy = XM_PIDIV4;
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.yaw = 0.f;
    desc.pitch = 0.f;
    desc.mouseSensor = 0.15f;
    desc.speedPerSec = 5.f;
    desc.degreePerSec = 180.f;

    return desc;
}

void Animation_View::Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera)
{
    if (camera == nullptr)
        return;

    Shared<TransformCom> cameraTransform = camera->Get_Transform();
    if (cameraTransform == nullptr)
        return;

    cameraTransform->Set_WorldPosition(Vec3(-1.35f, 2.25f, 6.2f));
    cameraTransform->LookAt(Vec3(-1.6f, -1.2f, -2.3f));
}

void Animation_View::Remove_NotifyTrackEntries(AnimNotifyData& animationData, int32 trackIndex)
{
    animationData.notifies.erase(
        remove_if(
            animationData.notifies.begin(),
            animationData.notifies.end(),
            [trackIndex](const AnimNotifyEventEntry& entry)
            {
                return entry.trackIndex == trackIndex;
            }),
        animationData.notifies.end());

    for (auto& entry : animationData.notifies)
    {
        if (entry.trackIndex > trackIndex)
            --entry.trackIndex;
    }
}

void Animation_View::Remove_NotifyStateTrackEntries(AnimNotifyData& animationData, int32 trackIndex)
{
    animationData.notifyStates.erase(
        remove_if(
            animationData.notifyStates.begin(),
            animationData.notifyStates.end(),
            [trackIndex](const AnimNotifyStateEntry& entry)
            {
                return entry.trackIndex == trackIndex;
            }),
        animationData.notifyStates.end());

    for (auto& entry : animationData.notifyStates)
    {
        if (entry.trackIndex > trackIndex)
            --entry.trackIndex;
    }
}

void Animation_View::Sync_PreviewAnimationSelection(bool keepCurrentTime)
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    const string playbackName = currentEntry ? currentEntry->playbackName : _currentAnimationName;
    const int32 animationIndex = Find_AnimationIndex(playbackModel, playbackName);
    if (animationIndex < 0)
        return;

    const float currentTimeSec = keepCurrentTime ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;
    playbackModel->Set_Animation(static_cast<uint32>(animationIndex), true);
    playbackModel->Set_AnimationPaused(false);
    playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
    playbackModel->Play_Animation(0.f); // preview 애니메이션 교체 직후에도 현재 시점 notify/state를 한 번 즉시 반영한다.
    Apply_PreviewBaseFrameSpeed();
    _previewPlayEffectIncludeStartEdge = true;
    _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
}

AnimNotifyData* Animation_View::Get_CurrentAnimation()
{
    if (_currentAnimationName.empty())
        return nullptr;

    if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName))
        return animation;

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    if (currentEntry == nullptr)
        return nullptr;

    return Find_OrNormalize_NotifyAnimation(_asset, *currentEntry);
}

const AnimNotifyData* Animation_View::Get_CurrentAnimation() const
{
    if (_currentAnimationName.empty())
        return nullptr;

    if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName))
        return animation;

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    if (currentEntry == nullptr)
        return nullptr;

    return Find_NotifyAnimation(_asset, *currentEntry);
}


Shared<Animation_View> Animation_View::Create()
{
    return make_shared<Animation_View>();
}
