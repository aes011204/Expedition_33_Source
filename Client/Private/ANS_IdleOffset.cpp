#include "ANS_IdleOffset.h"

#include <GameObject.h>
#include "ModelCom.h"
#include "TransformCom.h"

void ANS_IdleOffset::On_Begin(const AnimNotifyContext& context)
{
    _initialized = Try_GetPelvisWorldPosition(context, _prevPelvisWorld);
}

void ANS_IdleOffset::On_Tick(const AnimNotifyContext& context)
{
    if (!_initialized || !context.owner)
        return;

    Vec3 currentPelvisWorld{};
    if (!Try_GetPelvisWorldPosition(context, currentPelvisWorld))
        return;

    Vec3 delta = _prevPelvisWorld - currentPelvisWorld;
    delta.y = 0.f; // 전환 체감만 보정하고 높이는 건드리지 않는다.

    if (delta.LengthSquared() > 0.000001f)
    {
        auto transform = context.owner->Get_Transform();
        transform->Set_Position(transform->Get_Position() + delta);
    }

    // 이번 틱 보정 후 pelvis 기준점을 다음 틱 기준으로 갱신한다.
    _prevPelvisWorld = currentPelvisWorld + delta;
}

void ANS_IdleOffset::On_End(const AnimNotifyContext& context)
{
    _initialized = false;
    _prevPelvisWorld = {};
}

bool ANS_IdleOffset::Try_GetPelvisWorldPosition(const AnimNotifyContext& context, Vec3& outWorldPos)
{
    if (!context.owner || !context.model)
        return false;

    const Matrix* pelvisMatrix = context.model->Get_BoneMatrixPtr("pelvis");
    if (!pelvisMatrix)
        return false;

    const Matrix actorWorld = context.owner->Get_Transform()->Get_WorldMatrix();
    outWorldPos = Vec3::Transform(Vec3::Zero, (*pelvisMatrix) * actorWorld);
    return true;
}
