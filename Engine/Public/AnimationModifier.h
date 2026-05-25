#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class ModelCom;

class ENGINE_DLL AnimationModifier : public Base
{
public:
    enum class ModifierBlendMode
    {
        ModifyReplace,   // 해당 애니메이션 구성으로 변경
        ModifyAddictive, // 해당 애니메이션 움직임을 기존 애니메이션에 누적함
    };

public:
    struct AnimationModifierDesc
    {
        wstring modifierTag{};
        float strengthAlpha{ 1.f };
    };

public:
    AnimationModifier() = default;
    virtual ~AnimationModifier() override = default;

    virtual void Evaluate_Modifier(ModelCom* model, float timeDelta) PURE;

    void Set_Active(bool isActive) { _isActive = isActive; }
    bool Is_Active() const { return _isActive; }

    void Set_StrengthAlpha(float alpha) { _strengthAlpha = alpha; }
    float Get_StrengthAlpha() const { return _strengthAlpha; }

    const wstring& Get_ModifierTag() const { return _modifierTag; }

protected:
    virtual HRESULT Initialize(AnimationModifierDesc* desc);

protected:
    bool _isActive = true;
    float _strengthAlpha{ 1.f }; // 왜 인지 모르지만 alpha로 언리얼에서 표현 하는듯 섞을 애니메이션 강도 0.f ~ 1.f
    wstring _modifierTag{};

};

NS_END
