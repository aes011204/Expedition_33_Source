#include "pch.h"
#include "MotionScaleAnimationModifier.h"

#include "ModelCom.h"

HRESULT MotionScaleAnimationModifier::Initialize(AnimationModifierDesc* desc)
{
    CHECK_FAILED(__super::Initialize(desc), E_FAIL);
    return S_OK;
}

void MotionScaleAnimationModifier::Begin_Control(
    ModelCom* model,
    const vector<BoneScaleDesc>& boneScales,
    float blendInSec,
    float blendOutSec)
{
    _runtimeBones.clear();
    _blendInSec = max(0.f, blendInSec);
    _blendOutSec = max(0.f, blendOutSec);
    _targetActive = true;

    if (_blendInSec <= 0.f)
        _blendAlpha = 1.f;

    if (model == nullptr)
        return;

    for (const BoneScaleDesc& desc : boneScales)
    {
        const int32 boneIndex = model->Get_BoneIndex(desc.boneName);
        if (boneIndex < 0)
            continue;

        Vec3 baseScale{};
        Vec3 position{};
        Quat baseRotation{};
        Matrix local = model->Get_BoneLocalMatrix(boneIndex);
        local.Decompose(baseScale, baseRotation, position);

        RuntimeBoneScale runtime{};
        runtime.boneIndex = boneIndex;
        runtime.referencePosition = position;
        runtime.translationScale = desc.translationScale;
        _runtimeBones.push_back(runtime);
    }
}

void MotionScaleAnimationModifier::End_Control()
{
    _targetActive = false;

    if (_blendOutSec <= 0.f)
    {
        _blendAlpha = 0.f;
        _runtimeBones.clear();
    }
}

void MotionScaleAnimationModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
{
    if (model == nullptr)
        return;

    const float targetAlpha = _targetActive ? 1.f : 0.f;

    if (_blendAlpha < targetAlpha)
        _blendAlpha = (_blendInSec > 0.f) ? min(_blendAlpha + timeDelta / _blendInSec, targetAlpha) : targetAlpha;
    else if (_blendAlpha > targetAlpha)
        _blendAlpha = (_blendOutSec > 0.f) ? max(_blendAlpha - timeDelta / _blendOutSec, targetAlpha) : targetAlpha;

    if (_blendAlpha <= 0.f)
    {
        if (!_targetActive)
            _runtimeBones.clear();

        return;
    }

    for (const RuntimeBoneScale& runtime : _runtimeBones)
    {
        Matrix local = model->Get_BoneLocalMatrix(runtime.boneIndex);

        Vec3 baseScale{};
        Vec3 currentPosition{};
        Quat baseRotation{};
        local.Decompose(baseScale, baseRotation, currentPosition);

        const Vec3 scaledPosition =
            Apply_ComponentScale(runtime.referencePosition, currentPosition, runtime.translationScale);

        const Vec3 finalPosition = Vec3::Lerp(currentPosition, scaledPosition, _blendAlpha);

        model->Set_BoneLocalMatrix(
            runtime.boneIndex,
            Matrix::CreateScale(baseScale)
            * Matrix::CreateFromQuaternion(baseRotation)
            * Matrix::CreateTranslation(finalPosition));
    }
}

Vec3 MotionScaleAnimationModifier::Apply_ComponentScale(const Vec3& base, const Vec3& current, const Vec3& scale)
{
    const Vec3 delta = current - base;

    return Vec3(
        base.x + delta.x * scale.x,
        base.y + delta.y * scale.y,
        base.z + delta.z * scale.z);
}

Shared<MotionScaleAnimationModifier> MotionScaleAnimationModifier::Create(MotionScaleDesc* desc)
{
    auto instance = make_shared<MotionScaleAnimationModifier>();

    CHECK_FAILED(instance->Initialize(desc), nullptr);

    return instance;
}
