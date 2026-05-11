#pragma once
#include "SceneContext.h"

NS_BEGIN(MapEditor)

class IEditorCommand abstract
{
public:
    IEditorCommand() = default;
    virtual ~IEditorCommand() = default;

public:
    virtual void Execute(SceneContext& ctx) = 0;
    virtual void Undo(SceneContext& ctx) = 0;
};

NS_END
