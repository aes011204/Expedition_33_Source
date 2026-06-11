#pragma once
#include "AnimationModifier.h"
#include "FacialClip.h"

NS_BEGIN(Engine)

class ENGINE_DLL FacialAnimationModifier final : public AnimationModifier
{
public:
    struct FacialModifierDesc : public AnimationModifierDesc
    {
        Shared<FacialClip> clip{};
        bool loop{ true };
        bool suppressMouthArea{ false };
    };

private:
    struct MorphBinding
    {
        const FacialClip::MorphTrack* track{};
        int32 targetIndex{ -1 };
        bool isBlinkMorph{ false };
    };

    struct BoneBinding
    {
        const FacialClip::BoneTrack* track{};
        int32 boneIndex{ -1 };
        Matrix baseLocal{ Matrix::Identity };
        bool isEyeBone{ false };
        bool isUpperEyelidBone{ false };
    };

public:
    FacialAnimationModifier() = default;
    ~FacialAnimationModifier() override = default;

public:
    HRESULT Initialize(AnimationModifierDesc* desc) override;
    void Evaluate_Modifier(ModelCom* model, float timeDelta) override;

    void Set_FacialClip(const Shared<FacialClip>& clip);
    void Set_Loop(bool loop) { _loop = loop; }
    void Set_TimelineTimeSec(float timeSec) { _timelineTimeSec = std::max(0.f, timeSec); }
    void Reset_Pose(ModelCom* model);

private:
    void Rebuild_Bindings(ModelCom* model);
    Matrix Build_LocalMatrix(
        const FacialClip::TransformKey& key,
        bool additiveLocal,
        bool eyeBone,
        bool upperEyelidBone,
        float blinkClosure) const;

private:
    Shared<FacialClip> _clip{};
    vector<MorphBinding> _morphBindings{};
    vector<BoneBinding> _boneBindings{};
    float _timelineTimeSec{ 0.f };
    bool _loop{ true };
    bool _suppressMouthArea{ false };
    bool _bindingsDirty{ true };
    bool _poseApplied{ false };

public:
    static Shared<FacialAnimationModifier> Create(FacialModifierDesc* desc);
};

NS_END
