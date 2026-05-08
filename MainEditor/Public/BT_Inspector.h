#pragma once
#include "Base.h"

NS_BEGIN(MainEditor)

struct EditorNode;


class BT_Inspector :
    public Base
{
public:
    BT_Inspector() ;
    virtual ~BT_Inspector();

public:
    void Draw_Inspector(EditorNode* node);

public:
    static Shared<BT_Inspector> Create();
    void Free() override;
};

NS_END
