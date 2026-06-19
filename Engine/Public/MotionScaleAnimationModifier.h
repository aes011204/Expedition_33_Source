#pragma once

#include "AnimationModifier.h"

NS_BEGIN(Engine)

class ModelCom;

class ENGINE_DLL MotionScaleAnimationModifier final : public AnimationModifier
{
public:
    struct BoneScaleDesc
    {
        string boneName{};
        Vec3 translationScale{ 1.f, 1.f, 1.f };
    };

    struct MotionScaleDesc : public AnimationModifierDesc
    {
        vector<BoneScaleDesc> boneScales{};
        float blendInSec = 0.f;
        float blendOutSec = 0.f;
    };

private:
    struct RuntimeBoneScale
    {
        int32 boneIndex = -1;
        Vec3 referencePosition{};
        Vec3 translationScale{ 1.f, 1.f, 1.f };
    };

public:
    MotionScaleAnimationModifier() = default;
    ~MotionScaleAnimationModifier() override = default;

public:
    HRESULT Initialize(AnimationModifierDesc* desc) override;
    void Evaluate_Modifier(ModelCom* model, float timeDelta) override;

    void Begin_Control(ModelCom* model, const vector<BoneScaleDesc>& boneScales, float blendInSec, float blendOutSec);
    void End_Control();

private:
    static Vec3 Apply_ComponentScale(const Vec3& base, const Vec3& current, const Vec3& scale);

private:
    vector<RuntimeBoneScale> _runtimeBones{};
    float _blendInSec = 0.f;
    float _blendOutSec = 0.f;
    float _blendAlpha = 0.f;
    bool _targetActive = false;

public:
    static Shared<MotionScaleAnimationModifier> Create(MotionScaleDesc* desc);
};

NS_END
