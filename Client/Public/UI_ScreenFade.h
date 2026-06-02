#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class UI_Panel;
NS_END

NS_BEGIN(Client)

class UI_ScreenFade final : public Base
{
public:
    UI_ScreenFade() = default;
    ~UI_ScreenFade() override = default;

public:
    HRESULT Initialize(float InitialAlpha = 0.f, int32 SortOrder = 1000000);
    void    Update(float TimeDelta);

    void    Set_FadeAlpha(float Alpha);
    float   Get_FadeAlpha() const { return _fadeAlpha; }

    void    Set_FadeColor(const Vec4& Color);
    void    Set_Visible(bool IsVisible);
    bool    Is_Ready() const { return _fadePanel != nullptr; }

private:
    HRESULT Ready_FadePanel(float InitialAlpha, int32 SortOrder);
    void    Refresh_ViewportSize();
    void    Apply_Color();

private:
    Shared<UI_Panel> _fadePanel{};
    Vec4             _fadeColor{ 0.f, 0.f, 0.f, 1.f };
    float            _fadeAlpha = 0.f;

public:
    static Unique<UI_ScreenFade> Create(float InitialAlpha = 0.f, int32 SortOrder = 1000000);
    void Free() override;
};

NS_END
