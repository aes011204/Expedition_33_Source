#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"
#include <string>

NS_BEGIN(Engine)

struct PostProcessCB;

enum class DOFTransitionEasing : uint8
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    EaseInStrong,
    EaseOutStrong,
    EaseInOutStrong,
    EaseInCubic,
    EaseOutCubic,
    EaseInExpo,
    EaseOutExpo,
    EaseOutBack
};

struct DOFTransitionDesc
{
    DOFTransitionEasing easing = DOFTransitionEasing::EaseInOut;
    float duration = 0.35f;
};

// 하나의 완성된 DOF 설정 저장
struct DOFPreset
{
    bool enableDOF = true;

    float focusDistance = 3.f;      // 카메라로부터 초점 중심까지의 거리
    float focusRange = 1.5f;        // focusDistance를 중심으로 선명하게 유지할 거리 범위

    // 흐려짐의 정도. 블러 크기
    float nearMaxRadius = 0.55f;
    float farMaxRadius = 0.85f;

    float bokehThreshold = 1.5f;
    float bokehIntensity = 4.f;
    float depthSharpness = 0.01f;
    float cocEpsilon = 0.01f;

    // 블러가 얼마나 강하게 보이는지
    float nearIntensity = 0.85f;
    float farIntensity = 2.f;
    uint32 sampleCount = 12;
    uint32 debugMode = 0;
};

class ENGINE_DLL PostProcessPipeline : public Base
{
public:
    explicit PostProcessPipeline(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~PostProcessPipeline() = default;

public:
    HRESULT Initialize(PostProcessFeature initialFlag);

    void Set_PostProcessFeatures(PostProcessFeature featureFlag);
    void Set_PostProcessFeature(PostProcessFeature featureFlag, bool enable);
    bool Has_PostProcessFeature(PostProcessFeature featureFlag) const;

    void Set_PostProcessParams(const PostProcessCB& postProcessCB);
    const PostProcessCB& Get_PostProcessParams() const;
    PostProcessCB& Get_PostProcessParams();
    void Set_LightShaftParams(const LightShaftCB& lightShaftCB);
    const LightShaftCB& Get_LightShaftParams() const;
    LightShaftCB& Get_LightShaftParams();
    HRESULT Begin_DOFOverrideScope(const string& ownerTag);
    HRESULT Set_DOFOverridePreset(const string& ownerTag, const DOFPreset& preset, const DOFTransitionDesc& transition);
    HRESULT Set_DOFOverrideFocus(const string& ownerTag, float focusDistance, float focusRange);
    HRESULT Clear_DOFOverrideToBaseline(const string& ownerTag, const DOFTransitionDesc& transition);
    HRESULT End_DOFOverrideScope(const string& ownerTag, const DOFTransitionDesc& transition);
    void Update(float timeDelta);

    PostProcessFeature Get_PostProcessFeatures() const;

private:
    struct DOFOverrideState
    {
        bool scopeActive = false;
        bool pendingScopeEnd = false;
        bool baselineDOFEnabled = false;
        bool transitionActive = false;
        float elapsed = 0.f;
        float duration = 0.35f;
        DOFTransitionEasing easing = DOFTransitionEasing::EaseInOut;
        string ownerTag{};
        DOFPreset baselinePreset{};
        DOFPreset fromPreset{};
        DOFPreset currentPreset{};
        DOFPreset targetPreset{};
    };

    ComPtr<Device>              _device;
    ComPtr<Context>             _context;

    PostProcessFeature          _featureFlag = PostProcessFeature::None;
    PostProcessCB               _postProcessCB{};
    LightShaftCB                _lightShaftCB{};
    DOFOverrideState            _dofOverride{};
    bool                        _dofForceDisabled = false;

public:
    static Shared<PostProcessPipeline> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;
};

NS_END
