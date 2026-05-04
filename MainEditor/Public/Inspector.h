#pragma once

#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Inspector final : public Editor_Window
{
public:
    Inspector();
    virtual ~Inspector() = default;

public:
    virtual void Render() override;

public:
    static Shared<Inspector> Create();

};

NS_END
