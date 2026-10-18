#include "CmdConvertOneToStatic.h"
#include "SceneContext.h"

Shared<CmdConvertOneToStatic> CmdConvertOneToStatic::Create(Shared<EditorObject> instancedObj, int32 instanceIdx)
{
    if (!instancedObj || instancedObj->_type != EObjectType::InstancedMesh)
    {
        return nullptr;
    }
    if (instanceIdx < 0 || instanceIdx >= static_cast<int32>(instancedObj->_instancedTransforms.size()))
    {
        return nullptr;
    }

    auto cmd = make_shared<CmdConvertOneToStatic>();
    cmd->_originalInstanced = instancedObj;
    cmd->_instanceIdx = instanceIdx;
    return cmd;
}

void CmdConvertOneToStatic::Execute(SceneContext& ctx)
{
    auto& transforms = _originalInstanced->_instancedTransforms;
    _savedTransform = transforms[_instanceIdx];

    transforms.erase(transforms.begin() + _instanceIdx);
    _originalInstanced->_instancedParamsDirty = true;

    if (transforms.empty())
    {
        ctx.Remove(_originalInstanced);
        _removedGroup = true;
    }

    const wstring modelPath = _originalInstanced->_instancedModelPath;
    auto newObj = make_shared<EditorObject>();
    newObj->_type = EObjectType::StaticMesh;
    newObj->_modelPath = modelPath;
    newObj->_name = fs::path(modelPath).stem().wstring() + L"_extracted";
    newObj->_position = _savedTransform._position;
    newObj->_scale = _savedTransform._scale;
    newObj->_rotation = Quat(_savedTransform._rotation.x, _savedTransform._rotation.y,
        _savedTransform._rotation.z, _savedTransform._rotation.w);
    newObj->UpdateWorldMatrix();

    _createdStatic = newObj;
    ctx.Add(newObj);

    ctx._dirtyRenderHandles = true;
    ctx._selected = newObj;
}

void CmdConvertOneToStatic::Undo(SceneContext& ctx)
{
    ctx.Remove(_createdStatic);

    if (_removedGroup)
    {
        ctx.Add(_originalInstanced);
    }

    auto& transforms = _originalInstanced->_instancedTransforms;
    int32 insertIdx = min(_instanceIdx, static_cast<int32>(transforms.size()));
    transforms.insert(transforms.begin() + insertIdx, _savedTransform);
    _originalInstanced->_instancedParamsDirty = true;

    ctx._dirtyRenderHandles = true;
    ctx._selected = _originalInstanced;
    ctx._selected->_selectedInstanceIdx = _instanceIdx;
}
