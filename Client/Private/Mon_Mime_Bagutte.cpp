#include "Mon_Mime_Bagutte.h"
#include "EffectCom.h"

NS_BEGIN(Client)

Mon_Mime_Bagutte::Mon_Mime_Bagutte(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Mime_Bagutte::Mon_Mime_Bagutte(const Mon_Mime_Bagutte& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Mime_Bagutte::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Mime_Bagutte::Initialize(void* arg)
{
    auto* desc = static_cast<WEAPON_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_Mime_Bagutte::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Mime_Bagutte::Update(float timeDelta)
{
    if (_socketMatrix != nullptr)
        Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_Mime_Bagutte::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Mime_Bagutte::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Mime_Bagutte::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 Mon_Mime_Bagutte::Decide_PassIndex(Shared<Material> mat)
{
    const int OPAQUE_PASS = 0;
    return OPAQUE_PASS;
}

Shared<Mon_Mime_Bagutte> Mon_Mime_Bagutte::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Mime_Bagutte>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Mime_Bagutte::Clone(void* arg)
{
    auto clone = make_shared<Mon_Mime_Bagutte>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Mime_Bagutte::Free()
{
    Monster_Part::Free();
}

NS_END
