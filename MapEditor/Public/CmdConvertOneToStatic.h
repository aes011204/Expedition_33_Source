#pragma once
#include "IEditorCommand.h"
#include "EditorObject.h"

NS_BEGIN(MapEditor)

class CmdConvertOneToStatic : public IEditorCommand
{
public:
    explicit CmdConvertOneToStatic() = default;
    virtual ~CmdConvertOneToStatic() = default;

private:
    Shared<EditorObject> _originalInstanced;
    Shared<EditorObject> _createdStatic;
    InstanceTransform _savedTransform;
    int32 _instanceIdx = -1;
    bool _removedGroup = false;

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdConvertOneToStatic> Create(Shared<EditorObject> instancedObj, int32 instanceIdx);
};

NS_END
