#pragma once

#include "Base.h"
#include "GameplayUI_Types.h"

NS_BEGIN(Client)

class GamePlayMenuFade final : public Base
{
public:
    GamePlayMenuFade() = default;
    ~GamePlayMenuFade() override = default;

public:
    bool Request_MenuTransition(
        GameplayMenu From,
        GameplayMenu To,
        const function<void()>& ApplyMenuChange) const;

    bool Should_UseFade(GameplayMenu From, GameplayMenu To) const;
};

NS_END
