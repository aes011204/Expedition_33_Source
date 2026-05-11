#include "Mon_Lancleier_Weapon.h"

NS_BEGIN(Client)

Mon_Lancleier_Weapon::Mon_Lancleier_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Lancleier_Weapon::Mon_Lancleier_Weapon(const Mon_Lancleier_Weapon& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Lancleier_Weapon::Initialize_Prototype()
{


    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Lancleier_Weapon::Initialize(void* arg)
{
    auto* desc = static_cast<WEAPON_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    if(FAILED(Monster_Part::Initialize(desc)))
    {
        return E_FAIL;
    }

    return S_OK;
}

void Mon_Lancleier_Weapon::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Lancleier_Weapon::Update(float timeDelta)
{
    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix() * (  *_socketMatrix));
}

void Mon_Lancleier_Weapon::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Lancleier_Weapon::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Lancleier_Weapon::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

Shared<Mon_Lancleier_Weapon> Mon_Lancleier_Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Lancleier_Weapon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Lancleier_Weapon::Clone(void* arg)
{
    auto clone = make_shared<Mon_Lancleier_Weapon>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Lancleier_Weapon::Free()
{
    Monster_Part::Free();
}

NS_END
