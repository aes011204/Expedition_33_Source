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

inline bool Evaluate_BattleTurnCurrentPresetPose(
    const Vec3& targetPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float yawBase,
    float pitchBase,
    const Vec3* smoothedTargetPos,
    float clipTime,
    BattleTurnCameraEvaluatedPose& outPose)
{
    const float finalFovY = clamp(preset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = preset.distance + clipSample.distanceOffset;
    const float finalSideOffset = preset.sideOffset + clipSample.sideOffset;
    const float finalPivotHeight = preset.pivotHeight + clipSample.pivotOffset;
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
    const Vec3 framingPos = pivotPos + rightDir * preset.aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * preset.heightOffset + rightDir * finalSideOffset;
    Vec3 lookTarget = framingPos + Vec3::Up * preset.lookHeight;

    Vec3 cameraForward = lookTarget - desiredPos;
    cameraForward.Normalize();

    Vec3 cameraRight = Vec3::Up.Cross(cameraForward);
    cameraRight.Normalize();

    Vec3 cameraUp = cameraForward.Cross(cameraRight);
    cameraUp.Normalize();

    desiredPos += Sample_BattleCameraShakeOffset(clipSample.shakeAmplitude, clipTime, cameraRight, cameraUp);

    outPose.cameraPos = desiredPos;
    outPose.lookTarget = lookTarget;
    outPose.fovY = finalFovY;
    outPose.rollOffset = preset.rollOffset;
    return true;
}

inline bool Evaluate_BattleTurnEnemyAttackPose(
    const Vec3& subjectPos,
    const Vec3& opponentPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float clipTime,
    BattleTurnCameraEvaluatedPose& outPose)
{
    const float finalFovY = clamp(preset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = preset.distance + clipSample.distanceOffset;
    const float finalSideOffset = preset.sideOffset + clipSample.sideOffset;
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

    Vec3 cameraPos = opponentPos + yawAdjustedBack * finalDistance + right * finalSideOffset + Vec3::Up * preset.heightOffset;
    const Vec3 lookTarget = subjectPos + forward * preset.aimOffset + Vec3::Up * preset.lookHeight;

    Vec3 cameraForward = lookTarget - cameraPos;
    cameraForward.Normalize();

    Vec3 cameraRight = Vec3::Up.Cross(cameraForward);
    cameraRight.Normalize();

    Vec3 cameraUp = cameraForward.Cross(cameraRight);
    cameraUp.Normalize();

    cameraPos += Sample_BattleCameraShakeOffset(clipSample.shakeAmplitude, clipTime, cameraRight, cameraUp);

    outPose.cameraPos = cameraPos;
    outPose.lookTarget = lookTarget;
    outPose.fovY = finalFovY;
    outPose.rollOffset = preset.rollOffset;
    return true;
}

NS_END
