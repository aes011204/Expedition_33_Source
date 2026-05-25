#pragma once
#include "Base.h"
#include "Delegate.h"

NS_BEGIN(Engine)

    class ENGINE_DLL ScreenFx_Instance abstract : Base
{
public:
    DECLARE_DELEGATE(EndCondition, bool&);

    enum class ScreenFx_BlendMode : uint8
    {
        Override,    // 기존 값을 덮어씀 (나중에 들어온 효과가 우선)
        Add,         // 기존 값에 더함 (빛이 강해지는 효과, 예: Bloom 중첩)
        Multiply,    // 기존 값에 곱함 (어두워지는 효과, 예: Vignette 중첩)
        Max,         // 기존 값과 비교해 더 큰 값을 취함 (가장 강한 효과 유지)
        Min,         // 기존 값과 비교해 더 작은 값을 취함
    };

    enum class ScreenFx_Type : uint32
    {
        ToneMapping,    // Color, Tone
        ColorGrading,
        Vignette,       // Lens, Camera
        Bloom, 
        RadialBlur,
        ChromaticAberration, // Color split
        Ghosting,       // Custom & Transition
        Transition,
        Distortion,
    };

    struct ScreenFxDesc
    {
        ScreenFx_Type type{};
        ScreenFx_BlendMode blendMode{};
        EndCondition endCondition{};
    };

public:
    ScreenFx_Instance(ScreenFxDesc* desc);
    virtual ~ScreenFx_Instance() override = default;

public:
    virtual bool Update_ScreenFx(float timeDelta);
    virtual void Apply_To(struct ScreenFxCB& outParameters) PURE;
    

    ScreenFx_Type           Get_FxType() const { return _type; }
    ScreenFx_BlendMode      Get_FxBlendMode() const { return _blendMode; }

protected:
    ScreenFx_Type           _type{};
    ScreenFx_BlendMode      _blendMode{};
    EndCondition            _endCondition{};
};

NS_END

