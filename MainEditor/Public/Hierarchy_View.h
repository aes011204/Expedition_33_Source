#pragma once

#include "EditorWindow.h"

NS_BEGIN(MainEditor)

class Hierarchy_View final : public EditorWindow
{
public:
    /* 이후 Object_Manager 레이어를 표시할 씬 하이어라키 창을 생성한다. */
    Hierarchy_View();
    virtual ~Hierarchy_View() = default;

public:
    /* 초기 하이어라키 트리 골격을 그린다. */
    virtual void Render() override;
};

NS_END
