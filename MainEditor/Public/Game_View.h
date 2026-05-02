#pragma once

#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Game_View final : public Editor_Window
{
public:
    /* 에디터가 재생 모드일 때 런타임 화면을 보여줄 게임 뷰 창을 생성한다. */
    Game_View();
    virtual ~Game_View() = default;

public:
    /* 런타임 렌더 타겟이 연결되기 전까지 사용할 게임 뷰 임시 화면을 그린다. */
    virtual void Render() override;

public:
    static Shared<Game_View> Create();

};

NS_END
