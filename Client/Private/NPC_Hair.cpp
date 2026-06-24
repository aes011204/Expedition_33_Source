#include "NPC_Hair.h"

#include "EffectCom.h"
#include "ChainModifierPreset.h"
#include "PoseFollowChainModifier.h"

NS_BEGIN(Client)

NPC_Hair::NPC_Hair(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

NPC_Hair::NPC_Hair(const NPC_Hair& prototype)
    : Monster_Part(prototype)
{
}

HRESULT NPC_Hair::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT NPC_Hair::Initialize(void* arg)
{
    auto* desc = static_cast<HEAD_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;
    ;

    return S_OK;
}


void NPC_Hair::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void NPC_Hair::Update(float timeDelta)
{
    if (_socketMatrix != nullptr)
        Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));
    else
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

    if (_model)
        _model->Play_Animation(timeDelta);

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void NPC_Hair::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT NPC_Hair::Render()
{
    return Monster_Part::Render();
}

HRESULT NPC_Hair::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 NPC_Hair::Decide_PassIndex(Shared<Material> mat)
{
    UNREFERENCED_PARAMETER(mat);
    return mat->Get_PassIndex();
}

Shared<NPC_Hair> NPC_Hair::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<NPC_Hair>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> NPC_Hair::Clone(void* arg)
{
    auto clone = make_shared<NPC_Hair>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void NPC_Hair::Free()
{
    Monster_Part::Free();
}

NS_END
