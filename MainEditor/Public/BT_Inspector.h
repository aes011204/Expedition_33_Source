#pragma once
#include "Editor_Window.h"
NS_BEGIN(MainEditor)

class BT_Inspector :
    public Editor_Window
{
public:
    BT_Inspector();
    virtual ~BT_Inspector();


public:

    static Shared<BT_Inspector> Create();
    virtual void Free();
};

NS_END
