#include "PostProcessPipeline.h"

NS_BEGIN(Engine)

namespace
{
    DOFPreset Build_DOFPresetFromCB(const PostProcessCB& cb, bool enableDOF)
    {
        DOFPreset preset{};
        preset.enableDOF = enableDOF;
        preset.focusDistance = cb.dofFocusDistance;
        preset.focusRange = cb.dofFocusRange;
        preset.nearMaxRadius = cb.dofNearMaxRadius;
        preset.farMaxRadius = cb.dofFarMaxRadius;
        preset.bokehThreshold = cb.dofBokehThreshold;
        preset.bokehIntensity = cb.dofBokehIntensity;
        preset.depthSharpness = cb.dofDepthSharpness;
        preset.cocEpsilon = cb.dofCoCEpsilon;
        preset.nearIntensity = cb.dofNearIntensity;
        preset.farIntensity = cb.dofFarIntensity;
        preset.sampleCount = cb.dofSampleCount;
        preset.debugMode = cb.dofDebugMode;
        return preset;
    }

    void Apply_DOFPresetToCB(const DOFPreset& preset, PostProcessCB& cb)
    {
        cb.dofFocusDistance = preset.focusDistance;
        cb.dofFocusRange = preset.focusRange;
        cb.dofNearMaxRadius = preset.nearMaxRadius;
        cb.dofFarMaxRadius = preset.farMaxRadius;
        cb.dofBokehThreshold = preset.bokehThreshold;
        cb.dofBokehIntensity = preset.bokehIntensity;
        cb.dofDepthSharpness = preset.depthSharpness;
        cb.dofCoCEpsilon = preset.cocEpsilon;
        cb.dofNearIntensity = preset.nearIntensity;
        cb.dofFarIntensity = preset.farIntensity;
        cb.dofSampleCount = preset.sampleCount;
        cb.dofDebugMode = preset.debugMode;
    }

    bool Is_SameDOFPreset(const DOFPreset& lhs, const DOFPreset& rhs)
    {
        return lhs.enableDOF == rhs.enableDOF &&
            lhs.focusDistance == rhs.focusDistance &&
            lhs.focusRange == rhs.focusRange &&
            lhs.nearMaxRadius == rhs.nearMaxRadius &&
            lhs.farMaxRadius == rhs.farMaxRadius &&
            lhs.bokehThreshold == rhs.bokehThreshold &&
            lhs.bokehIntensity == rhs.bokehIntensity &&
            lhs.depthSharpness == rhs.depthSharpness &&
            lhs.cocEpsilon == rhs.cocEpsilon &&
            lhs.nearIntensity == rhs.nearIntensity &&
            lhs.farIntensity == rhs.farIntensity &&
            lhs.sampleCount == rhs.sampleCount &&
            lhs.debugMode == rhs.debugMode;
    }

    float Evaluate_DOFTransitionT(DOFTransitionEasing easing, float t)
    {
        t = clamp(t, 0.f, 1.f);
        constexpr float kBackOvershoot = 1.70158f;

        switch (easing)
        {
        case DOFTransitionEasing::Linear:
            return t;
        case DOFTransitionEasing::EaseIn:
            return t * t;
        case DOFTransitionEasing::EaseOut:
            return 1.f - (1.f - t) * (1.f - t);
        case DOFTransitionEasing::EaseInStrong:
        case DOFTransitionEasing::EaseInCubic:
            return t * t * t;
        case DOFTransitionEasing::EaseOutStrong:
        case DOFTransitionEasing::EaseOutCubic:
            return 1.f - powf(1.f - t, 3.f);
        case DOFTransitionEasing::EaseInOutStrong:
            return t < 0.5f ? 4.f * t * t * t : 1.f - powf(-2.f * t + 2.f, 3.f) * 0.5f;
        case DOFTransitionEasing::EaseInExpo:
            return t <= 0.f ? 0.f : powf(2.f, 10.f * t - 10.f);
        case DOFTransitionEasing::EaseOutExpo:
            return t >= 1.f ? 1.f : 1.f - powf(2.f, -10.f * t);
        case DOFTransitionEasing::EaseOutBack:
        {
            const float x = t - 1.f;
            return 1.f + (kBackOvershoot + 1.f) * x * x * x + kBackOvershoot * x * x;
        }
        case DOFTransitionEasing::EaseInOut:
            return t * t * (3.f - 2.f * t);
        default:
            return t;
        }
    }

    DOFPreset Lerp_DOFPreset(const DOFPreset& from, const DOFPreset& to, float t)
    {
        DOFPreset result = to;
        result.enableDOF = (t < 1.f) ? from.enableDOF : to.enableDOF;
        result.focusDistance = lerp(from.focusDistance, to.focusDistance, t);
        result.focusRange = lerp(from.focusRange, to.focusRange, t);
        result.nearMaxRadius = lerp(from.nearMaxRadius, to.nearMaxRadius, t);
        result.farMaxRadius = lerp(from.farMaxRadius, to.farMaxRadius, t);
        result.bokehThreshold = lerp(from.bokehThreshold, to.bokehThreshold, t);
        result.bokehIntensity = lerp(from.bokehIntensity, to.bokehIntensity, t);
        result.depthSharpness = lerp(from.depthSharpness, to.depthSharpness, t);
        result.cocEpsilon = lerp(from.cocEpsilon, to.cocEpsilon, t);
        result.nearIntensity = lerp(from.nearIntensity, to.nearIntensity, t);
        result.farIntensity = lerp(from.farIntensity, to.farIntensity, t);
        result.sampleCount = to.sampleCount;
        result.debugMode = to.debugMode;
        return result;
    }

    void Copy_DOFPresetValues(const DOFPreset& source, DOFPreset& dest)
    {
        dest.focusDistance = source.focusDistance;
        dest.focusRange = source.focusRange;
        dest.nearMaxRadius = source.nearMaxRadius;
        dest.farMaxRadius = source.farMaxRadius;
        dest.bokehThreshold = source.bokehThreshold;
        dest.bokehIntensity = source.bokehIntensity;
        dest.depthSharpness = source.depthSharpness;
        dest.cocEpsilon = source.cocEpsilon;
        dest.nearIntensity = source.nearIntensity;
        dest.farIntensity = source.farIntensity;
        dest.sampleCount = source.sampleCount;
        dest.debugMode = source.debugMode;
    }
}

PostProcessPipeline::PostProcessPipeline(ComPtr<Device> device, ComPtr<Context> context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT PostProcessPipeline::Initialize(PostProcessFeature initialFlag)
{
    _featureFlag = initialFlag;
    _postProcessCB.featureMask = To_Underlying(_featureFlag);

    return S_OK;
}

void PostProcessPipeline::Set_PostProcessFeatures(PostProcessFeature featureFlag)
{
    _featureFlag = featureFlag;
    _postProcessCB.featureMask = To_Underlying(_featureFlag);
}

void PostProcessPipeline::Set_PostProcessFeature(PostProcessFeature featureFlag, bool enable)
{
    if (featureFlag == PostProcessFeature::DOF)
        _dofForceDisabled = !enable;

    Set_Flag(_featureFlag, featureFlag, enable);
    _postProcessCB.featureMask = To_Underlying(_featureFlag);
}

bool PostProcessPipeline::Has_PostProcessFeature(PostProcessFeature featureFlag) const
{
    if (featureFlag == PostProcessFeature::DOF && _dofForceDisabled)
        return false;

    return Has_Flag(_featureFlag, featureFlag);
}

void PostProcessPipeline::Set_PostProcessParams(const PostProcessCB& postProcessCB)
{
    _postProcessCB = postProcessCB;
    _postProcessCB.featureMask = To_Underlying(_featureFlag);

    if (_dofOverride.scopeActive)
    {
        const DOFPreset editedPreset = Build_DOFPresetFromCB(_postProcessCB, true);
        Copy_DOFPresetValues(editedPreset, _dofOverride.fromPreset);
        Copy_DOFPresetValues(editedPreset, _dofOverride.currentPreset);
        Copy_DOFPresetValues(editedPreset, _dofOverride.targetPreset);
    }
}

const PostProcessCB& PostProcessPipeline::Get_PostProcessParams() const
{
    return _postProcessCB;
}

PostProcessCB& PostProcessPipeline::Get_PostProcessParams()
{
    return _postProcessCB;
}

HRESULT PostProcessPipeline::Begin_DOFOverrideScope(const string& ownerTag)
{
    if (ownerTag.empty())
        return E_FAIL;

    if (_dofOverride.scopeActive)
    {
        if (_dofOverride.ownerTag == ownerTag)
            return S_FALSE;

        LOG_WARN("DOF override scope already active. owner='{}', requester='{}'", _dofOverride.ownerTag.c_str(), ownerTag.c_str());
        return E_FAIL;
    }

    _dofOverride.scopeActive = true;
    _dofOverride.pendingScopeEnd = false;
    _dofOverride.baselineDOFEnabled = Has_PostProcessFeature(PostProcessFeature::DOF);
    _dofOverride.ownerTag = ownerTag;
    _dofOverride.baselinePreset = Build_DOFPresetFromCB(_postProcessCB, _dofOverride.baselineDOFEnabled);
    _dofOverride.fromPreset = _dofOverride.baselinePreset;
    _dofOverride.currentPreset = _dofOverride.baselinePreset;
    _dofOverride.targetPreset = _dofOverride.baselinePreset;
    _dofOverride.transitionActive = false;
    _dofOverride.elapsed = 0.f;
    _dofOverride.duration = 0.35f;
    _dofOverride.easing = DOFTransitionEasing::EaseInOut;

    return S_OK;
}

HRESULT PostProcessPipeline::Set_DOFOverridePreset(const string& ownerTag, const DOFPreset& preset, const DOFTransitionDesc& transition)
{
    if (!_dofOverride.scopeActive || _dofOverride.ownerTag != ownerTag)
        return E_FAIL;

    DOFPreset targetPreset = preset;
    targetPreset.enableDOF = true;

    if (Is_SameDOFPreset(_dofOverride.targetPreset, targetPreset))
        return S_FALSE;

    _dofOverride.pendingScopeEnd = false;
    _dofOverride.currentPreset = Build_DOFPresetFromCB(_postProcessCB, Has_PostProcessFeature(PostProcessFeature::DOF));
    _dofOverride.fromPreset = _dofOverride.currentPreset;
    _dofOverride.targetPreset = targetPreset;
    _dofOverride.easing = transition.easing;
    _dofOverride.duration = max(0.f, transition.duration);
    _dofOverride.elapsed = 0.f;
    _dofOverride.transitionActive = _dofOverride.duration > 0.f;

    if (!_dofOverride.transitionActive)
    {
        _dofOverride.currentPreset = _dofOverride.targetPreset;
        Apply_DOFPresetToCB(_dofOverride.currentPreset, _postProcessCB);
        if (!_dofForceDisabled)
            Set_PostProcessFeature(PostProcessFeature::DOF, _dofOverride.currentPreset.enableDOF);
    }

    return S_OK;
}

HRESULT PostProcessPipeline::Set_DOFOverrideFocus(const string& ownerTag, float focusDistance, float focusRange)
{
    if (!_dofOverride.scopeActive || _dofOverride.ownerTag != ownerTag)
        return E_FAIL;

    _dofOverride.targetPreset.enableDOF = true;
    _dofOverride.targetPreset.focusDistance = max(0.f, focusDistance);
    _dofOverride.targetPreset.focusRange = max(0.01f, focusRange);

    if (!_dofOverride.transitionActive)
    {
        _dofOverride.currentPreset.focusDistance = _dofOverride.targetPreset.focusDistance;
        _dofOverride.currentPreset.focusRange = _dofOverride.targetPreset.focusRange;
        Apply_DOFPresetToCB(_dofOverride.currentPreset, _postProcessCB);
        if (!_dofForceDisabled)
            Set_PostProcessFeature(PostProcessFeature::DOF, _dofOverride.currentPreset.enableDOF);
    }

    return S_OK;
}

HRESULT PostProcessPipeline::Clear_DOFOverrideToBaseline(const string& ownerTag, const DOFTransitionDesc& transition)
{
    if (!_dofOverride.scopeActive || _dofOverride.ownerTag != ownerTag)
        return E_FAIL;

    DOFPreset baselinePreset = _dofOverride.baselinePreset;
    baselinePreset.enableDOF = _dofOverride.baselineDOFEnabled;

    if (Is_SameDOFPreset(_dofOverride.targetPreset, baselinePreset))
        return S_FALSE;

    _dofOverride.currentPreset = Build_DOFPresetFromCB(_postProcessCB, Has_PostProcessFeature(PostProcessFeature::DOF));
    _dofOverride.fromPreset = _dofOverride.currentPreset;
    _dofOverride.targetPreset = baselinePreset;
    _dofOverride.easing = transition.easing;
    _dofOverride.duration = max(0.f, transition.duration);
    _dofOverride.elapsed = 0.f;
    _dofOverride.transitionActive = _dofOverride.duration > 0.f;

    if (!_dofOverride.transitionActive)
    {
        _dofOverride.currentPreset = _dofOverride.targetPreset;
        Apply_DOFPresetToCB(_dofOverride.currentPreset, _postProcessCB);
        if (!_dofForceDisabled)
            Set_PostProcessFeature(PostProcessFeature::DOF, _dofOverride.currentPreset.enableDOF);
    }

    return S_OK;
}

HRESULT PostProcessPipeline::End_DOFOverrideScope(const string& ownerTag, const DOFTransitionDesc& transition)
{
    if (!_dofOverride.scopeActive || _dofOverride.ownerTag != ownerTag)
        return E_FAIL;

    _dofOverride.pendingScopeEnd = true;
    const HRESULT clearResult = Clear_DOFOverrideToBaseline(ownerTag, transition);

    if (clearResult == S_FALSE)
    {
        if (_dofOverride.transitionActive)
            return S_OK;

        _dofOverride.currentPreset = _dofOverride.targetPreset;
        Apply_DOFPresetToCB(_dofOverride.currentPreset, _postProcessCB);
        if (_dofForceDisabled)
            Set_Flag(_featureFlag, PostProcessFeature::DOF, false);
        else
            Set_Flag(_featureFlag, PostProcessFeature::DOF, _dofOverride.currentPreset.enableDOF);
        _postProcessCB.featureMask = To_Underlying(_featureFlag);
        _dofOverride = DOFOverrideState{};
        return S_OK;
    }

    if (SUCCEEDED(clearResult) && !_dofOverride.transitionActive)
        _dofOverride = DOFOverrideState{};

    return clearResult;
}

void PostProcessPipeline::Update(float timeDelta)
{
    if (!_dofOverride.scopeActive)
        return;

    if (_dofOverride.transitionActive)
    {
        _dofOverride.elapsed += max(0.f, timeDelta);
        const float duration = max(0.0001f, _dofOverride.duration);
        const float rawT = clamp(_dofOverride.elapsed / duration, 0.f, 1.f);
        const float easedT = Evaluate_DOFTransitionT(_dofOverride.easing, rawT);

        _dofOverride.currentPreset = Lerp_DOFPreset(_dofOverride.fromPreset, _dofOverride.targetPreset, easedT);

        if (rawT >= 1.f)
        {
            _dofOverride.currentPreset = _dofOverride.targetPreset;
            _dofOverride.transitionActive = false;
        }
    }

    Apply_DOFPresetToCB(_dofOverride.currentPreset, _postProcessCB);

    const bool keepDOFEnabledDuringTransition = _dofOverride.transitionActive &&
        (_dofOverride.fromPreset.enableDOF || _dofOverride.targetPreset.enableDOF);
    const bool enableDOF = keepDOFEnabledDuringTransition ? true : _dofOverride.currentPreset.enableDOF;
    if (_dofForceDisabled)
    {
        Set_Flag(_featureFlag, PostProcessFeature::DOF, false);
        _postProcessCB.featureMask = To_Underlying(_featureFlag);
    }
    else
    {
        Set_Flag(_featureFlag, PostProcessFeature::DOF, enableDOF);
        _postProcessCB.featureMask = To_Underlying(_featureFlag);
    }

    if (!_dofOverride.transitionActive && _dofOverride.pendingScopeEnd)
        _dofOverride = DOFOverrideState{};
}

void PostProcessPipeline::Set_LightShaftParams(const LightShaftCB& lightShaftCB)
{
    _lightShaftCB = lightShaftCB;
}

const LightShaftCB& PostProcessPipeline::Get_LightShaftParams() const
{
    return _lightShaftCB;
}

LightShaftCB& PostProcessPipeline::Get_LightShaftParams()
{
    return _lightShaftCB;
}

PostProcessFeature PostProcessPipeline::Get_PostProcessFeatures() const
{
    return _featureFlag;
}

Shared<PostProcessPipeline> PostProcessPipeline::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    Shared<PostProcessPipeline> instance = make_shared<PostProcessPipeline>(device, context);

    if (FAILED(instance->Initialize(
        PostProcessFeature::HDR |
        PostProcessFeature::Exposure |
        PostProcessFeature::ToneMapping |
        PostProcessFeature::Bloom |
        PostProcessFeature::Gamma |
        PostProcessFeature::LightShaft |
        PostProcessFeature::DOF |
        PostProcessFeature::ColorGrade)))
    {
        MSG_BOX("Failed to Create : PostProcessPipeline");
        return nullptr;
    }

    return instance;
}

void PostProcessPipeline::Free()
{
}

NS_END
