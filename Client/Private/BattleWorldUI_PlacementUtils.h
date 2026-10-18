#pragma once

#include "Camera.h"
#include "GameInstance.h"
#include "TransformCom.h"

NS_BEGIN(Client)

namespace BattleWorldUIPlacement
{
    inline Vec3 Resolve_CameraHorizontalPlacementDelta(const Vec3& offset)
    {
        const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
        if (activeCamera == nullptr || activeCamera->Get_Transform() == nullptr)
            return offset;

        const Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();

        Vec3 right = cameraTransform->Get_WorldRight();
        right.y = 0.f;
        if (right.LengthSquared() <= 0.0001f)
            right = Vec3::Right;
        else
            right.Normalize();

        Vec3 forward = cameraTransform->Get_WorldForward();
        forward.y = 0.f;
        if (forward.LengthSquared() <= 0.0001f)
            forward = Vec3::Forward;
        else
            forward.Normalize();

        return
            right * offset.x +
            Vec3::Up * offset.y +
            forward * offset.z;
    }
}

NS_END
