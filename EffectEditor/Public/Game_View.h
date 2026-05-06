#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Game_View final : public Editor_Window
{
public:
    Game_View();
    ~Game_View() override = default;

public:
    void Render() override;

public:
    static Shared<Game_View> Create();
};

NS_END
