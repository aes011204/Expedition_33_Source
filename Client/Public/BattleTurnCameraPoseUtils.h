#pragma once

#include "BattleCameraClipTypes.h"
#include "BattleCameraClipUtils.h"
#include "CameraPresetTypes.h"

NS_BEGIN(Client)

struct BattleTurnCameraEvaluatedPose
{
    Vec3 cameraPos = Vec3::Zero;
    Vec3 lookTarget = Vec3::Zero;
    float fovY = XMConvertToRadians(45.f);
    float rollOffset = 0.f;
};

inline bool Evaluate_BattleTurnWidePose(
    const Vec3& heroCenter,
    float yawDegrees,
    float fovY,
    float backOffset,
    float heightOffset,
    float lookHeightOffset,
    BattleTurnCameraEvaluatedPose& outPose)
{
    const float cameraYawRad = XMConvertToRadians(yawDegrees);

    Vec3 forwardDir{};
    forwardDir.x = sinf(cameraYawRad);
    forwardDir.y = 0.f;
    forwardDir.z = cosf(cameraYawRad);
    if (forwardDir.LengthSquared() <= 0.0001f)
        forwardDir = Vec3::Look;
    else
        forwardDir.Normalize();

    const Vec3 backDir = -forwardDir;

    Vec3 rightDir{};
    rightDir.x = cosf(cameraYawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(cameraYawRad);
    if (rightDir.LengthSquared() <= 0.0001f)
        rightDir = Vec3::Right;
    else
        rightDir.Normalize();

    outPose.cameraPos =
        heroCenter +
        backDir * (8.5f + backOffset) +
        rightDir +
        Vec3(0.f, 3.f + heightOffset, 2.f);
    outPose.lookTarget = heroCenter + Vec3(0.f, 2.2f + lookHeightOffset, 0.f);
    outPose.fovY = fovY;
    outPose.rollOffset = 0.f;
    return true;
}

inline bool Evaluate_BattleTurnPresetPose(
    const Vec3& targetPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float yawBase,
    float pitchBase,
    const Vec3* smoothedTargetPos,
    float /*clipTime*/,
    BattleTurnCameraEvaluatedPose& outPose)
{
    const float finalFovY = clamp(preset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = preset.distance + clipSample.distanceOffset;
    const float finalSideOffset = preset.sideOffset + clipSample.sideOffset;
    const float finalPivotHeight = preset.pivotHeight + clipSample.pivotOffset;
    const float finalHeightOffset = preset.heightOffset + clipSample.heightOffset;
    const float finalAimOffset = preset.aimOffset + clipSample.aimOffset;
    const float finalLookHeight = preset.lookHeight + clipSample.lookHeightOffset;
    const float finalYaw = yawBase + preset.yawOffset + clipSample.yawOffset;
    const float finalPitch = pitchBase + preset.pitchOffset;

    const float yawRad = XMConvertToRadians(finalYaw);
    const float pitchRad = XMConvertToRadians(finalPitch);

    Vec3 backDir;
    backDir.x = -sinf(yawRad);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRad);
    backDir.Normalize();

    Vec3 rightDir;
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * finalDistance;
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);
    const Vec3 pivotAnchor = smoothedTargetPos ? *smoothedTargetPos : targetPos;
    const Vec3 pivotPos = pivotAnchor + Vec3::Up * finalPivotHeight;
    const Vec3 framingPos = pivotPos + rightDir * finalAimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * finalHeightOffset + rightDir * finalSideOffset;
    Vec3 lookTarget = framingPos + Vec3::Up * finalLookHeight;

    if (fabsf(clipSample.orbitYaw) > 0.0001f ||
        fabsf(clipSample.orbitRadius) > 0.0001f ||
        fabsf(clipSample.orbitHeight) > 0.0001f ||
        fabsf(clipSample.orbitLookHeight) > 0.0001f)
    {
        Vec3 orbitVector = desiredPos - pivotPos;
        const float baseHeight = orbitVector.y;
        orbitVector.y = 0.f;

        if (orbitVector.LengthSquared() > 0.0001f)
        {
            const Matrix orbitRotation = XMMatrixRotationY(XMConvertToRadians(clipSample.orbitYaw));
            Vec3 rotatedOrbit = XMVector3TransformNormal(orbitVector, orbitRotation);
            rotatedOrbit.y = 0.f;

            const float baseRadius = orbitVector.Length();
            if (rotatedOrbit.LengthSquared() > 0.0001f)
            {
                rotatedOrbit.Normalize();
                rotatedOrbit *= std::max(0.05f, baseRadius + clipSample.orbitRadius);
            }

            desiredPos = pivotPos + rotatedOrbit + Vec3::Up * (baseHeight + clipSample.orbitHeight);
        }
        else
        {
            desiredPos += Vec3::Up * clipSample.orbitHeight;
        }

        lookTarget += Vec3::Up * clipSample.orbitLookHeight;
    }

    outPose.cameraPos = desiredPos;
    outPose.lookTarget = lookTarget;
    outPose.fovY = finalFovY;
    outPose.rollOffset = preset.rollOffset;
    return true;
}

inline bool Evaluate_BattleTurnPlayerAttackRelationPose (
 const Vec3& subjectPos,
 const Vec3& opponentPos,
 const BattleCameraPreset& preset,
 const BattleCameraClipSample& clipSample,
 float /*clipTime*/,
 BattleTurnCameraEvaluatedPose& outPose)
{
    const float finalFovY = clamp(preset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = preset.distance + clipSample.distanceOffset;
    const float finalSideOffset = preset.sideOffset + clipSample.sideOffset;
    const float finalHeightOffset = preset.heightOffset + clipSample.heightOffset;
    const float finalAimOffset = preset.aimOffset + clipSample.aimOffset;
    const float finalLookHeight = preset.lookHeight + clipSample.lookHeightOffset;
    const float finalYawOffset = preset.yawOffset + clipSample.yawOffset;

   Vec3 attackDir = opponentPos - subjectPos;
   attackDir.y = 0.f;
   if(attackDir.LengthSquared() <= 0.0001f)
       return false;

    attackDir.Normalize();

    Vec3 right;
    right.x = attackDir.z;
    right.y = 0.f;
    right.z = -attackDir.x;
    right.Normalize();

    const Vec3 backDir = -attackDir;
    const float yawRad = XMConvertToRadians(finalYawOffset);
    const Vec3 yawAdjustedBack = backDir * cosf(yawRad) + right * sinf(yawRad);

    Vec3 anchorPos = Vec3::Lerp(subjectPos, opponentPos, 0.25f);
    Vec3 cameraPos = anchorPos + yawAdjustedBack * finalDistance + right * finalSideOffset + Vec3::Up * finalHeightOffset;
    const Vec3 lookTarget = subjectPos + attackDir * finalAimOffset + Vec3::Up * finalLookHeight;

    outPose.cameraPos = cameraPos;
    outPose.lookTarget = lookTarget;
    outPose.fovY = finalFovY;
    outPose.rollOffset = preset.rollOffset;
    return true;
}

inline bool Evaluate_BattleTurnEnemyAttackPose (
    const Vec3& subjectPos,
    const Vec3& opponentPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float /*clipTime*/,
    BattleTurnCameraEvaluatedPose& outPose)
{
    const float finalFovY = clamp(preset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = preset.distance + clipSample.distanceOffset;
    const float finalSideOffset = preset.sideOffset + clipSample.sideOffset;
    const float finalHeightOffset = preset.heightOffset + clipSample.heightOffset;
    const float finalAimOffset = preset.aimOffset + clipSample.aimOffset;
    const float finalLookHeight = preset.lookHeight + clipSample.lookHeightOffset;
    const float finalYawOffset = preset.yawOffset + clipSample.yawOffset;

    Vec3 forward = opponentPos - subjectPos;
    forward.y = 0.f;
    if (forward.LengthSquared() <= 0.0001f)
        return false;

    forward.Normalize();

    Vec3 right;
    right.x = forward.z;
    right.y = 0.f;
    right.z = -forward.x;
    right.Normalize();

    const Vec3 backDir = -forward;
    const float yawRad = XMConvertToRadians(finalYawOffset);
    const Vec3 yawAdjustedBack = backDir * cosf(yawRad) + right * sinf(yawRad);

    Vec3 cameraPos = opponentPos + yawAdjustedBack * finalDistance + right * finalSideOffset + Vec3::Up * finalHeightOffset;
    const Vec3 lookTarget = subjectPos + forward * finalAimOffset + Vec3::Up * finalLookHeight;

    outPose.cameraPos = cameraPos;
    outPose.lookTarget = lookTarget;
    outPose.fovY = finalFovY;
    outPose.rollOffset = preset.rollOffset;
    return true;
}

inline bool Evaluate_BattleTurnPlayerAttackBattlePlanePose(
    const Vec3& subjectPos,
    const Vec3& opponentPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float yawBase,
    float /*clipTime*/,
    BattleTurnCameraEvaluatedPose& outPose)
{
    const float finalFovY = clamp(preset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = preset.distance + clipSample.distanceOffset;
    const float finalSideOffset = preset.sideOffset + clipSample.sideOffset;
    const float finalHeightOffset = preset.heightOffset + clipSample.heightOffset;
    const float finalAimOffset = preset.aimOffset + clipSample.aimOffset;
    const float finalLookHeight = preset.lookHeight + clipSample.lookHeightOffset;
    const float finalYaw = yawBase + preset.yawOffset + clipSample.yawOffset;

    Vec3 attackDir = opponentPos - subjectPos;
    attackDir.y = 0.f;
    if (attackDir.LengthSquared() <= 0.0001f)
        return false;

    attackDir.Normalize();

    const float yawRad = XMConvertToRadians(finalYaw);

    Vec3 forwardDir;
    forwardDir.x = sinf(yawRad);
    forwardDir.y = 0.f;
    forwardDir.z = cosf(yawRad);
    forwardDir.Normalize();

    const Vec3 backDir = -forwardDir;

    Vec3 rightDir;
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    Vec3 anchorPos = Vec3::Lerp(subjectPos, opponentPos, 0.25f);
    Vec3 cameraPos = anchorPos + backDir * finalDistance + rightDir * finalSideOffset + Vec3::Up * finalHeightOffset;
    const Vec3 lookTarget = subjectPos + attackDir * finalAimOffset + Vec3::Up * finalLookHeight;

    outPose.cameraPos = cameraPos;
    outPose.lookTarget = lookTarget;
    outPose.fovY = finalFovY;
    outPose.rollOffset = preset.rollOffset;
    return true;
}

NS_END
