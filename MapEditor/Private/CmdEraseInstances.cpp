#include "pch.h"
#include "CmdEraseInstances.h"

HRESULT CmdEraseInstances::Initialize(Shared<EditorObject> obj, float radius, const Vec3& center)
{
    if (!obj || obj->_type != EObjectType::InstancedMesh)
    {
        return E_FAIL;
    }

    const float radiusSq = radius * radius;
    for (int32 i = 0; i < static_cast<int32>(obj->_instancedTransforms.size()); ++i)
    {
        const InstanceTransform& transform = obj->_instancedTransforms[i];
        const float dx = transform._position.x - center.x;
        const float dz = transform._position.z - center.z;
        if ((dx * dx + dz * dz) <= radiusSq)
        {
            RemovedInstance removed{};
            removed.index = i;
            removed.transform = transform;
            _removedInstances.push_back(removed);
        }
    }

    if (_removedInstances.empty())
    {
        return E_FAIL;
    }

    _obj = obj;
    return S_OK;
}

void CmdEraseInstances::Execute(SceneContext& ctx)
{
    UNREFERENCED_PARAMETER(ctx);

    if (!_obj || _obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    for (auto it = _removedInstances.rbegin(); it != _removedInstances.rend(); ++it)
    {
        if (it->index >= 0 && it->index < static_cast<int32>(_obj->_instancedTransforms.size()))
        {
            _obj->_instancedTransforms.erase(_obj->_instancedTransforms.begin() + it->index);
        }
    }

    _obj->_instancedParamsDirty = true;
    _obj->_selectedInstanceIdx = -1;
    ctx._dirtyRenderHandles = true;
    ctx._octreeDirty = true;
    ctx._selected = _obj;
}

void CmdEraseInstances::Undo(SceneContext& ctx)
{
    if (!_obj || _obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    int32 lastRestoredIndex = -1;
    for (const RemovedInstance& removed : _removedInstances)
    {
        const int32 clampedIndex = std::clamp(removed.index, 0, static_cast<int32>(_obj->_instancedTransforms.size()));
        _obj->_instancedTransforms.insert(_obj->_instancedTransforms.begin() + clampedIndex, removed.transform);
        lastRestoredIndex = clampedIndex;
    }

    _obj->_instancedParamsDirty = true;
    _obj->_selectedInstanceIdx = lastRestoredIndex;
    ctx._dirtyRenderHandles = true;
    ctx._octreeDirty = true;
    ctx._selected = _obj;
}

Shared<CmdEraseInstances> CmdEraseInstances::Create(Shared<EditorObject> obj, float radius, const Vec3& center)
{
    auto instance = make_shared<CmdEraseInstances>();
    if (FAILED(instance->Initialize(obj, radius, center)))
    {
        return nullptr;
    }

    return instance;
}
