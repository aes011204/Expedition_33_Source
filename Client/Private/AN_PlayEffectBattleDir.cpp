#include "AN_PlayEffectBattleDir.h"

#include "BattleController.h"
#include "ClientInstance.h"
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
    bool usePitch)
{
    if (context.isPreview || context.owner == nullptr)
        return;

    const Shared<GameObject> notifyOwnerRoot = Resolve_NotifyOwnerRoot(context.owner);
    if (notifyOwnerRoot == nullptr)
        return;

    const Shared<BattleController> battle = CLIENT->Get_BattleController();
    if (battle == nullptr || !battle->Is_BattleStarted())
        return;

    const BattleChar& currentActor = battle->Get_TurnAction().Get_CurChar();
    if (currentActor.actor == nullptr || currentActor.actor.get() != notifyOwnerRoot.get())
        return;

    const BattleChar* currentTarget = Resolve_CurrentBattleTarget(*battle);
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

    PROPERTY_WSTRING("이펙트 이름", _effectName);
    PROPERTY_ENUM("Target", _target, EffectCom::EffectPlayTarget);
    PROPERTY_WSTRING("파츠 이름", _partTag);
    PROPERTY_STRING("소켓 이름", _socketName);
    PROPERTY_WSTRING("레이어 태그", _layerTag);
    PROPERTY_VEC3("로컬 오프셋", _localOffset);
    PROPERTY_VEC3("로컬 회전", _localRotation);
    PROPERTY_VEC3("스케일", _scale);
    PROPERTY_FLOAT("이펙트 재생 속도", _playbackSpeed, 0.f, 10.f, 0.01f);
    PROPERTY_BOOL("위치 상속", _inheritPosition);
    PROPERTY_BOOL("회전 상속", _inheritRotation);
    PROPERTY_BOOL("스케일 상속", _inheritScale);
    PROPERTY_BOOL("Use Pitch", _usePitch);
    PROPERTY_BOOL("Yaw 만 적용", _useBattleTargetYawOnly);

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

    effectCom->Play_Effect(desc);
}
