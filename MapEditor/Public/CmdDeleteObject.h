#pragma once
#include "IEditorCommand.h"

NS_BEGIN(MapEditor)

class CmdDeleteObject : public IEditorCommand
{
public:
    explicit CmdDeleteObject() = default;
    virtual ~CmdDeleteObject() = default;

public:
    HRESULT Initialize(Shared<EditorObject> obj, int index);

private:
    Shared<EditorObject> _obj;
    int _insertIndex = {};

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdDeleteObject> Create(Shared<EditorObject> obj, int index);
};

NS_END
