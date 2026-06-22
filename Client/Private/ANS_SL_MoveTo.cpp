#include "ANS_SL_MoveTo.h"

#include "ClientInstance.h"
#include "GameObject.h"
#include "MovementCom.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(ANS_SL_MoveTo)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Forward Offset", _forwardOffset, -20.f, 20.f, 0.05f);

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

    const Shared<TransformCom> playerTransform = player->Get_Transform();
    Vec3 playerForward = playerTransform->Get_WorldForward();
    playerForward.y = 0.f;

    if (playerForward.LengthSquared() > 0.0001f)
        playerForward.Normalize();
    else
        playerForward = Vec3(0.f, 0.f, 1.f);

    _goalPos = playerTransform->Get_WorldPosition() + playerForward * _forwardOffset;
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
