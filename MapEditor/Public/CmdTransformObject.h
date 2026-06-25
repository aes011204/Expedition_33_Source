#pragma once
#include "IEditorCommand.h"

NS_BEGIN(MapEditor)

class CmdTransformObject : public IEditorCommand
{
public:
    explicit CmdTransformObject() = default;
    virtual ~CmdTransformObject() = default;

public:
    HRESULT Initialize(Shared<EditorObject> obj
    , Vec3 oldPos, Quat oldRot, Vec3 oldScale,
        Vec3 newPos, Quat newRot, Vec3 newScale);
    HRESULT Initialize(Shared<EditorObject> obj, int32 instanceIndex,
        const InstanceTransform& oldTransform, const InstanceTransform& newTransform);

private:
    Shared<EditorObject> _obj;
    int32 _instanceIndex = -1;
    Vec3 _oldPos = {};
    Vec3 _newPos = {};
    Quat _oldRot = {};
    Quat _newRot = {};
    Vec3 _oldScale = {};
    Vec3 _newScale = {};
    InstanceTransform _oldInstanceTransform = {};
    InstanceTransform _newInstanceTransform = {};

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdTransformObject> Create(Shared<EditorObject> obj, Vec3 oldPos, Quat oldRot, Vec3 oldScale, Vec3 newPos, Quat newRot, Vec3 newScale);
    static Shared<CmdTransformObject> Create(Shared<EditorObject> obj, int32 instanceIndex,
        const InstanceTransform& oldTransform, const InstanceTransform& newTransform);
};

NS_END
