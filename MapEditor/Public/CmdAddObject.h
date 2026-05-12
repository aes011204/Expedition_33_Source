#pragma once
#include "IEditorCommand.h"

NS_BEGIN(MapEditor)

class CmdAddObject : public IEditorCommand
{
public:
    explicit CmdAddObject() = default;
    virtual ~CmdAddObject() = default;

public:
    HRESULT Initialize(Shared<EditorObject> obj);

private:
    Shared<EditorObject> _obj;

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdAddObject> Create(Shared<EditorObject> obj);
};

NS_END
