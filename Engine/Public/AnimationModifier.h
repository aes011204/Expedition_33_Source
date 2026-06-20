#pragma once
#include "Base.h"

NS_BEGIN(Engine)

struct ClassReflectionInfo;
struct PropertyInfo;
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
        bool evaluateOnLeaderPose{ false };
        ModifierBlendMode blendMode{ ModifierBlendMode::ModifyReplace };
    };

public:
    AnimationModifier() = default;
    virtual ~AnimationModifier() override = default;

    virtual void Evaluate_Modifier(ModelCom* model, float timeDelta) PURE;

    virtual const ClassReflectionInfo* Get_RuntimeReflectionInfo() const { return nullptr; }
    virtual void* Get_RuntimeReflectionTarget() { return nullptr; }
    virtual void On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property) { UNREFERENCED_PARAMETER(model); UNREFERENCED_PARAMETER(property); }
    virtual bool Can_RuntimeResetPose() const { return false; }
    virtual void RuntimeResetPose(ModelCom* model) { UNREFERENCED_PARAMETER(model); }
    virtual bool Can_RuntimeRebuild() const { return false; }
    virtual void RuntimeRebuild(ModelCom* model) { UNREFERENCED_PARAMETER(model); }

    void Set_Active(bool isActive);
    bool Is_Active() const { return _isActive; }

    void Set_StrengthAlpha(float alpha) { _strengthAlpha = alpha; }
    float Get_StrengthAlpha() const { return _strengthAlpha; }

    const wstring& Get_ModifierTag() const { return _modifierTag; }

    bool Is_EvaluateOnLeaderPose() const { return _evaluateOnLeaderPose; }
    ModifierBlendMode Get_BlendMode() const { return _blendMode; }
    void Bind_Model(ModelCom* model) { _ownerModel = model; }

protected:
    virtual HRESULT Initialize(AnimationModifierDesc* desc);
    virtual void On_ActiveChanged(ModelCom* model, bool isActive) { UNREFERENCED_PARAMETER(model); UNREFERENCED_PARAMETER(isActive); }

protected:
    bool _isActive = true;
    bool _evaluateOnLeaderPose = false;
    ModelCom* _ownerModel{ nullptr };
    float _strengthAlpha{ 1.f }; // 왜 인지 모르지만 alpha로 언리얼에서 표현 하는듯 섞을 애니메이션 강도 0.f ~ 1.f
    ModifierBlendMode _blendMode{ ModifierBlendMode::ModifyReplace };
    wstring _modifierTag{};

};

NS_END
