#include "CmdAddObject.h"

HRESULT CmdAddObject::Initialize(Shared<EditorObject> obj)
{
    if (!obj)
    {
        return E_FAIL;
    }

    _obj = obj;
    return S_OK;
}

void CmdAddObject::Execute(SceneContext& ctx)
{
    ctx.Add(_obj);
    ctx._selected = _obj;
}

void CmdAddObject::Undo(SceneContext& ctx)
{
    ctx.Remove(_obj);
}

Shared<CmdAddObject> CmdAddObject::Create(Shared<EditorObject> obj)
{
    auto instance = make_shared<CmdAddObject>();
    if (FAILED(instance->Initialize(obj)))
    {
        return nullptr;
    }

    return instance;
}
