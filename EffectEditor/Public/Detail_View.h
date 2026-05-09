#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Detail_View final : public Editor_Window
{
public:
    // 선택된 오브젝트와 컴포넌트 속성을 보여줄 인스펙터 창을 생성한다.
    Detail_View();
    ~Detail_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override; // 초기 속성 검사 영역의 골격을 그린다.

public:
    static Shared<Detail_View> Create();
    void Free() override;
};

NS_END
