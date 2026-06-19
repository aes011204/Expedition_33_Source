#include "Mon_Dualliste_Sword_R.h"

#include "EffectCom.h"

NS_BEGIN(Client)

Mon_Dualliste_Sword_R::Mon_Dualliste_Sword_R(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Dualliste_Sword_R::Mon_Dualliste_Sword_R(const Mon_Dualliste_Sword_R& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Dualliste_Sword_R::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Dualliste_Sword_R::Initialize(void* arg)
{
    auto* desc = static_cast<SWORD_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_Dualliste_Sword_R::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Dualliste_Sword_R::Update(float timeDelta)
{
    Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_Dualliste_Sword_R::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Dualliste_Sword_R::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Dualliste_Sword_R::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 Mon_Dualliste_Sword_R::Decide_PassIndex(Shared<Material> mat)
{
    const int opaquePass = 0;
    UNREFERENCED_PARAMETER(mat);
    return opaquePass;
}

Shared<Mon_Dualliste_Sword_R> Mon_Dualliste_Sword_R::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Dualliste_Sword_R>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Dualliste_Sword_R::Clone(void* arg)
{
    auto clone = make_shared<Mon_Dualliste_Sword_R>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Dualliste_Sword_R::Free()
{
    Monster_Part::Free();
}

NS_END
