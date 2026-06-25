#pragma once
#include "IEditorCommand.h"

NS_BEGIN(MapEditor)

class CmdEraseInstances final : public IEditorCommand
{
public:
    explicit CmdEraseInstances() = default;
    virtual ~CmdEraseInstances() = default;

public:
    HRESULT Initialize(Shared<EditorObject> obj, float radius, const Vec3& center);

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdEraseInstances> Create(Shared<EditorObject> obj, float radius, const Vec3& center);

private:
    struct RemovedInstance
    {
        int32 index = -1;
        InstanceTransform transform = {};
    };

private:
    Shared<EditorObject> _obj = {};
    vector<RemovedInstance> _removedInstances = {};
};

NS_END
