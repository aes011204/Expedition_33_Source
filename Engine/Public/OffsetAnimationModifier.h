#pragma once
#include "AnimationModifier.h"

NS_BEGIN(Engine)

class ModelCom;

class ENGINE_DLL OffsetAnimationModifier final : public AnimationModifier
{
public:
    struct OffsetModifierDesc : public AnimationModifierDesc
    {
        ModelCom* baseModelCom{ nullptr };
        string animName{};
        vector<string> endBoneNames{};
        string rootBoneName{};
        float maxPitchAngle{ 60.f };
        ModifierBlendMode blendMode{ ModifierBlendMode::ModifyReplace };
        // 사용할 애니메이션 범위 정의 0.f ~ 1.f
        float timeRatio_Max{ 1.0f };
        float timeRatio_Center{ 0.5f };
        float timeRatio_Min{ 0.0f };
        // 보간 프로퍼티
        float responseCurve{ 1.f };
        float blendInTime{ 0.f };
        float blendOutTime{ 0.f };
    };
public:
    OffsetAnimationModifier() = default;
    ~OffsetAnimationModifier() override = default;

public:
    HRESULT Initialize(AnimationModifierDesc* desc) override;
    void Evaluate_Modifier(ModelCom* model, float timeDelta) override;
    void Set_AimPitch(float pitch) { _targetPitch = pitch; }

private:
    bool BlendAlpha(ModelCom* model, float timeDelta);
    void UpdateWeightOffset(ModelCom* model, float timeDelta);

private:
    OffsetModifierDesc   _desc{};
    vector<int32>        _boneChain;
    float                _targetPitch{ 0.f };
    int32                _animIndex{ -1 };
    float                _blendAlpha{ 0.f };

public:
    static Shared<OffsetAnimationModifier> Create(OffsetModifierDesc* modifierDesc);
};

NS_END
