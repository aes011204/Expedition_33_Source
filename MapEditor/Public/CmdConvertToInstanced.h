#pragma once
#include "IEditorCommand.h"
#include "EditorObject.h"

NS_BEGIN(MapEditor)

class CmdConvertToInstanced : public IEditorCommand
{
public:
    explicit CmdConvertToInstanced() = default;
    virtual ~CmdConvertToInstanced() = default;

private:
    Shared<EditorObject> _originalStatic;
    Shared<EditorObject> _targetInstanced;
    int32 _addedInstanceIdx = -1;
    bool _createdNew = false;

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdConvertToInstanced> Create(Shared<EditorObject> staticObj);

};

NS_END
