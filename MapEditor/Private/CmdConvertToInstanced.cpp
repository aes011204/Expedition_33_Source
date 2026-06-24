#include "CmdConvertToInstanced.h"
#include "SceneContext.h"

void CmdConvertToInstanced::Execute(SceneContext& ctx)
{
    ctx.Remove(_originalStatic);

    for (auto& obj : ctx._instancedMeshes)
    {
        if (obj->_instancedModelPath == _originalStatic->_modelPath)
        {
            InstanceTransform t{};
            t._position = _originalStatic->_position;
            t._rotation = Vec4(_originalStatic->_rotation.x, _originalStatic->_rotation.y,
                _originalStatic->_rotation.z, _originalStatic->_rotation.w);
            t._scale = _originalStatic->_scale;

            _addedInstanceIdx = static_cast<int32>(obj->_instancedTransforms.size());
            obj->_instancedTransforms.push_back(t);
            obj->_instancedParamsDirty = true;

            _targetInstanced = obj;
            _createdNew = false;
            ctx._dirtyRenderHandles = true;
            ctx._selected = obj;
            return;
        }
    }

    auto newObj = make_shared<EditorObject>();
    newObj->_type = EObjectType::InstancedMesh;
    newObj->_instancedModelPath = _originalStatic->_modelPath;
    newObj->_name = fs::path(_originalStatic->_modelPath).stem().wstring();
    newObj->_position = Vec3::Zero;
    newObj->_scale = Vec3(1.f, 1.f, 1.f);
    newObj->_rotation = Quat::Identity;

    InstanceTransform t{};
    t._position = _originalStatic->_position;
    t._rotation = Vec4(_originalStatic->_rotation.x, _originalStatic->_rotation.y,
        _originalStatic->_rotation.z, _originalStatic->_rotation.w);
    t._scale = _originalStatic->_scale;
    newObj->_instancedTransforms.push_back(t);
    newObj->_instancedParamsDirty = true;
    newObj->UpdateWorldMatrix();

    _addedInstanceIdx = 0;
    _targetInstanced = newObj;
    _createdNew = true;

    ctx.Add(newObj);
    ctx._dirtyRenderHandles = true;
    ctx._selected = newObj;
}

void CmdConvertToInstanced::Undo(SceneContext& ctx)
{
    if (!_targetInstanced)
    {
        return;
    }

    if (_createdNew)
    {
        ctx.Remove(_targetInstanced);
    }
    else
    {
        if (_addedInstanceIdx >= 0 &&
            _addedInstanceIdx < static_cast<int32>(_targetInstanced->_instancedTransforms.size()))
        {
            _targetInstanced->_instancedTransforms.erase(
                _targetInstanced->_instancedTransforms.begin() + _addedInstanceIdx);
            _targetInstanced->_instancedParamsDirty = true;
        }
    }

    ctx.Add(_originalStatic);
    ctx._dirtyRenderHandles = true;
    ctx._selected = _originalStatic;
}

Shared<CmdConvertToInstanced> CmdConvertToInstanced::Create(Shared<EditorObject> staticObj)
{
    if (!staticObj || staticObj->_type != EObjectType::StaticMesh)
    {
        return nullptr;
    }

    auto cmd = make_shared<CmdConvertToInstanced>();
    cmd->_originalStatic = staticObj;
    return cmd;
}
