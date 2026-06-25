#include "Mon_PotatoBag_Weapon.h"
#include "EffectCom.h"

NS_BEGIN(Client)

Mon_PotatoBag_Weapon::Mon_PotatoBag_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_PotatoBag_Weapon::Mon_PotatoBag_Weapon(const Mon_PotatoBag_Weapon& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_PotatoBag_Weapon::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_PotatoBag_Weapon::Initialize(void* arg)
{
    auto* desc = static_cast<WEAPON_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;


    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_PotatoBag_Weapon::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_PotatoBag_Weapon::Update(float timeDelta)
{
    if (_socketMatrix != nullptr)
        Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_PotatoBag_Weapon::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_PotatoBag_Weapon::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_PotatoBag_Weapon::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 Mon_PotatoBag_Weapon::Decide_PassIndex(Shared<Material> mat)
{
    return mat->Get_PassIndex();
}

Shared<Mon_PotatoBag_Weapon> Mon_PotatoBag_Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_PotatoBag_Weapon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_PotatoBag_Weapon::Clone(void* arg)
{
    auto clone = make_shared<Mon_PotatoBag_Weapon>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_PotatoBag_Weapon::Free()
{
    Monster_Part::Free();
}

NS_END
