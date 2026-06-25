#include "NPC_Body.h"
#include "ModelCom.h"

NS_BEGIN(Client)

NPC_Body::NPC_Body(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

NPC_Body::NPC_Body(const NPC_Body& prototype)
    : Monster_Part(prototype)
{
}

HRESULT NPC_Body::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT NPC_Body::Initialize(void* arg)
{
    if (FAILED(Monster_Part::Initialize(arg)))
        return E_FAIL;

    _rootIndex = _model->Get_BoneIndex("root");
    _model->Set_LocalRootNode(_rootIndex);

    return S_OK;
}

void NPC_Body::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void NPC_Body::Update(float timeDelta)
{
    Monster_Part::Update(timeDelta);
}

void NPC_Body::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT NPC_Body::Render()
{
    return Monster_Part::Render();
}

HRESULT NPC_Body::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

Shared<NPC_Body> NPC_Body::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Body>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> NPC_Body::Clone(void* arg)
{
    auto clone = make_shared<NPC_Body>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Body::Free()
{
    Monster_Part::Free();
}

NS_END
