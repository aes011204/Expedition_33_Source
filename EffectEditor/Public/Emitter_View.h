#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Emitter_View final : public Editor_Window
{
public:
    Emitter_View();
    ~Emitter_View() override = default;

public:
    void Render() override;

public:
    static Shared<Emitter_View> Create();
};

NS_END
