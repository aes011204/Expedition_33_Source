#include "pch.h"
#include "FacialAnimationModifier.h"

#include "ModelCom.h"

NS_BEGIN(Engine)

namespace
{
    constexpr float kAdditiveTranslationScale = 0.01f;
    constexpr float kUpperEyelidLowTranslationScale = 0.004f;
    constexpr float kUpperEyelidPeakTranslationScale = 0.045f;
    constexpr float kUpperEyelidLowRotationScale = 0.25f;
    constexpr float kUpperEyelidPeakRotationScale = 1.25f;
    constexpr float kUpperEyelidPeakStart = 0.08f;
    constexpr float kUpperEyelidPeakEnd = 0.20f;
    constexpr float kBlinkClosureStart = 0.42f;
    constexpr float kBlinkClosureEnd = 0.68f;
    constexpr float kUpperEyelidClosureOffsetY = -0.085f;
    constexpr float kBlinkMorphGamma = 2.2f;
    constexpr float kBlinkMorphPeakScale = 1.75f;
    constexpr float kEyeRotationScale = 0.25f;

    bool Is_EyeBoneTrack(const string& boneName)
    {
        return boneName == "FACIAL_L_Eye" || boneName == "FACIAL_R_Eye";
    }

    bool Is_UpperEyelidBoneTrack(const string& boneName)
    {
        return boneName.find("EyelidUpper") != string::npos &&
            boneName.find("Furrow") == string::npos;
    }

    bool Is_BlinkMorphTrack(const string& trackName)
    {
        return trackName.find("eye_blink") != string::npos ||
            trackName.find("eyeBlink") != string::npos ||
            trackName.find("CTRL_expressions_eyeBlink") != string::npos ||
            trackName.find("Eblink") != string::npos;
    }

    float Shape_BlinkMorphValue(float value)
    {
        const float sign = value < 0.f ? -1.f : 1.f;
        const float shaped = std::pow(std::abs(value), kBlinkMorphGamma) * kBlinkMorphPeakScale;
        return sign * shaped;
    }

    float SmoothStep(float edge0, float edge1, float value)
    {
        const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    bool Is_MouthAreaTrack(const string& trackName)
    {
        return trackName.find("Mouth") != string::npos ||
            trackName.find("mouth") != string::npos ||
            trackName.find("Lip") != string::npos ||
            trackName.find("lip") != string::npos ||
            trackName.find("Jaw") != string::npos ||
            trackName.find("jaw") != string::npos ||
            trackName.find("Chin") != string::npos ||
            trackName.find("chin") != string::npos ||
            trackName.find("Nasolabial") != string::npos ||
            trackName.find("nasolabial") != string::npos;
    }
}

HRESULT FacialAnimationModifier::Initialize(AnimationModifierDesc* desc)
{
    CHECK_FAILED(__super::Initialize(desc), E_FAIL);

    auto facialDesc = static_cast<FacialModifierDesc*>(desc);
    _clip = facialDesc->clip;
    _loop = facialDesc->loop;
    _suppressMouthArea = facialDesc->suppressMouthArea;
    _applyMorphCpu = facialDesc->applyMorphCpu;
    _bindingsDirty = true;
    _timelineTimeSec = 0.f;
    return S_OK;
}

void FacialAnimationModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
{
    if (model == nullptr || _clip == nullptr)
        return;

    if (!_isActive)
    {
        if (_poseApplied)
        {
            Reset_Pose(model);
            if (_applyMorphCpu)
                CHECK_FAILED(model->Apply_MorphWeightsToCpuVertices(), );
        }
        return;
    }

    if (_bindingsDirty)
        Rebuild_Bindings(model);

    _timelineTimeSec += std::max(0.f, timeDelta);

    model->Reset_MorphWeights();

    float blinkClosure = 0.f;
    for (const MorphBinding& binding : _morphBindings)
    {
        if (binding.track == nullptr || binding.targetIndex < 0)
            continue;

        float value{};
        if (!_clip->Evaluate_MorphTrack(*binding.track, _timelineTimeSec, _loop, value))
            continue;

        const float morphValue = binding.isBlinkMorph ? Shape_BlinkMorphValue(value) : value;
        model->Set_MorphWeight(static_cast<uint32>(binding.targetIndex), morphValue * _strengthAlpha);

        if (binding.isBlinkMorph)
            blinkClosure = std::max(blinkClosure, std::abs(value));
    }

    for (const BoneBinding& binding : _boneBindings)
    {
        if (binding.track == nullptr || binding.boneIndex < 0)
            continue;

        FacialClip::TransformKey key{};
        if (!_clip->Evaluate_BoneTrack(*binding.track, _timelineTimeSec, _loop, key))
            continue;

        const bool additiveLocal = _clip->Is_AdditiveLocal();
        const Matrix sampledLocal = Build_LocalMatrix(
            key,
            additiveLocal,
            binding.isEyeBone,
            binding.isUpperEyelidBone,
            blinkClosure);
        const Matrix targetLocal = additiveLocal
            ? sampledLocal * binding.baseLocal
            : sampledLocal;

        model->Set_BoneLocalMatrix(static_cast<uint32>(binding.boneIndex), targetLocal);
    }

    _poseApplied = true;

    if (_applyMorphCpu)
        CHECK_FAILED(model->Apply_MorphWeightsToCpuVertices(), );
}

void FacialAnimationModifier::On_ActiveChanged(ModelCom* model, bool isActive)
{
    if (isActive || model == nullptr || !_poseApplied)
        return;

    Reset_Pose(model);

    if (_applyMorphCpu)
    {
        model->Rebuild_BoneCombinedMatrices();
        CHECK_FAILED(model->Apply_MorphWeightsToCpuVertices(), );
    }
}

void FacialAnimationModifier::Set_FacialClip(const Shared<FacialClip>& clip)
{
    _clip = clip;
    _morphBindings.clear();
    _boneBindings.clear();
    _bindingsDirty = true;
    _timelineTimeSec = 0.f;
    _poseApplied = false;
}

void FacialAnimationModifier::Reset_Pose(ModelCom* model)
{
    if (model == nullptr || !_poseApplied)
        return;

    model->Reset_MorphWeights();

    for (const BoneBinding& binding : _boneBindings)
    {
        if (binding.boneIndex < 0)
            continue;

        model->Set_BoneLocalMatrix(static_cast<uint32>(binding.boneIndex), binding.baseLocal);
    }

    _timelineTimeSec = 0.f;
    _poseApplied = false;
}

void FacialAnimationModifier::Rebuild_Bindings(ModelCom* model)
{
    _morphBindings.clear();
    _boneBindings.clear();

    if (model == nullptr || _clip == nullptr)
    {
        _bindingsDirty = false;
        return;
    }

    for (const FacialClip::MorphTrack& track : _clip->Get_MorphTracks())
    {
        if (_suppressMouthArea && Is_MouthAreaTrack(track.name))
            continue;

        const int32 targetIndex = model->Find_MorphTargetIndex(track.name);
        if (targetIndex < 0)
            continue;

        _morphBindings.push_back(MorphBinding{ &track, targetIndex, Is_BlinkMorphTrack(track.name) });
    }

    for (const FacialClip::BoneTrack& track : _clip->Get_BoneTracks())
    {
        if (_suppressMouthArea && Is_MouthAreaTrack(track.name))
            continue;

        const int32 boneIndex = model->Get_BoneIndex(track.name);
        if (boneIndex < 0)
            continue;

        _boneBindings.push_back(BoneBinding{
            &track,
            boneIndex,
            model->Get_BoneLocalMatrix(static_cast<uint32>(boneIndex)),
            Is_EyeBoneTrack(track.name),
            Is_UpperEyelidBoneTrack(track.name) });
    }

    _bindingsDirty = false;
}

Matrix FacialAnimationModifier::Build_LocalMatrix(
    const FacialClip::TransformKey& key,
    bool additiveLocal,
    bool eyeBone,
    bool upperEyelidBone,
    float blinkClosure) const
{
    Vec3 scale = key.scale;
    Vec3 position = key.position;
    Quat rotation = key.rotation;

    if (additiveLocal)
    {
        scale = Vec3::One + key.scale;
        float translationScale = kAdditiveTranslationScale;

        if (upperEyelidBone)
        {
            const float peakAlpha = SmoothStep(
                kUpperEyelidPeakStart,
                kUpperEyelidPeakEnd,
                key.position.Length());
            translationScale = std::lerp(
                kUpperEyelidLowTranslationScale,
                kUpperEyelidPeakTranslationScale,
                peakAlpha);
            rotation = Quat::Slerp(
                Quat::Identity,
                rotation,
                std::lerp(kUpperEyelidLowRotationScale, kUpperEyelidPeakRotationScale, peakAlpha));
        }

        position *= translationScale;

        if (upperEyelidBone)
        {
            const float closureAlpha = SmoothStep(
                kBlinkClosureStart,
                kBlinkClosureEnd,
                blinkClosure);
            position.y += kUpperEyelidClosureOffsetY * closureAlpha;
        }

        if (eyeBone)
            rotation = Quat::Slerp(Quat::Identity, rotation, kEyeRotationScale);
    }

    return Matrix::CreateScale(scale) *
        Matrix::CreateFromQuaternion(rotation) *
        Matrix::CreateTranslation(position);
}

Shared<FacialAnimationModifier> FacialAnimationModifier::Create(FacialModifierDesc* desc)
{
    auto instance = make_shared<FacialAnimationModifier>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END
