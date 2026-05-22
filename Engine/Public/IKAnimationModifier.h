#pragma once
#include "AnimationModifier.h"

NS_BEGIN(Engine)

class ModelCom;

class ENGINE_DLL IKAnimationModifier final : public AnimationModifier
{
    struct IKModifierDesc
    {
        ModelCom* baseModelCom{ nullptr };
        string endBoneName{};
        string rootBoneName{};
        ModelBoneRotationLimits limits{};
    };

public:
    IKAnimationModifier() = default;
    ~IKAnimationModifier() override = default;

public:
    HRESULT Initialize(const IKModifierDesc& IKModifierDesc);
    void Evaluate_Modifier(ModelCom* model, float timeDelta, float alpha) override;

public:
    void Set_AimAngles(float yaw, float pitch);
    void Setup_BoneChain(const string& endBoneName, const string& InvRootBoneName);
    void Set_Limits(const ModelBoneRotationLimits& limits);
    
private:
    string _endBoneName{};
    string _rootBoneName{};
    vector<ModelBoneNode> _boneChain;
    ModelBoneRotationLimits _limits = {
        -XM_PI, XM_PI, -XM_PI, XM_PI
    };

    float _targetYaw{ 0.f };
    float _targetPitch{ 0.f };

public:
    static Shared<IKAnimationModifier> Create(const IKModifierDesc& IKModifierDesc);

};

NS_END
