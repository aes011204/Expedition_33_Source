#pragma once
#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Camera_Tool : public Editor_Window
{
public:
    Camera_Tool();
    virtual ~Camera_Tool() = default;

public:
    virtual void Render() override;

public:
    static Shared<Camera_Tool> Create();
};

NS_END
