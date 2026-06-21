#include "pch.h"
#include "CmdAddInstance.h"

HRESULT CmdAddInstance::Initialize(Shared<EditorObject> obj, const InstanceTransform& transform, int32 insertIndex)
{
    if (!obj || obj->_type != EObjectType::InstancedMesh)
    {
        return E_FAIL;
    }

    _obj = obj;
    _transform = transform;
    _insertIndex = insertIndex;
    return S_OK;
}

void CmdAddInstance::Execute(SceneContext& ctx)
{
    UNREFERENCED_PARAMETER(ctx);

    if (!_obj || _obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    const int32 clampedIndex = std::clamp(_insertIndex, 0, static_cast<int32>(_obj->_instancedTransforms.size()));
    _obj->_instancedTransforms.insert(_obj->_instancedTransforms.begin() + clampedIndex, _transform);
    _obj->_instancedParamsDirty = true;
    _obj->_selectedInstanceIdx = clampedIndex;
    ctx._dirtyRenderHandles = true;
    ctx._octreeDirty = true;
    ctx._selected = _obj;
}

void CmdAddInstance::Undo(SceneContext& ctx)
{
    if (!_obj || _obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    const int32 instanceCount = static_cast<int32>(_obj->_instancedTransforms.size());
    if (_insertIndex >= 0 && _insertIndex < instanceCount)
    {
        _obj->_instancedTransforms.erase(_obj->_instancedTransforms.begin() + _insertIndex);
    }

    _obj->_instancedParamsDirty = true;
    if (_obj->_instancedTransforms.empty())
    {
        _obj->_selectedInstanceIdx = -1;
    }
    else
    {
        _obj->_selectedInstanceIdx = std::clamp(_insertIndex - 1, 0, static_cast<int32>(_obj->_instancedTransforms.size()) - 1);
    }

    ctx._dirtyRenderHandles = true;
    ctx._octreeDirty = true;
    ctx._selected = _obj;
}

Shared<CmdAddInstance> CmdAddInstance::Create(Shared<EditorObject> obj, const InstanceTransform& transform, int32 insertIndex)
{
    auto instance = make_shared<CmdAddInstance>();
    if (FAILED(instance->Initialize(obj, transform, insertIndex)))
    {
        return nullptr;
    }

    return instance;
}
