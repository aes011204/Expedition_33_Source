#include "Mon_WeakPoint.h"
#include "EffectCom.h"
#include "ClientInstance.h"

NS_BEGIN(Client)

Mon_WeakPoint::Mon_WeakPoint(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_WeakPoint::Mon_WeakPoint(const Mon_WeakPoint& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_WeakPoint::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_WeakPoint::Initialize(void* arg)
{
    auto* desc = static_cast<WEAK_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_WeakPoint::BeginPlay()
{
    Monster_Part::BeginPlay();

    if (!CLIENT->Play_AttachPartEffect(Get_Owner(), L"Monster_Weak", L"Part_Weak", Vec3::Zero, Vec3::Zero,Vec3(.3f,.3f,.3f) ), E_FAIL)
    {
        return;
    }
}

void Mon_WeakPoint::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_WeakPoint::Update(float timeDelta)
{
    Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);

    
 
}

void Mon_WeakPoint::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_WeakPoint::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_WeakPoint::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 Mon_WeakPoint::Decide_PassIndex(Shared<Material> mat)
{
    const int OPAQUE_PASS = 0;
    return OPAQUE_PASS;
}

Shared<Mon_WeakPoint> Mon_WeakPoint::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_WeakPoint>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_WeakPoint::Clone(void* arg)
{
    auto clone = make_shared<Mon_WeakPoint>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_WeakPoint::Free()
{
    Monster_Part::Free();
}

NS_END
