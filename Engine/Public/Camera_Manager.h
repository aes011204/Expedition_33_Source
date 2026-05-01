#pragma once
#include "Base.h"

NS_BEGIN(Engine)
class Camera_Manager : public Base
{
public:
    Camera_Manager();
    virtual ~Camera_Manager();

public:
    static Unique<Camera_Manager> Create();
    virtual void Free() override;
};
NS_END
