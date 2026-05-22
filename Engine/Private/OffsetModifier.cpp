#include "pch.h"
#include "OffsetModifier.h"

#include "Animation.h"
#include "Channel.h"
#include "ModelCom.h"
#include "TransformCom.h"

HRESULT OffsetModifier::Initialize(const OffsetModifierDesc& desc)
{
    _desc = desc;
    auto baseModel = desc.baseModelCom;
    if (nullptr == baseModel || _desc.endBoneName.empty() || _desc.rootBoneName.empty())
    {
        return E_FAIL;
    }

    _animIndex = baseModel->Get_AnimationIndexByName(desc.animName);
    if (_animIndex == UINT_MAX)
        return E_FAIL;

    int32 currentBoneIdx = baseModel->Get_BoneIndex(_desc.endBoneName);
    int32 rootBoneIdx = baseModel->Get_BoneIndex(_desc.rootBoneName);

    if (currentBoneIdx == -1 || rootBoneIdx == -1)
    {
        return E_FAIL;
    }

    int32 maxDepth = 100;
    int32 depthCount = 0;

    while (currentBoneIdx != -1 && depthCount < maxDepth)
    {
        _boneChain.push_back(currentBoneIdx);

        if (currentBoneIdx == rootBoneIdx)
        {
            break;
        }

        currentBoneIdx = baseModel->Get_BoneParentIndex(currentBoneIdx);
        ++depthCount;
    }

    if (_boneChain.empty() || _boneChain.back() != rootBoneIdx)
    {
        return E_FAIL;
    }

    return S_OK;
}

void OffsetModifier::Evaluate_Modifier(ModelCom* model, float timeDelta, float alpha)
{
    if (!_isActive || nullptr == model || alpha <= 0.f || _animIndex < 0 || _boneChain.empty())
    {
        return;
    }

    float clampedPitch = clamp(_targetPitch, -_desc.maxPitchAngle, _desc.maxPitchAngle);

    float weightUp = 0.f;
    float weightDown = 0.f;
    float weightCenter = 1.f;

    if (clampedPitch > FLT_EPSILON)
    {
        weightUp = clampedPitch / _desc.maxPitchAngle;
        weightCenter = 1.f - weightUp;
    }
    else if (clampedPitch < -FLT_EPSILON)
    {
        weightDown = abs(clampedPitch) / _desc.maxPitchAngle;
        weightCenter = 1.f - weightDown;
    }

    Animation* offsetAnim = model->Get_Animation(_animIndex);
    if (nullptr == offsetAnim) return;

    for (int32 boneIdx : _boneChain)
    {
        const auto& channels = offsetAnim->Get_Channels();
        Shared<Channel> boneChannel = nullptr;
        for (const auto& channel : channels)
        {
            if (channel->Get_BoneIndex() == boneIdx)
            {
                boneChannel = channel;
                break;
            }
        }

        if (!boneChannel)
        {
            continue; 
        }

        auto Get_TransformAtRatio = [&](float ratio) -> RootMotionFrame
        {
            RootMotionFrame outTransform;
            uint32 keyFrameIndex = 0;
            float targetTrackPosition = offsetAnim->Get_Duration() * ratio;
            boneChannel->Get_ChannelTransform(targetTrackPosition, keyFrameIndex, offsetAnim->Get_Duration(), true, outTransform);
            return outTransform;
        };

        RootMotionFrame centerPose = Get_TransformAtRatio(_desc.TimeRatio_Center);
        RootMotionFrame targetPose;

        if (weightUp > 0.f)
        {
            RootMotionFrame upPose = Get_TransformAtRatio(_desc.TimeRatio_Up);

            targetPose.rotation = Quaternion::Slerp(centerPose.rotation, upPose.rotation, weightUp);
            targetPose.position = Vector3::Lerp(centerPose.position, upPose.position, weightUp);
            targetPose.scale = Vector3::Lerp(centerPose.scale, upPose.scale, weightUp);
        }
        else if (weightDown > 0.f)
        {
            RootMotionFrame downPose = Get_TransformAtRatio(_desc.TimeRatio_Down);

            targetPose.rotation = Quaternion::Slerp(centerPose.rotation, downPose.rotation, weightDown);
            targetPose.position = Vector3::Lerp(centerPose.position, downPose.position, weightDown);
            targetPose.scale = Vector3::Lerp(centerPose.scale, downPose.scale, weightDown);
        }
        else
        {
            targetPose = centerPose;
        }

        Matrix localMat = Matrix::CreateScale(targetPose.scale)
            * Matrix::CreateFromQuaternion(targetPose.rotation)
            * Matrix::CreateTranslation(targetPose.position);

        model->Set_BoneLocalMatrix(boneIdx, localMat);
    }
}

Shared<OffsetModifier> OffsetModifier::Create(const OffsetModifierDesc& modifierDesc)
{
    auto instance = make_shared<OffsetModifier>();

    CHECK_FAILED(instance->Initialize(modifierDesc), nullptr);

    return instance;
}


