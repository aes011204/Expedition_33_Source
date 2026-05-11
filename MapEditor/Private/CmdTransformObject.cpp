#include "CmdTransformObject.h"

HRESULT CmdTransformObject::Initialize(Shared<EditorObject> obj, Vec3 oldPos, Quat oldRot, Vec3 oldScale, Vec3 newPos, Quat newRot, Vec3 newScale)
{
    if (!obj)
    {
        return E_FAIL;
    }

    _obj = obj;

    _oldPos = oldPos;
    _oldRot = oldRot;
    _oldScale = oldScale;

    _newPos = newPos;
    _newRot = newRot;
    _newScale = newScale;

    return S_OK;
}

void CmdTransformObject::Execute(SceneContext& ctx)
{
    _obj->_position = _newPos;
    _obj->_rotation = _newRot;
    _obj->_scale = _newScale;
    _obj->UpdateWorldMatrix();
}

void CmdTransformObject::Undo(SceneContext& ctx)
{
    _obj->_position = _oldPos;
    _obj->_rotation = _oldRot;
    _obj->_scale = _oldScale;
    _obj->UpdateWorldMatrix();

    ctx._selected = _obj;
}

Shared<CmdTransformObject> CmdTransformObject::Create(Shared<EditorObject> obj, Vec3 oldPos, Quat oldRot, Vec3 oldScale, Vec3 newPos, Quat newRot, Vec3 newScale)
{
    auto instance = make_shared<CmdTransformObject>();
    if (FAILED(instance->Initialize(obj, oldPos, oldRot, oldScale, newPos, newRot, newScale)))
    {                  
        return nullptr;
    }

    return instance;
}
