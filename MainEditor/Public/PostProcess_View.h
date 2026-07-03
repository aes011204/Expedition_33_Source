#pragma once

#include "Editor_Window.h"
#include "PostProcessPipeline.h"
#include "LightingPipeline.h"
#include <string>

NS_BEGIN(MainEditor)

class PostProcess_View final : public Editor_Window
{
public:
    PostProcess_View();
    virtual ~PostProcess_View() = default;

public:
    HRESULT Initialize() override;
    void    Update(float timeDelta) override;
    void    Pre_Render() override;
    void    Render() override;

private:
    void    Draw_FeatureControl(Shared<PostProcessPipeline> pipeline, bool& dirty);
    void    Draw_ParameterControl(PostProcessCB& params, bool& dirty);
    void    Draw_LightShaftControl(LightShaftCB& params, bool& dirty);
    void    Draw_QuickAction(PostProcessCB& params, LightShaftCB& lightShaftParams, Shared<PostProcessPipeline> pipeline, Shared<LightingPipeline> lightingPipeline, bool& dirty, bool& lightingDirty);
    void    Draw_LightingControl(Shared<LightingPipeline> pipeline, bool& dirty);
    void    Draw_StatusBadge();
    void    Cache_DefaultSnapshots();

private:
    PostProcessCB        _defaultPostProcessParams{};
    LightShaftCB         _defaultLightShaftParams{};
    SSAOCB               _defaultSSAOParams{};
    DeferredLightingCB   _defaultDeferredLightingParams{};
    std::string _reloadShaderStatusText{};
    bool        _reloadShaderStatusIsError = false;
    std::string _reloadFroxelComputeStatusText{};
    bool        _reloadFroxelComputeStatusIsError = false;
    std::string _reloadShaderComStatusText{};
    bool        _reloadShaderComStatusIsError = false;
    bool        _hasDefaultSnapshots = false;

public:
    static Shared<PostProcess_View> Create();
    void   Free() override;
};

NS_END
