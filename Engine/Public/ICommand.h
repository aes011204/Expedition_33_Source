#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL ICommand : public Base
{
public:
    ICommand() = default;
    virtual ~ICommand() = default;
    
public:
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual void Redo() = 0;

    virtual string Get_Description() const { return "Unknown"; }

public:
    void Free() override {}

};

NS_END
