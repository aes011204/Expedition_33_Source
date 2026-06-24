#include "ANS_SL_MoveTo.h"

#include "ClientInstance.h"
#include "GameObject.h"
#include "MovementCom.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"
#include "TransformCom.h"

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
    Shared<GameObject> target = Resolve_MoveTarget(context.owner);
    if (target == nullptr || target->Get_Transform() == nullptr)
        return;

    const Vec3 desiredWorldPos = Vec3::Lerp(_startPos, _goalPos, context.notifyStateProgress);
    Apply_Position(context, target, desiredWorldPos);
}

void ANS_SL_MoveTo::On_End(const AnimNotifyContext& context)
{
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

void ANS_SL_MoveTo::Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos) const
{
    const Shared<TransformCom> transform = target ? target->Get_Transform() : nullptr;
    if (transform == nullptr)
        return;

    if (!context.isPreview)
    {
        if (Shared<MovementCom> movement = target->Get_Component<MovementCom>())
        {
            Vec3 displacement = desiredWorldPos - transform->Get_WorldPosition();
            movement->Move_ExternalDisplacement(displacement, context.deltaTime);
            return;
        }
    }

    transform->Set_WorldPosition(desiredWorldPos);
}
