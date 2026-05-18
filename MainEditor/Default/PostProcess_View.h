#pragma once

#include "Editor_Window.h"
#include "PostProcessPipeline.h"

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
    void    Draw_QuickAction(PostProcessCB& params, bool& dirty);
    void    Draw_StatusBadge();

private:
    static PostProcessCB Make_DefaultParams();

public:
    static Shared<PostProcess_View> Create();
    void   Free() override;
};

NS_END
