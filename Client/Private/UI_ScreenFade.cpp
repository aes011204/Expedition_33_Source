#include "pch.h"
#include "UI_ScreenFade.h"

#include "GameInstance.h"
#include "UI_Panel.h"

namespace
{
    constexpr const wchar_t* ScreenFadePanelPrototypeTag = L"UI_ScreenFade_Panel";
}

HRESULT UI_ScreenFade::Initialize(float InitialAlpha, int32 SortOrder)
{
    return Ready_FadePanel(InitialAlpha, SortOrder);
}

void UI_ScreenFade::Update(float TimeDelta)
{
    UNREFERENCED_PARAMETER(TimeDelta);

    Refresh_ViewportSize();
}

void UI_ScreenFade::Set_FadeAlpha(float Alpha)
{
    _fadeAlpha = clamp(Alpha, 0.f, 1.f);
    Apply_Color();

    if (_fadePanel)
        Set_Visible(_fadeAlpha > 0.f);
}

void UI_ScreenFade::Set_FadeColor(const Vec4& Color)
{
    _fadeColor = Color;
    Apply_Color();
}

void UI_ScreenFade::Set_Visible(bool IsVisible)
{
    if (_fadePanel == nullptr)
        return;

    if (IsVisible)
        _fadePanel->Open();
    else
        _fadePanel->Close();
}

HRESULT UI_ScreenFade::Ready_FadePanel(float InitialAlpha, int32 SortOrder)
{
    const uint32 staticLevelIndex = ETOI(LevelType::Static);

    if (!GAME->Has_Prototype(staticLevelIndex, ScreenFadePanelPrototypeTag))
    {
        CHECK_FAILED(GAME->Add_Prototype(staticLevelIndex, ScreenFadePanelPrototypeTag, UI_Panel::Create()), E_FAIL);
    }

    const float viewportWidth = static_cast<float>(max(1u, GAME->Get_ViewportWidth()));
    const float viewportHeight = static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

    UI_Panel::UI_PANEL_DESC desc{};
    desc.transformDesc.position = Vec2(viewportWidth * 0.5f, viewportHeight * 0.5f);
    desc.transformDesc.size = Vec2(viewportWidth, viewportHeight);
    desc.transformDesc.scale = Vec2(1.f, 1.f);
    desc.transformDesc.pivot = Vec2(0.5f, 0.5f);
    desc.isOpenOnCreate = InitialAlpha > 0.f;
    desc.sortOrder = SortOrder;
    desc.renderGroup = UI_RenderGroup::Foreground;
    desc.color = Vec4(_fadeColor.x, _fadeColor.y, _fadeColor.z, clamp(InitialAlpha, 0.f, 1.f));
    desc.shaderPrototypeLevelIndex = staticLevelIndex;
    desc.viBufferPrototypeLevelIndex = staticLevelIndex;

    Shared<UI_Base> fadeBase{};
    CHECK_FAILED(GAME->Add_UI(staticLevelIndex, ScreenFadePanelPrototypeTag, &desc, &fadeBase), E_FAIL);

    _fadePanel = dynamic_pointer_cast<UI_Panel>(fadeBase);
    CHECK_NULL(_fadePanel, E_FAIL);

    Set_FadeAlpha(InitialAlpha);
    return S_OK;
}

void UI_ScreenFade::Refresh_ViewportSize()
{
    if (_fadePanel == nullptr || _fadePanel->Get_Transform() == nullptr)
        return;

    const float viewportWidth = static_cast<float>(max(1u, GAME->Get_ViewportWidth()));
    const float viewportHeight = static_cast<float>(max(1u, GAME->Get_ViewportHeight()));
    UI_Transform* transform = _fadePanel->Get_Transform();

    transform->Set_Position(Vec2(viewportWidth * 0.5f, viewportHeight * 0.5f));
    transform->Set_Size(Vec2(viewportWidth, viewportHeight));
}

void UI_ScreenFade::Apply_Color()
{
    if (_fadePanel == nullptr)
        return;

    _fadePanel->Set_Color(Vec4(_fadeColor.x, _fadeColor.y, _fadeColor.z, _fadeAlpha * _fadeColor.w));
}

Unique<UI_ScreenFade> UI_ScreenFade::Create(float InitialAlpha, int32 SortOrder)
{
    auto instance = make_unique<UI_ScreenFade>();

    if (FAILED(instance->Initialize(InitialAlpha, SortOrder)))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_ScreenFade::Free()
{
    if (_fadePanel)
    {
        _fadePanel->Close();
        _fadePanel.reset();
    }

    __super::Free();
}
