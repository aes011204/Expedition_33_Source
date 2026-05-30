#pragma once

#include "UI_Panel.h"

NS_BEGIN(Engine)

class ENGINE_DLL UI_Button final : public UI_Panel
{
public:
    struct UI_BUTTON_DESC : public UI_Panel::UI_PANEL_DESC
    {
        UI_StateAnimationProfile stateAnimationProfile{};
        bool isFocused = false;
        bool isDisabled = false;
    };

public:
    UI_Button();
    UI_Button(const UI_Button& prototype);
    ~UI_Button() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

public:
   // void Enable_HoveredAnim(Vec2 targetScale) {_enableHoveredAnim = true; _targetScale = targetScale; }
    bool Is_Hovered() const { return _isHovered; }
    bool Is_Pressed() const { return _isPressed; }
    bool Was_Clicked() const { return _wasClicked; }
    bool Is_Focused() const { return _isFocusedForState; }
    bool Is_Disabled() const { return _isDisabled; }
    void Set_Focused(bool isFocused);
    void Set_Disabled(bool isDisabled);
    void Set_StateAnimationProfile(const UI_StateAnimationProfile& stateAnimationProfile);

protected:
    Vec4 Resolve_RenderColor() const override;

private:
    bool Hit_TestMouse() const;
    void Update_StateAnimationEvents();
    bool Try_PlayStateAnimation(UI_StateAnimationEvent eventType);
    static Vec4 Scale_RGB(const Vec4& color, float scale);

private:
    bool _isHovered { false };
    bool _isPressed { false };
    bool _wasClicked { false };
    bool _isFocusedForState { false };
    bool _isDisabled { false };
    bool _previousHoveredForState { false };
    bool _previousPressedForState { false };
    bool _previousFocusedForState { false };
    bool _previousDisabledForState { false };
    UI_StateAnimationProfile _stateAnimationProfile = {};

public:
    static Shared<UI_Button> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END
