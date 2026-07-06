#include "ANS_SL_DodgeMove.h"

#include "GameObject.h"
#include "MovementCom.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(ANS_SL_DodgeMove)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Distance", _distance, -10.f, 10.f, 0.05f);
    PROPERTY_VEC2("Move Axis", _moveAxis, 0.05f);

    return true;
}

void ANS_SL_DodgeMove::On_Begin(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    Shared<GameObject> target = Resolve_MoveTarget(context.owner);
    if (target == nullptr || target->Get_Transform() == nullptr)
        return;

    const Shared<TransformCom> transform = target->Get_Transform();
    const Vec3 direction = Resolve_WorldDirection(transform);

    _startPos = transform->Get_WorldPosition();
    _goalPos = _startPos + direction * _distance;
}

void ANS_SL_DodgeMove::On_Tick(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    Shared<GameObject> target = Resolve_MoveTarget(context.owner);
    if (target == nullptr || target->Get_Transform() == nullptr)
        return;

    const Vec3 desiredWorldPos = Vec3::Lerp(_startPos, _goalPos, context.notifyStateProgress);
    Apply_Position(context, target, desiredWorldPos);
}

void ANS_SL_DodgeMove::On_End(const AnimNotifyContext& context)
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

Shared<GameObject> ANS_SL_DodgeMove::Resolve_MoveTarget(GameObject* notifyOwner) const
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

Vec3 ANS_SL_DodgeMove::Resolve_WorldDirection(const Shared<TransformCom>& transform) const
{
    Vec2 moveAxis = _moveAxis;
    if (moveAxis.LengthSquared() <= 0.0001f)
        moveAxis = Vec2(0.f, 1.f);

    moveAxis.Normalize();

    Vec3 right = transform->Get_State(State::Right);
    Vec3 look = transform->Get_State(State::Look);
    right.y = 0.f;
    look.y = 0.f;

    if (right.LengthSquared() > 0.0001f)
        right.Normalize();
    else
        right = Vec3(1.f, 0.f, 0.f);

    if (look.LengthSquared() > 0.0001f)
        look.Normalize();
    else
        look = Vec3(0.f, 0.f, 1.f);

    Vec3 worldDir = right * moveAxis.x + look * moveAxis.y;
    if (worldDir.LengthSquared() <= 0.0001f)
        worldDir = look;

    worldDir.Normalize();
    return worldDir;
}

void ANS_SL_DodgeMove::Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos) const
{
    const Shared<TransformCom> transform = target ? target->Get_Transform() : nullptr;
    if (transform == nullptr)
        return;

    if (!context.isPreview)
    {
        if (Shared<MovementCom> movement = target->Get_Component<MovementCom>())
        {
            Vec3 displacement = desiredWorldPos - transform->Get_WorldPosition();
            displacement.y = 0.f;
            movement->Move_ExternalDisplacement(displacement, context.deltaTime);
            return;
        }
    }

    transform->Set_WorldPosition(desiredWorldPos);
}
