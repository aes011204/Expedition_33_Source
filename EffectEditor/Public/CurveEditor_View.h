#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class CurveEditor_View final : public Editor_Window
{
public:
    CurveEditor_View();
    ~CurveEditor_View() override = default;

public:
    void Render() override;

public:
    static Shared<CurveEditor_View> Create();
};

NS_END
