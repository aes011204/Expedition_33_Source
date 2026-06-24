#include "CmdConvertAllToStatic.h"
#include "SceneContext.h"

Shared<CmdConvertAllToStatic> CmdConvertAllToStatic::Create(Shared<EditorObject> instancedObj)
{
    if (!instancedObj || instancedObj->_type != EObjectType::InstancedMesh)
    {
        return nullptr;
    }

    auto cmd = make_shared<CmdConvertAllToStatic>();
    cmd->_originalInstanced = instancedObj;
    return cmd;
}

void CmdConvertAllToStatic::Execute(SceneContext& ctx)
{
    ctx.Remove(_originalInstanced);
    _createdStatics.clear();

    const wstring modelPath = _originalInstanced->_instancedModelPath;
    const wstring baseName = fs::path(modelPath).stem().wstring();

    for (int32 i = 0; i < static_cast<int32>(_originalInstanced->_instancedTransforms.size()); ++i)
    {
        const auto& t = _originalInstanced->_instancedTransforms[i];

        auto newObj = make_shared<EditorObject>();
        newObj->_type = EObjectType::StaticMesh;
        newObj->_modelPath = modelPath;
        newObj->_name = baseName + L"_" + to_wstring(i);
        newObj->_position = t._position;
        newObj->_scale = t._scale;
        newObj->_rotation = Quat(t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w);
        newObj->UpdateWorldMatrix();

        _createdStatics.push_back(newObj);
        ctx.Add(newObj);
    }

    ctx._dirtyRenderHandles = true;
    ctx._selected = _createdStatics.empty() ? nullptr : _createdStatics.front();
}

void CmdConvertAllToStatic::Undo(SceneContext& ctx)
{
    for (auto& obj : _createdStatics)
        ctx.Remove(obj);

    _createdStatics.clear();

    ctx.Add(_originalInstanced);
    ctx._dirtyRenderHandles = true;
    ctx._selected = _originalInstanced;
}
