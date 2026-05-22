#pragma once
#include "AnimationModifier.h"

NS_BEGIN(Engine)

class ModelCom;

class ENGINE_DLL OffsetModifier final : public AnimationModifier
{
public:
    struct OffsetModifierDesc
    {
        ModelCom* baseModelCom{ nullptr };
        string animName{};
        string endBoneName{};
        string rootBoneName{};
        float maxPitchAngle{};

        float TimeRatio_Up = 1.0f;     
        float TimeRatio_Center = 0.5f; 
        float TimeRatio_Down = 0.0f; 
    };
public:
    OffsetModifier() = default;
    ~OffsetModifier() override = default;

public:
    HRESULT Initialize(const OffsetModifierDesc& desc);
    void Evaluate_Modifier(ModelCom* model, float timeDelta, float alpha) override;
    void Set_AimPitch(float pitch) { _targetPitch = pitch; }

private:
    OffsetModifierDesc   _desc{};
    vector<int32>        _boneChain;
    float                _targetPitch{ 0.f };
    int32                _animIndex{ -1 };

public:
    static Shared<OffsetModifier> Create(const OffsetModifierDesc& modifierDesc);
};

NS_END
