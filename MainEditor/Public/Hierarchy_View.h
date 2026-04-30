#pragma once

#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Hierarchy_View final : public Editor_Window
{
public:
    /* 이후 Object_Manager 레이어를 표시할 씬 하이어라키 창을 생성한다. */
    Hierarchy_View();
    virtual ~Hierarchy_View() = default;

public:
    /* 초기 하이어라키 트리 골격을 그린다. */
    virtual void Render() override;

public:
    static Shared<Hierarchy_View> Create();

};

NS_END
