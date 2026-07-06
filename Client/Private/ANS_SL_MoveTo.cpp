#include "ANS_SL_MoveTo.h"

#include "ClientInstance.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "MovementCom.h"
#include "PhysX_Types.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"
#include "TransformCom.h"

namespace
{
    Shared<ColliderCom> Resolve_MovementCollider(const Shared<GameObject>& target)
    {
        if (target == nullptr)
            return nullptr;

        if (const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(target))
        {
            if (const Shared<ColliderCom> bodyCollider = dualliste->Get_Component<ColliderCom>(TEXT("Com_SL_MonsterBody")))
                return bodyCollider;
        }

        return target->Get_Component<ColliderCom>();
    }
}

REGISTER_GLOBAL_ENUM(Client::ANS_SL_MoveTo::MoveMode, "SLMoveToMode")

IMPLEMENT_REFLECTION(ANS_SL_MoveTo)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("Move Mode", _moveMode, MoveMode);
    PROPERTY_FLOAT("Forward Offset", _forwardOffset, -20.f, 20.f, 0.05f);
    PROPERTY_FLOAT("Distance", _distance, 0.f, 20.f, 0.05f);

    return true;
}

void ANS_SL_MoveTo::On_Begin(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    _goalClampedByPlayer = false;

    Shared<GameObject> target = Resolve_MoveTarget(context.owner);
    if (target == nullptr || target->Get_Transform() == nullptr)
        return;

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
    if (player == nullptr || player->Get_Transform() == nullptr)
        return;

    _startPos = target->Get_Transform()->Get_WorldPosition();
    _goalPos = Resolve_GoalPosition(target, player);
}

void ANS_SL_MoveTo::On_Tick(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    Shared<GameObject> target = Resolve_MoveTarget(context.owner);
    if (target == nullptr || target->Get_Transform() == nullptr)
        return;

    const Vec3 desiredWorldPos = _goalClampedByPlayer
        ? _goalPos
        : Vec3::Lerp(_startPos, _goalPos, context.notifyStateProgress);
    Apply_Position(context, target, desiredWorldPos);
}

void ANS_SL_MoveTo::On_End(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    Shared<GameObject> target = Resolve_MoveTarget(context.owner);
    if (target == nullptr || target->Get_Transform() == nullptr)
        return;

    Apply_Position(context, target, _goalPos);

    if (!context.isPreview)
    {
        if (Shared<MovementCom> movement = target->Get_Component<MovementCom>())
            movement->Stop_HorizontalMovement();
    }
}

Shared<GameObject> ANS_SL_MoveTo::Resolve_MoveTarget(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (current->Get_Component<MovementCom>())
            return current;

        if (current->Get_Owner() == nullptr)
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

Vec3 ANS_SL_MoveTo::Resolve_GoalPosition(const Shared<GameObject>& moveTarget, const Shared<GameObject>& target) const
{
    if (moveTarget == nullptr || target == nullptr ||
        moveTarget->Get_Transform() == nullptr || target->Get_Transform() == nullptr)
    {
        return _goalPos;
    }

    const Shared<TransformCom> moveTransform = moveTarget->Get_Transform();
    const Shared<TransformCom> targetTransform = target->Get_Transform();

    const Vec3 movePos = moveTransform->Get_WorldPosition();
    const Vec3 targetPos = targetTransform->Get_WorldPosition();

    switch (_moveMode)
    {
    case MoveMode::ForwardDistance:
    {
        Vec3 forward = moveTransform->Get_WorldForward();
        forward.y = 0.f;

        if (forward.LengthSquared() > 0.0001f)
            forward.Normalize();
        else
            forward = Vec3(0.f, 0.f, 1.f);

        Vec3 goalPos = movePos + forward * _distance;
        goalPos.y = movePos.y;
        return goalPos;
    }

    case MoveMode::BackwardDistance:
    {
        Vec3 backward = -moveTransform->Get_WorldForward();
        backward.y = 0.f;

        if (backward.LengthSquared() > 0.0001f)
            backward.Normalize();
        else
            backward = Vec3(0.f, 0.f, -1.f);

        Vec3 goalPos = movePos + backward * _distance;
        goalPos.y = movePos.y;
        return goalPos;
    }

    case MoveMode::TargetForwardOffset:
    default:
    {
        Vec3 offsetDir = movePos - targetPos;
        offsetDir.y = 0.f;

        if (offsetDir.LengthSquared() > 0.0001f)
            offsetDir.Normalize();
        else
            offsetDir = Vec3(0.f, 0.f, 1.f);

        Vec3 goalPos = targetPos + offsetDir * _forwardOffset;
        goalPos.y = movePos.y;
        return goalPos;
    }
    }
}

void ANS_SL_MoveTo::Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos)
{
    if (context.isCinematic)
        return;

    const Shared<TransformCom> transform = target ? target->Get_Transform() : nullptr;
    if (transform == nullptr)
        return;

    if (!context.isPreview)
    {
        if (Shared<MovementCom> movement = target->Get_Component<MovementCom>())
        {
            const Vec3 currentWorldPos = transform->Get_WorldPosition();
            Vec3 displacement = desiredWorldPos - currentWorldPos;
            const Shared<ColliderCom> collider = Resolve_MovementCollider(target);
            bool clampedByPlayer = false;
            bool hitPlayerBody = false;

            if (collider)
            {
                Vec3 sweepDir = displacement;
                const float sweepDistance = sweepDir.Length();

                if (sweepDistance > 0.0001f)
                {
                    sweepDir /= sweepDistance;

                    PhysXQueryHit hit{};
                    if (GAME->Sweep_Collider(
                        collider.get(),
                        transform->Get_WorldMatrix(),
                        sweepDir,
                        sweepDistance,
                        hit) &&
                        hit.collider != nullptr &&
                        hit.collider->Get_CollisionPreset() == Collision_Preset::SL_PlayerBody)
                    {
                        // 플레이어 body를 감싸는 경우만 MoveTo를 줄이고, 턱/환경 hit는 기존 이동계가 처리하게 둔다.
                        const float skinWidth = max(0.f, movement->Get_MoveDesc().sweep.skinWidth);
                        const float allowedDistance = max(0.f, hit.distance - skinWidth);

                        displacement = sweepDir * allowedDistance;
                        hitPlayerBody = true;
                    }
                }
            }

            const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
            const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
            const Shared<TransformCom> playerTransform = player ? player->Get_Transform() : nullptr;
            if (playerTransform != nullptr && collider != nullptr)
            {
                float playerRadius = 0.35f;
                if (Shared<ColliderCom> playerCollider = player->Get_Component<ColliderCom>();
                    playerCollider != nullptr &&
                    playerCollider->Get_CollisionPreset() == Collision_Preset::SL_PlayerBody)
                {
                    const ColliderShapeDesc& playerShape = playerCollider->Get_ShapeDesc();
                    playerRadius = max(playerShape.extents.x, playerShape.extents.z);
                }

                const ColliderShapeDesc& targetShape = collider->Get_ShapeDesc();
                const float targetRadius = max(targetShape.extents.x, targetShape.extents.z);
                const float minSeparation =
                    targetRadius + playerRadius + max(0.f, movement->Get_MoveDesc().sweep.skinWidth);

                const Vec3 playerPos = playerTransform->Get_WorldPosition();
                const Vec3 candidatePos = currentWorldPos + displacement;

                Vec3 currentFromPlayer = currentWorldPos - playerPos;
                currentFromPlayer.y = 0.f;

                Vec3 fromPlayer = candidatePos - playerPos;
                fromPlayer.y = 0.f;

                const float currentPlayerDistance = currentFromPlayer.Length();
                const float candidatePlayerDistance = fromPlayer.Length();

                if (hitPlayerBody && candidatePlayerDistance <= currentPlayerDistance + 0.0001f)
                    clampedByPlayer = true;

                if (fromPlayer.LengthSquared() <= 0.0001f)
                {
                    fromPlayer = currentWorldPos - playerPos;
                    fromPlayer.y = 0.f;
                }

                if (fromPlayer.LengthSquared() <= 0.0001f)
                {
                    fromPlayer = -displacement;
                    fromPlayer.y = 0.f;
                }

                if (fromPlayer.LengthSquared() <= 0.0001f)
                    fromPlayer = Vec3(0.f, 0.f, 1.f);

                const float playerDistance = fromPlayer.Length();
                if (playerDistance < minSeparation)
                {
                    fromPlayer /= playerDistance;

                    // 환경 hit가 먼저 잡혀도 최종 MoveTo 위치는 플레이어 body 반경 안으로 들어가지 않게 한다.
                    Vec3 clampedPos = playerPos + fromPlayer * minSeparation;
                    clampedPos.y = candidatePos.y;

                    displacement = clampedPos - currentWorldPos;
                    if (candidatePlayerDistance <= currentPlayerDistance + 0.0001f)
                        clampedByPlayer = true;
                }
            }
            else if (hitPlayerBody)
            {
                clampedByPlayer = true;
            }

            if (clampedByPlayer)
            {
                // 한 번 플레이어 body 앞에서 막힌 MoveTo는 원래 goal로 다시 밀어붙이지 않는다.
                _goalPos = currentWorldPos + displacement;
                _goalClampedByPlayer = true;
            }

            if (displacement.LengthSquared() <= 0.000001f)
                return;

            movement->Move_ExternalDisplacement(displacement, context.deltaTime);
            return;
        }
    }

    transform->Set_WorldPosition(desiredWorldPos);
}
