#include "Mon_PotatoBag_Shield.h"

#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "EffectCom.h"

NS_BEGIN(Client)

Mon_PotatoBag_Shield::Mon_PotatoBag_Shield(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_PotatoBag_Shield::Mon_PotatoBag_Shield(const Mon_PotatoBag_Shield& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_PotatoBag_Shield::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_PotatoBag_Shield::Initialize(void* arg)
{
    auto* desc = static_cast<SHIELD_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_PotatoBag_Shield::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_PotatoBag_Shield::Update(float timeDelta)
{
    if (_socketMatrix != nullptr)
        Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);

    if (_shieldColliderCom)
    {
        _shieldColliderCom->Update_Collider(Get_CombinedWorldMatrix());
        GAME->Add_Collider(_shieldColliderCom, Get_CombinedWorldMatrix());
    }
}

void Mon_PotatoBag_Shield::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_PotatoBag_Shield::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_PotatoBag_Shield::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

HRESULT Mon_PotatoBag_Shield::Ready_Components(const MONSTER_PART_DESC& desc)
{
    Bounding_OBB::BoundingOBBDesc shieldObbDesc{};
    shieldObbDesc.center = Vec3(0.f, 1.0f, 0.f);
    shieldObbDesc.extents = Vec3(0.75f, 1.0f, 0.35f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _shieldColliderCom, &shieldObbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_PotatoBag_ShieldCollider")), E_FAIL);

    _shieldColliderCom->Set_CollisionPreset(Collision_Preset::Monster_Body);
    _shieldColliderCom->Clear_Overlap();

    return Monster_Part::Ready_Components(desc);
}

uint32 Mon_PotatoBag_Shield::Decide_PassIndex(Shared<Material> mat)
{
    const int OPAQUE_PASS = 0;
    UNREFERENCED_PARAMETER(mat);
    return OPAQUE_PASS;
}

Shared<Mon_PotatoBag_Shield> Mon_PotatoBag_Shield::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_PotatoBag_Shield>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_PotatoBag_Shield::Clone(void* arg)
{
    auto clone = make_shared<Mon_PotatoBag_Shield>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_PotatoBag_Shield::Free()
{
    Monster_Part::Free();
}

NS_END
