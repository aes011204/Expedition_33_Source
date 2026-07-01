#include "AN_PlayEffectBattleDir.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Monster_Part.h"
#include "PlayableParts.h"
#include "TransformCom.h"
#include "Weapon.h"

namespace
{
Shared<GameObject> Resolve_NotifyOwnerRoot(GameObject* notifyOwner)
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (dynamic_pointer_cast<ContainerObject>(current))
            return current;

        if (current->Get_Owner() == nullptr)
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

bool Try_GetOwnerRootWorldMatrix(const Shared<GameObject>& ownerObject, Matrix& outWorldMatrix)
{
    if (ownerObject == nullptr)
        return false;

    if (const Shared<PartObject> ownerPart = dynamic_pointer_cast<PartObject>(ownerObject))
    {
        outWorldMatrix = ownerPart->Get_CombinedWorldMatrix();
        return true;
    }

    const Shared<TransformCom> ownerTransform = ownerObject->Get_Transform();
    if (ownerTransform == nullptr)
        return false;

    outWorldMatrix = ownerTransform->Get_WorldMatrix();
    return true;
}

bool Try_GetSocketWorldMatrix(
    const Shared<GameObject>& ownerObject,
    const wstring& partTag,
    const string& socketName,
    Matrix& outWorldMatrix)
{
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    if (const auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        outWorldMatrix = *socketMatrix * playablePart->Get_CombinedWorldMatrix();
        return true;
    }

    if (const auto* monsterPart = dynamic_cast<Monster_Part*>(partObject))
    {
        const Matrix* socketMatrix = monsterPart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        outWorldMatrix = *socketMatrix * monsterPart->Get_CombinedWorldMatrix();
        return true;
    }

    if (const auto* weaponPart = dynamic_cast<Weapon*>(partObject))
    {
        const Matrix* socketMatrix = weaponPart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        outWorldMatrix = *socketMatrix * weaponPart->Get_CombinedWorldMatrix();
        return true;
    }

    return false;
}

bool Try_GetPartRootWorldMatrix(
    const Shared<GameObject>& ownerObject,
    const wstring& partTag,
    Matrix& outWorldMatrix)
{
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    outWorldMatrix = partObject->Get_CombinedWorldMatrix();
    return true;
}

bool Try_ResolveNotifyBaseWorldMatrix(
    const Shared<GameObject>& ownerObject,
    EffectCom::EffectPlayTarget target,
    const wstring& partTag,
    const string& socketName,
    Matrix& outWorldMatrix)
{
    if (target == EffectCom::EffectPlayTarget::OwnerRoot)
        return Try_GetOwnerRootWorldMatrix(ownerObject, outWorldMatrix);

    if (target == EffectCom::EffectPlayTarget::PartSocket)
        return Try_GetSocketWorldMatrix(ownerObject, partTag, socketName, outWorldMatrix);

    return Try_GetPartRootWorldMatrix(ownerObject, partTag, outWorldMatrix);
}

const BattleChar* Resolve_CurrentBattleTarget(BattleController& battle)
{
    const vector<BattleChar>& currentStepTargets = battle.Get_TurnAction().GetCurStepTarget();
    for (const BattleChar& target : currentStepTargets)
    {
        if (target.actor != nullptr && !target.actor->Is_MarkDestroy())
            return &target;
    }

    const BattleChar& initTarget = battle.Get_TurnAction().GetInitTarget();
    if (initTarget.actor != nullptr && !initTarget.actor->Is_MarkDestroy())
        return &initTarget;

    return nullptr;
}

bool Try_ResolveCurrentBattleContext(
    const AnimNotifyContext& context,
    Shared<GameObject>& outNotifyOwnerRoot,
    Shared<BattleController>& outBattle)
{
    outNotifyOwnerRoot = nullptr;
    outBattle = nullptr;

    if (context.isPreview || context.owner == nullptr)
        return false;

    outNotifyOwnerRoot = Resolve_NotifyOwnerRoot(context.owner);
    if (outNotifyOwnerRoot == nullptr)
        return false;

    outBattle = CLIENT->Get_BattleController();
    if (outBattle == nullptr || !outBattle->Is_BattleStarted())
        return false;

    const BattleChar& currentActor = outBattle->Get_TurnAction().Get_CurChar();
    if (currentActor.actor == nullptr || currentActor.actor.get() != outNotifyOwnerRoot.get())
        return false;

    return true;
}

vector<BattleChar> Resolve_EffectBattleTargets(
    const AnimNotifyContext& context,
    bool includeDeadTargetsOnAllEnemies)
{
    Shared<GameObject> notifyOwnerRoot{};
    Shared<BattleController> battle{};
    if (!Try_ResolveCurrentBattleContext(context, notifyOwnerRoot, battle))
        return {};

    const BattleAction& turnAction = battle->Get_TurnAction();
    if (turnAction.GetCurStepIndex() >= turnAction.GetSteps().size())
        return {};

    const BattleAction::AttackStep& currentStep = turnAction.GetCurStep();
    const bool isAllEnemiesStep =
        currentStep.targetType == TargetType::AllEnemies ||
        currentStep.targetType == TargetType::AllEnemiesJump;

    if (isAllEnemiesStep)
    {
        if (includeDeadTargetsOnAllEnemies)
        {
            if (turnAction.Get_CurChar().statCom && turnAction.Get_CurChar().statCom->Is_Player())
                return battle->Get_BattleEnemies();

            return battle->Get_BattleHeroes();
        }

        return turnAction.GetCurStepTarget();
    }

    vector<BattleChar> resolvedTargets{};
    for (const BattleChar& target : turnAction.GetCurStepTarget())
    {
        if (target.actor != nullptr && !target.actor->Is_MarkDestroy())
            resolvedTargets.push_back(target);
    }

    if (!resolvedTargets.empty())
        return resolvedTargets;

    const BattleChar& initTarget = turnAction.GetInitTarget();
    if (initTarget.actor != nullptr && !initTarget.actor->Is_MarkDestroy())
        resolvedTargets.push_back(initTarget);

    return resolvedTargets;
}

float Resolve_YawDegFromDirection(const Vec3& direction, float fallbackYawDeg)
{
    Vec3 flattenedDirection = direction;
    flattenedDirection.y = 0.f;

    if (flattenedDirection.LengthSquared() <= 0.0001f)
        return fallbackYawDeg;

    flattenedDirection.Normalize();
    return XMConvertToDegrees(atan2f(flattenedDirection.x, flattenedDirection.z));
}

float Resolve_PitchDegFromDirection(const Vec3& direction, float fallbackPitchDeg)
{
    const float horizontalLength = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (horizontalLength <= 0.0001f && abs(direction.y) <= 0.0001f)
        return fallbackPitchDeg;

    return XMConvertToDegrees(atan2f(-direction.y, max(horizontalLength, 0.0001f)));
}

void Apply_BattleTargetFacingIfNeeded(
    const AnimNotifyContext& context,
    const EffectCom::EffectPlayDesc& sourceDesc,
    EffectCom::EffectPlayDesc& inOutDesc,
    bool usePitch,
    const BattleChar* explicitTarget = nullptr)
{
    Shared<GameObject> notifyOwnerRoot{};
    Shared<BattleController> battle{};
    if (!Try_ResolveCurrentBattleContext(context, notifyOwnerRoot, battle))
        return;

    const BattleChar* currentTarget = explicitTarget ? explicitTarget : Resolve_CurrentBattleTarget(*battle);
    if (currentTarget == nullptr || currentTarget->actor == nullptr || currentTarget->actor->Get_Transform() == nullptr)
        return;

    Matrix baseWorldMatrix = Matrix::Identity;
    if (!Try_ResolveNotifyBaseWorldMatrix(
        notifyOwnerRoot,
        sourceDesc.target,
        sourceDesc.partTag,
        sourceDesc.socketName,
        baseWorldMatrix))
    {
        return;
    }

    Matrix rotationBasisMatrix = baseWorldMatrix;
    if (!sourceDesc.transformInheritance.inheritRotation &&
        !Try_GetOwnerRootWorldMatrix(notifyOwnerRoot, rotationBasisMatrix))
    {
        return;
    }

    Vec3 baseScale = Vec3::One;
    Quat baseRotation = Quat::Identity;
    Vec3 baseTranslation = Vec3::Zero;
    if (!baseWorldMatrix.Decompose(baseScale, baseRotation, baseTranslation))
        return;

    const Vec3 basisLook = sourceDesc.transformInheritance.inheritRotation
                           ? Vec3(baseWorldMatrix._31, baseWorldMatrix._32, baseWorldMatrix._33)
                           : Vec3(rotationBasisMatrix._31, rotationBasisMatrix._32, rotationBasisMatrix._33);

    const Vec3 targetDirection = currentTarget->actor->Get_Transform()->Get_WorldPosition() - baseTranslation;
    const float basisYawDeg = Resolve_YawDegFromDirection(basisLook, 0.f);
    const float desiredYawDeg = Resolve_YawDegFromDirection(targetDirection, basisYawDeg);

    inOutDesc.localRotation.y += desiredYawDeg - basisYawDeg;

    if (!usePitch)
        return;

    const float basisPitchDeg = Resolve_PitchDegFromDirection(basisLook, 0.f);
    const float desiredPitchDeg = Resolve_PitchDegFromDirection(targetDirection, basisPitchDeg);
    inOutDesc.localRotation.x += desiredPitchDeg - basisPitchDeg;
}
}

IMPLEMENT_REFLECTION(AN_PlayEffectBattleDir)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Effect Name", _effectName);
    PROPERTY_ENUM("Target", _target, EffectCom::EffectPlayTarget);
    PROPERTY_WSTRING("Part Tag", _partTag);
    PROPERTY_STRING("Socket Name", _socketName);
    PROPERTY_WSTRING("Layer Tag", _layerTag);
    PROPERTY_VEC3("Local Offset", _localOffset);
    PROPERTY_VEC3("Local Rotation", _localRotation);
    PROPERTY_VEC3("Scale", _scale);
    PROPERTY_FLOAT("Playback Speed", _playbackSpeed, 0.f, 10.f, 0.01f);
    PROPERTY_BOOL("Inherit Position", _inheritPosition);
    PROPERTY_BOOL("Inherit Rotation", _inheritRotation);
    PROPERTY_BOOL("Inherit Scale", _inheritScale);
    PROPERTY_BOOL("Use Pitch", _usePitch);
    PROPERTY_BOOL("Use Battle Target Yaw Only", _useBattleTargetYawOnly);
    PROPERTY_INT("AllEnemies Repeat Count", _allEnemiesRepeatCount);
    PROPERTY_BOOL("Include Dead Targets On AllEnemies", _includeDeadTargetsOnAllEnemies);

    return true;
}

void AN_PlayEffectBattleDir::Execute(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner, _target);
    if (effectCom == nullptr)
        return;

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = _effectName;
    desc.target = _target;
    desc.partTag = _partTag;
    desc.socketName = _socketName;
    desc.layerTag = _layerTag;
    desc.localOffset = _localOffset;
    desc.localRotation = _localRotation;
    desc.scale = _scale;
    desc.playbackSpeed = _playbackSpeed;
    desc.transformInheritance.inheritPosition = _inheritPosition;
    desc.transformInheritance.inheritRotation = _inheritRotation;
    desc.transformInheritance.inheritScale = _inheritScale;
    desc.followTarget = false;

    EffectCom::EffectPlayDesc battleTargetDesc = desc;
    Apply_BattleTargetFacingIfNeeded(context, desc, battleTargetDesc, _usePitch);

    if (_useBattleTargetYawOnly)
    {
        if (_usePitch)
            desc.localRotation.x = battleTargetDesc.localRotation.x;

        desc.localRotation.y = battleTargetDesc.localRotation.y;
    }
    else
    {
        desc.localRotation = battleTargetDesc.localRotation;
    }

    if (context.isPreview)
    {
        EffectInstancePool::AttachedHandle handle{};
        effectCom->Play_Effect(desc, &handle);
        return;
    }

    const vector<BattleChar> effectTargets =
        Resolve_EffectBattleTargets(context, _includeDeadTargetsOnAllEnemies);

    if (effectTargets.empty())
    {
        effectCom->Play_Effect(desc);
        return;
    }

    const Shared<BattleController> battle = CLIENT->Get_BattleController();
    const bool isAllEnemiesStep =
        battle != nullptr &&
        battle->Get_TurnAction().GetCurStepIndex() < battle->Get_TurnAction().GetSteps().size() &&
        (battle->Get_TurnAction().GetCurStep().targetType == TargetType::AllEnemies ||
         battle->Get_TurnAction().GetCurStep().targetType == TargetType::AllEnemiesJump);

    const int32 repeatCount = isAllEnemiesStep ? max(_allEnemiesRepeatCount, 1) : 1;

    for (const BattleChar& target : effectTargets)
    {
        if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
            continue;

        EffectCom::EffectPlayDesc perTargetDesc = desc;
        EffectCom::EffectPlayDesc perTargetBattleDesc = desc;
        Apply_BattleTargetFacingIfNeeded(context, desc, perTargetBattleDesc, _usePitch, &target);

        if (_useBattleTargetYawOnly)
        {
            if (_usePitch)
                perTargetDesc.localRotation.x = perTargetBattleDesc.localRotation.x;

            perTargetDesc.localRotation.y = perTargetBattleDesc.localRotation.y;
        }
        else
        {
            perTargetDesc.localRotation = perTargetBattleDesc.localRotation;
        }

        for (int32 repeatIndex = 0; repeatIndex < repeatCount; ++repeatIndex)
            effectCom->Play_Effect(perTargetDesc);
    }
}
