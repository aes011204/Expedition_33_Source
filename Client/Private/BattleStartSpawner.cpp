#include "BattleStartSpawner.h"
#include "ColliderCom.h"
#include "Bounding_Sphere.h"

IMPLEMENT_REFLECTION(BattleStartSpawner)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "BattleStartSpawner";
    info.category = "BattleStartSpawner";

    PROPERTY_INT("Spawner Id", _spawnerId, 1, 10, 1);

    return true;
}

BattleStartSpawner::BattleStartSpawner(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

BattleStartSpawner::BattleStartSpawner(const BattleStartSpawner& prototype)
    : GameObject(prototype)
{
}

HRESULT BattleStartSpawner::Initialize_Prototype()
{
    return S_OK;
}

HRESULT BattleStartSpawner::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(GameObject::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void BattleStartSpawner::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

     if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

HRESULT BattleStartSpawner::Ready_Components()
{
     Bounding_Sphere::FBoundingSphereDesc sphereDesc{};
    sphereDesc.center = Vec3(0.f, 0.5f, 0.f);
    sphereDesc.radius = 0.6f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), _colliderCom, &sphereDesc), E_FAIL);

    _colliderCom->Set_IsActive(true);

    return S_OK;
}

Shared<BattleStartSpawner> BattleStartSpawner::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleStartSpawner>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : BattleStartSpawner");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> BattleStartSpawner::Clone(void* arg)
{
    auto clone = make_shared<BattleStartSpawner>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : BattleStartSpawner");
        return nullptr;
    }

    return clone;
}

void BattleStartSpawner::Free()
{
    GameObject::Free();
}
