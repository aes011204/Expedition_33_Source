#include "SL_Weapon.h"

#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(SL_Weapon)
{
    auto& info = GetStaticReflectionInfo();
    Weapon::Register_Properties();
    info.properties = Weapon::GetStaticReflectionInfo().properties;

    info.displayName = "SL_Weapon";
    info.category = "Weapon";

    return true;
}

SL_Weapon::SL_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Weapon(device, context)
{
}

SL_Weapon::SL_Weapon(const SL_Weapon& prototype)
    : Weapon(prototype)
{
}

HRESULT SL_Weapon::Initialize_Prototype()
{
    return Weapon::Initialize_Prototype();
}

HRESULT SL_Weapon::Initialize(void* arg)
{
    CHECK_FAILED(Weapon::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_AttackCollider(), E_FAIL);

    return S_OK;
}

void SL_Weapon::Update(float timeDelta)
{
    Weapon::Update(timeDelta);
    Sync_AttackCollider();
}

void SL_Weapon::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Weapon::OnBeginOverlap(self, other);

    const Shared<GameObject> owner = Get_Owner();
    if (owner == nullptr)
        return;

    owner->OnBeginOverlap(self, other);
}

HRESULT SL_Weapon::Ready_AttackCollider()
{
    Bounding_OBB::BoundingOBBDesc attackObbDesc{};
    attackObbDesc.center = Vec3(0.f, -55.f, 0.f);
    attackObbDesc.extents = Vec3(18.f, 65.f, 18.f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _attackColliderCom, &attackObbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_PlayerAttack")), E_FAIL);

    _attackColliderCom->Set_CollisionPreset(Collision_Preset::SL_PlayerAttack);
    _attackColliderCom->Set_IsActive(false);
    _attackColliderCom->Clear_Overlap();

    return S_OK;
}

void SL_Weapon::Sync_AttackCollider()
{
    if (_attackColliderCom == nullptr || !_attackColliderCom->Get_IsActive())
        return;

    _attackColliderCom->Update_Collider(Get_CombinedWorldMatrix());
    GAME->Add_Collider(_attackColliderCom, Get_CombinedWorldMatrix());
}

Shared<SL_Weapon> SL_Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_Weapon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> SL_Weapon::Clone(void* arg)
{
    auto instance = make_shared<SL_Weapon>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SL_Weapon");
        return nullptr;
    }

    return instance;
}

void SL_Weapon::Free()
{
    Weapon::Free();
}

NS_END
