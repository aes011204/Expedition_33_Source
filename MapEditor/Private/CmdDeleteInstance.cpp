#include "pch.h"
#include "CmdDeleteInstance.h"

HRESULT CmdDeleteInstance::Initialize(Shared<EditorObject> obj, int32 instanceIndex)
{
    if (!obj || obj->_type != EObjectType::InstancedMesh)
    {
        return E_FAIL;
    }

    const int32 instanceCount = static_cast<int32>(obj->_instancedTransforms.size());
    if (instanceIndex < 0 || instanceIndex >= instanceCount)
    {
        return E_FAIL;
    }

    _obj = obj;
    _instanceIndex = instanceIndex;
    _deletedTransform = obj->_instancedTransforms[instanceIndex];
    return S_OK;
}

void CmdDeleteInstance::Execute(SceneContext& ctx)
{
    UNREFERENCED_PARAMETER(ctx);

    if (!_obj || _obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    const int32 instanceCount = static_cast<int32>(_obj->_instancedTransforms.size());
    if (_instanceIndex < 0 || _instanceIndex >= instanceCount)
    {
        return;
    }

    _obj->_instancedTransforms.erase(_obj->_instancedTransforms.begin() + _instanceIndex);
    _obj->_instancedParamsDirty = true;
    _obj->_selectedInstanceIdx = -1;
    ctx._dirtyRenderHandles = true;
    ctx._octreeDirty = true;
    ctx._selected = _obj;
}

void CmdDeleteInstance::Undo(SceneContext& ctx)
{
    if (!_obj || _obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    const int32 clampedIndex = std::clamp(_instanceIndex, 0, static_cast<int32>(_obj->_instancedTransforms.size()));
    _obj->_instancedTransforms.insert(_obj->_instancedTransforms.begin() + clampedIndex, _deletedTransform);
    _obj->_instancedParamsDirty = true;
    _obj->_selectedInstanceIdx = clampedIndex;
    ctx._dirtyRenderHandles = true;
    ctx._octreeDirty = true;
    ctx._selected = _obj;
}

Shared<CmdDeleteInstance> CmdDeleteInstance::Create(Shared<EditorObject> obj, int32 instanceIndex)
{
    auto instance = make_shared<CmdDeleteInstance>();
    if (FAILED(instance->Initialize(obj, instanceIndex)))
    {
        return nullptr;
    }

    return instance;
}
