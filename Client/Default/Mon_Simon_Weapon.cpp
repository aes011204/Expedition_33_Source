#include "pch.h"
#include "Mon_Simon_Weapon.h"
NS_BEGIN(Client)

Mon_Simon_Weapon::Mon_Simon_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Simon_Weapon::Mon_Simon_Weapon(const Mon_Simon_Weapon& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Simon_Weapon::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Simon_Weapon::Initialize(void* arg)
{
    auto* desc = static_cast<WEAPON_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_Simon_Weapon::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Simon_Weapon::Update(float timeDelta)
{
    Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * *_socketMatrix);
}

void Mon_Simon_Weapon::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Simon_Weapon::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Simon_Weapon::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 Mon_Simon_Weapon::Decide_PassIndex(Shared<Material> mat)
{
    const int OPAQUE_PASS = 0;
    return OPAQUE_PASS;
}

Shared<Mon_Simon_Weapon> Mon_Simon_Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Simon_Weapon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Simon_Weapon::Clone(void* arg)
{
    auto clone = make_shared<Mon_Simon_Weapon>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Simon_Weapon::Free()
{
    Monster_Part::Free();
}

NS_END
