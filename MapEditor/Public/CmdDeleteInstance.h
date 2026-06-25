#pragma once
#include "IEditorCommand.h"

NS_BEGIN(MapEditor)

class CmdDeleteInstance final : public IEditorCommand
{
public:
    explicit CmdDeleteInstance() = default;
    virtual ~CmdDeleteInstance() = default;

public:
    HRESULT Initialize(Shared<EditorObject> obj, int32 instanceIndex);

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdDeleteInstance> Create(Shared<EditorObject> obj, int32 instanceIndex);

private:
    Shared<EditorObject> _obj = {};
    InstanceTransform _deletedTransform = {};
    int32 _instanceIndex = -1;
};

NS_END
