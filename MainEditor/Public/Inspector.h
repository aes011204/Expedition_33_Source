#pragma once

#include "EditorWindow.h"

NS_BEGIN(MainEditor)

class Inspector final : public EditorWindow
{
public:
    /* 선택된 오브젝트와 컴포넌트 속성을 보여줄 인스펙터 창을 생성한다. */
    Inspector();
    virtual ~Inspector() = default;

public:
    /* 초기 속성 검사 영역의 골격을 그린다. */
    virtual void Render() override;

public:
    static Shared<Inspector> Create();

};

NS_END
