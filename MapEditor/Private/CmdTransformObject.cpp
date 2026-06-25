#include "CmdTransformObject.h"

HRESULT CmdTransformObject::Initialize(Shared<EditorObject> obj, Vec3 oldPos, Quat oldRot, Vec3 oldScale, Vec3 newPos, Quat newRot, Vec3 newScale)
{
    if (!obj)
    {
        return E_FAIL;
    }

    _obj = obj;
    _instanceIndex = -1;

    _oldPos = oldPos;
    _oldRot = oldRot;
    _oldScale = oldScale;

    _newPos = newPos;
    _newRot = newRot;
    _newScale = newScale;

    return S_OK;
}

HRESULT CmdTransformObject::Initialize(Shared<EditorObject> obj, int32 instanceIndex,
    const InstanceTransform& oldTransform, const InstanceTransform& newTransform)
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
    _oldInstanceTransform = oldTransform;
    _newInstanceTransform = newTransform;

    return S_OK;
}

void CmdTransformObject::Execute(SceneContext& ctx)
{
    if (_instanceIndex >= 0)
    {
        if (!_obj || _obj->_type != EObjectType::InstancedMesh ||
            _instanceIndex >= static_cast<int32>(_obj->_instancedTransforms.size()))
        {
            return;
        }

        _obj->_instancedTransforms[_instanceIndex] = _newInstanceTransform;
        _obj->_instancedParamsDirty = true;
        _obj->_selectedInstanceIdx = _instanceIndex;
        ctx._dirtyRenderHandles = true;
        ctx._octreeDirty = true;
        ctx._selected = _obj;
        return;
    }

    _obj->_position = _newPos;
    _obj->_rotation = _newRot;
    _obj->_scale = _newScale;
    _obj->UpdateWorldMatrix();
    ctx._selected = _obj;
    if (_obj->_type == EObjectType::Light)
    {
        ctx._lightDirty = true;
    }
    if (_obj->_type == EObjectType::StaticMesh)
    {
        ctx._octreeDirty = true;
    }
}

void CmdTransformObject::Undo(SceneContext& ctx)
{
    if (_instanceIndex >= 0)
    {
        if (!_obj || _obj->_type != EObjectType::InstancedMesh ||
            _instanceIndex >= static_cast<int32>(_obj->_instancedTransforms.size()))
        {
            return;
        }

        _obj->_instancedTransforms[_instanceIndex] = _oldInstanceTransform;
        _obj->_instancedParamsDirty = true;
        _obj->_selectedInstanceIdx = _instanceIndex;
        ctx._dirtyRenderHandles = true;
        ctx._octreeDirty = true;
        ctx._selected = _obj;
        return;
    }

    _obj->_position = _oldPos;
    _obj->_rotation = _oldRot;
    _obj->_scale = _oldScale;
    _obj->UpdateWorldMatrix();

    ctx._selected = _obj;
    if (_obj->_type == EObjectType::Light)
    {
        ctx._lightDirty = true;
    }
    if (_obj->_type == EObjectType::StaticMesh)
    {
        ctx._octreeDirty = true;
    }
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

Shared<CmdTransformObject> CmdTransformObject::Create(Shared<EditorObject> obj, int32 instanceIndex,
    const InstanceTransform& oldTransform, const InstanceTransform& newTransform)
{
    auto instance = make_shared<CmdTransformObject>();
    if (FAILED(instance->Initialize(obj, instanceIndex, oldTransform, newTransform)))
    {
        return nullptr;
    }

    return instance;
}
