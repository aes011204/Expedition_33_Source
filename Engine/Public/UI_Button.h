#pragma once

#include "UI_Panel.h"

NS_BEGIN(Engine)

class ENGINE_DLL UI_Button final : public UI_Panel
{
public:
    struct UI_BUTTON_DESC : public UI_Panel::UI_PANEL_DESC
    {
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
    bool Is_Hovered() const { return _isHovered; }
    bool Is_Pressed() const { return _isPressed; }
    bool Was_Clicked() const { return _wasClicked; }

protected:
    Vec4 Resolve_RenderColor() const override;

private:
    bool Hit_TestMouse() const;
    static Vec4 Scale_RGB(const Vec4& color, float scale);

private:
    bool _isHovered{ false };
    bool _isPressed{ false };
    bool _wasClicked{ false };

public:
    static Shared<UI_Button> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END
