#include "pch.h"
#include "OffsetAnimationModifier.h"

#include "Animation.h"
#include "Channel.h"
#include "ModelCom.h"

HRESULT OffsetAnimationModifier::Initialize(AnimationModifierDesc* desc)
{
    CHECK_FAILED(__super::Initialize(desc), E_FAIL);
    _desc = *static_cast<OffsetModifierDesc*>(desc);
    auto baseModel = _desc.baseModelCom;
    if (nullptr == baseModel || _desc.offsetBoneRanges.empty())
    {
        return E_FAIL;
    }

    _animIndex = baseModel->Get_AnimationIndexByName(_desc.animName);
    if (_animIndex == UINT_MAX)
    {
        return E_FAIL;
    }

    for (auto& boneName : _desc.translationOnlyBones)
    {
        int32 boneIndex = baseModel->Get_BoneIndex(boneName);
        if (boneIndex != -1)
            _translationOnlyBones.insert(boneIndex);
    }

    unordered_set<int32> uniqueBones;
    for (const string& boneName : _desc.translationOnlyBones)
    {
        int32 idx = baseModel->Get_BoneIndex(boneName);
        if (idx != -1)
            _translationOnlyBones.insert(idx);
    }

    for (const auto& boneRange : _desc.offsetBoneRanges)
    {
        int32 rootBoneIdx = baseModel->Get_BoneIndex(boneRange.rootBoneName);
        int32 endBoneIdx = baseModel->Get_BoneIndex(boneRange.endBoneName);

        if (rootBoneIdx == -1 || endBoneIdx == -1)
            continue;

        int32 currentBoneIdx = endBoneIdx;
        int32 depthCount = 0;

        while (currentBoneIdx != -1 && depthCount++ < 100)
        {
            uniqueBones.insert(currentBoneIdx);

            if (currentBoneIdx == rootBoneIdx)
                break;

            currentBoneIdx = baseModel->Get_BoneParentIndex(currentBoneIdx);
        }
    }

    if (uniqueBones.empty()) return E_FAIL;

    _boneChain.assign(uniqueBones.begin(), uniqueBones.end());

    return S_OK;
}

// TODO : Compute 셰이더 편입이 맞는 계산일거 같음
// Result = BasePose times (1 - alpha) + (ModifiedPose times alpha)
void OffsetAnimationModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
{
    if (BlendAlpha(model, timeDelta) == false)
        return;
    
    UpdateWeightOffset(model, timeDelta);
}

void OffsetAnimationModifier::Play_Once(const OneShotPlaybackDesc& desc)
{
    Set_StrengthAlpha(max(0.f, desc.strengthAlpha));

    _oneShotMode = true;
    _oneShotPlaying = true;
    _oneShotElapsedSec = 0.f;
    _oneShotDurationSec = desc.durationSec;
    _oneShotPlayRate = desc.playRate > 0.f ? desc.playRate : 1.f;
    _trackPosition = 0.f;

    if (_ownerModel != nullptr && _animIndex >= 0)
    {
        if (Animation* offsetAnim = _ownerModel->Get_Animation(static_cast<uint32>(_animIndex)))
        {
            const float startRatio = clamp(desc.startRatio, 0.f, 1.f);
            _trackPosition = offsetAnim->Get_Duration() * startRatio;
        }
    }

    Set_Active(true);
}

bool OffsetAnimationModifier::BlendAlpha(ModelCom* model, float timeDelta)
{
    float targetAlpha = _isActive ? 1.f : 0.f;

    if (targetAlpha > _blendAlpha)
    {
        if (_desc.blendInTime > 0.f)
            _blendAlpha = min(_blendAlpha + timeDelta / _desc.blendInTime, targetAlpha);
        else
            _blendAlpha = targetAlpha;
    }
    else if (targetAlpha < _blendAlpha)
    {
        if (_desc.blendOutTime > 0.f)
            _blendAlpha = max(_blendAlpha - timeDelta / _desc.blendOutTime, targetAlpha);
        else
            _blendAlpha = targetAlpha;
    }

    if (_blendAlpha <= 0.f)
    {
        if (!_isActive)
            Reset_OneShotState();

        return false;
    }

    if (nullptr == model || _animIndex < 0 || _boneChain.empty())
        return false;

    return true;
}

void OffsetAnimationModifier::UpdateWeightOffset(ModelCom* model, float timeDelta)
{
    float clampedPitch = clamp(_targetPitch, -_desc.maxPitchAngle, _desc.maxPitchAngle);
    float normalized = clampedPitch / _desc.maxPitchAngle; // -1 ~ 1
    float curved = (normalized >= 0.f ? 1.f : -1.f) * Math::GetEaseInRatio(std::abs(normalized), _desc.responseCurve);
    float weightUp = std::max(curved, 0.f);
    float weightDown = std::max(-curved, 0.f);

    Animation* offsetAnim = model->Get_Animation(_animIndex);
    if (!offsetAnim) return;

    const bool usePlaybackTime = _oneShotMode || _desc.isDynamicPlay;

    if (_oneShotPlaying)
    {
        _oneShotElapsedSec += timeDelta;
        _trackPosition += timeDelta * offsetAnim->Get_TickPerSecond() * _oneShotPlayRate;

        const bool reachedAnimEnd = _trackPosition >= offsetAnim->Get_Duration();
        const bool reachedDuration =
            _oneShotDurationSec > 0.f && _oneShotElapsedSec >= _oneShotDurationSec;

        if (reachedAnimEnd || reachedDuration)
        {
            _trackPosition = min(_trackPosition, offsetAnim->Get_Duration());
            _oneShotPlaying = false;
            Set_Active(false);
        }
    }
    else if (!_oneShotMode && _desc.isDynamicPlay)
    {
        _trackPosition += timeDelta * offsetAnim->Get_TickPerSecond() * _desc.playRate;
        if (_trackPosition > offsetAnim->Get_Duration())
            _trackPosition = fmod(_trackPosition, offsetAnim->Get_Duration());
    }

    auto Get_TransformAtTime = [&](Shared<Channel> channel, float time) {
        RootMotionFrame tf;
        uint32 keyFrame = 0;
        channel->Get_ChannelTransform(time, keyFrame, offsetAnim->Get_Duration(), true, tf);
        return tf;
        };

    auto Get_TransformAtRatio = [&](Shared<Channel> channel, float ratio) {
        return Get_TransformAtTime(channel, offsetAnim->Get_Duration() * ratio);
        };

    for (int32 boneIndex : _boneChain)
    {
        Shared<Channel> boneChannel = nullptr;
        for (auto& channel : offsetAnim->Get_Channels())
        {
            if (channel->Get_BoneIndex() == boneIndex)
            {
                boneChannel = channel;
                break;
            }
        }

        if (!boneChannel) continue;

        RootMotionFrame center = Get_TransformAtRatio(boneChannel, _desc.timeRatio_Center);
        RootMotionFrame target = center;

        if (usePlaybackTime)
        {
            target = Get_TransformAtTime(boneChannel, _trackPosition);
        }
        else
        {
            // 정적 에임 오프셋 시: 상하 가중치에 따라 프레임 혼합
            if (weightUp > FLT_EPSILON) // 가중치가 존재하면
            {
                RootMotionFrame up = Get_TransformAtRatio(boneChannel, _desc.timeRatio_Max);
                target.rotation = Quaternion::Slerp(center.rotation, up.rotation, weightUp);
                target.position = Vector3::Lerp(center.position, up.position, weightUp);
                target.scale = Vector3::Lerp(center.scale, up.scale, weightUp);
            }
            else if (weightDown > FLT_EPSILON)
            {
                RootMotionFrame down = Get_TransformAtRatio(boneChannel, _desc.timeRatio_Min);
                target.rotation = Quaternion::Slerp(center.rotation, down.rotation, weightDown);
                target.position = Vector3::Lerp(center.position, down.position, weightDown);
                target.scale = Vector3::Lerp(center.scale, down.scale, weightDown);
            }
        }

        float effectiveAlpha = _blendAlpha * _strengthAlpha;

        Matrix baseMat = model->Get_BoneLocalMatrix(boneIndex);
        Vec3 baseScale, basePos; Quat baseRot;
        baseMat.Decompose(baseScale, baseRot, basePos);

        if (_desc.blendMode == ModifierBlendMode::ModifyAddictive)
        {
            Quat centerInv; center.rotation.Inverse(centerInv);
            Quat deltaRot = target.rotation * centerInv; deltaRot.Normalize();

            Quat finalRot = Quaternion::Slerp(Quaternion::Identity, deltaRot, effectiveAlpha) * baseRot;
            finalRot.Normalize();

            if (_translationOnlyBones.contains(boneIndex))
            {
                finalRot = baseRot;
            }

            Vec3 finalPos = basePos + (target.position - center.position) * effectiveAlpha;
            model->Set_BoneLocalMatrix(boneIndex, Matrix::CreateScale(baseScale)
                * Matrix::CreateFromQuaternion(finalRot)
                * Matrix::CreateTranslation(finalPos));
        }
        else if (_desc.blendMode == ModifierBlendMode::ModifyReplace)
        {
            Quat finalRot = Quaternion::Slerp(baseRot, target.rotation, effectiveAlpha);
            Vec3 finalPos = Vector3::Lerp(basePos, target.position, effectiveAlpha);
            Vec3 finalScale = Vector3::Lerp(baseScale, target.scale, effectiveAlpha);

            if (_translationOnlyBones.contains(boneIndex))
            {
                finalRot = baseRot;
                finalScale = baseScale;
            }

            model->Set_BoneLocalMatrix(boneIndex, Matrix::CreateScale(finalScale)
                * Matrix::CreateFromQuaternion(finalRot)
                * Matrix::CreateTranslation(finalPos));
        }
    }
}

void OffsetAnimationModifier::Reset_OneShotState()
{
    if (!_oneShotMode && !_oneShotPlaying)
        return;

    _oneShotMode = false;
    _oneShotPlaying = false;
    _oneShotElapsedSec = 0.f;
    _oneShotDurationSec = -1.f;
    _oneShotPlayRate = 1.f;
    _trackPosition = 0.f;
}

Shared<OffsetAnimationModifier> OffsetAnimationModifier::Create(OffsetModifierDesc* modifierDesc)
{
    auto instance = make_shared<OffsetAnimationModifier>();

    CHECK_FAILED(instance->Initialize(modifierDesc), nullptr);

    return instance;
}


