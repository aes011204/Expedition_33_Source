#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Inspector final : public Editor_Window
{
public:
    // 선택된 오브젝트와 컴포넌트 속성을 보여줄 인스펙터 창을 생성한다.
    Inspector();
    ~Inspector() override = default;

public:
    // 초기 속성 검사 영역의 골격을 그린다.
    void Render() override;

public:
    static Shared<Inspector> Create();
};

NS_END
