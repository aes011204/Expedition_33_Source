#include "Mon_Simon_Hair.h"

#include "EffectCom.h"

NS_BEGIN(Client)

Mon_Simon_Hair::Mon_Simon_Hair(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Simon_Hair::Mon_Simon_Hair(const Mon_Simon_Hair& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Simon_Hair::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Simon_Hair::Initialize(void* arg)
{
    auto* desc = static_cast<HEAD_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_Simon_Hair::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Simon_Hair::Update(float timeDelta)
{
    if (_socketMatrix != nullptr)
        Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_Simon_Hair::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Simon_Hair::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Simon_Hair::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 Mon_Simon_Hair::Decide_PassIndex(Shared<Material> mat)
{
    UNREFERENCED_PARAMETER(mat);
    return 0;
}

Shared<Mon_Simon_Hair> Mon_Simon_Hair::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Simon_Hair>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Simon_Hair::Clone(void* arg)
{
    auto clone = make_shared<Mon_Simon_Hair>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Simon_Hair::Free()
{
    Monster_Part::Free();
}

NS_END
