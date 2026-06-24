#pragma once
#include "IEditorCommand.h"

NS_BEGIN(MapEditor)

class CmdAddInstance final : public IEditorCommand
{
public:
    explicit CmdAddInstance() = default;
    virtual ~CmdAddInstance() = default;

public:
    HRESULT Initialize(Shared<EditorObject> obj, const InstanceTransform& transform, int32 insertIndex);

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdAddInstance> Create(Shared<EditorObject> obj, const InstanceTransform& transform, int32 insertIndex);

private:
    Shared<EditorObject> _obj = {};
    InstanceTransform _transform = {};
    int32 _insertIndex = -1;
};

NS_END
