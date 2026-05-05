#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

/// @class Game_View
/// @brief 에디터가 재생 모드일 때 런타임 화면을 보여줄 게임 뷰 창을 생성한다.

class Game_View final : public Editor_Window
{
public:
    Game_View();
    ~Game_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override; // 런타임 렌더 타겟이 연결되기 전까지 사용할 게임 뷰 임시 화면을 그린다.

public:
    static Shared<Game_View> Create();
    void Free() override;
};

NS_END
