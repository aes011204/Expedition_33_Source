#include "CmdDeleteObject.h"

HRESULT CmdDeleteObject::Initialize(Shared<EditorObject> obj, int index)
{
    if (!obj)
    {
        return E_FAIL;
    }

    _obj = obj;
    _insertIndex = index;

    return S_OK;
}

void CmdDeleteObject::Execute(SceneContext& ctx)
{
    ctx.Remove(_obj);
}

void CmdDeleteObject::Undo(SceneContext& ctx)
{
    int clampedIndex = min(_insertIndex, static_cast<int>(ctx._objects.size()));
    ctx._objects.insert(ctx._objects.begin() + clampedIndex, _obj);

    if (_obj->_type == EObjectType::StaticMesh)
    {
        ctx._staticMeshes.push_back(_obj);
    }
    else if (_obj->_type == EObjectType::InstancedMesh)
    {
        ctx._instancedMeshes.push_back(_obj);
    }
    else if (_obj->_type == EObjectType::Light)
    {
        ctx._lights.push_back(_obj);
    }

    ctx._dirtyRenderHandles = true;
    ctx._octreeDirty = true;
    ctx._selected = _obj;
}

Shared<CmdDeleteObject> CmdDeleteObject::Create(Shared<EditorObject> obj, int index)
{
    auto instance = make_shared<CmdDeleteObject>();
    if (FAILED(instance->Initialize(obj, index)))
    {
        return nullptr;
    }

    return instance;
}

