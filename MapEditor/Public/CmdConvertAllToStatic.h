#pragma once
#include "IEditorCommand.h"
#include "EditorObject.h"

NS_BEGIN(MapEditor)

class CmdConvertAllToStatic : public IEditorCommand
{
public:
    explicit CmdConvertAllToStatic() = default;
    virtual ~CmdConvertAllToStatic() = default;

private:
    Shared<EditorObject> _originalInstanced;
    vector<Shared<EditorObject>> _createdStatics;

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdConvertAllToStatic> Create(Shared<EditorObject> instancedObj);
};

NS_END
