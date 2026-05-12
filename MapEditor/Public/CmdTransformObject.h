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

private:
    Shared<EditorObject> _obj;
    Vec3 _oldPos = {};
    Vec3 _newPos = {};
    Quat _oldRot = {};
    Quat _newRot = {};
    Vec3 _oldScale = {};
    Vec3 _newScale = {};

public:
    void Execute(SceneContext& ctx) override;
    void Undo(SceneContext& ctx) override;

public:
    static Shared<CmdTransformObject> Create(Shared<EditorObject> obj, Vec3 oldPos, Quat oldRot, Vec3 oldScale, Vec3 newPos, Quat newRot, Vec3 newScale);
};

NS_END
