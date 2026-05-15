#pragma once

#include "Editor_Window.h"

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
    enum class ToneMappingType : int32
    {
        Reinhard = 0,
        ACES,
        Linear,
        END
    };

    struct POSTPROCESS_SETTING
    {
        bool    useHDR = true;
        bool    useToneMapping = true;
        int32   toneMappingType = ETOI(ToneMappingType::ACES);
        float   exposure = 1.f;
        float   gamma = 2.2f;
        bool    showPreview = true;
    };

private:
    void    Draw_HDRControl();
    void    Draw_ToneMappingControl();
    void    Draw_Preview();
    void    Draw_StatusBadge();

private:
    float   Apply_ToneMapping(float value) const;
    float   Apply_Gamma(float value) const;
    float   Saturate01(float value) const;

private:
    POSTPROCESS_SETTING _setting{};
    float               _previewTime = 0.f;

public:
    static Shared<PostProcess_View> Create();
    void   Free() override;
};

NS_END
