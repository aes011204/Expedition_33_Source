#include "Trigger_WorldAction.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "Monster_WorldBase.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(Trigger_WorldAction)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_WorldAction";
    info.category = "Trigger";

    return true;
}

Trigger_WorldAction::Trigger_WorldAction(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Trigger_WorldAction::Trigger_WorldAction(const Trigger_WorldAction& Prototype)
    : GameObject(Prototype)
{
}

HRESULT Trigger_WorldAction::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Trigger_WorldAction::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _spawnedLevelIndex = GAME->Current_LevelIndex();
    _elapsedSec = 0.f;
    _pendingDestroy = false;


    return S_OK;
}

void Trigger_WorldAction::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    _elapsedSec += timeDelta;

    if (_pendingDestroy || _elapsedSec >= _lifeTimeSec)
    {
        GAME->Remove_GameObject(_spawnedLevelIndex, GetSharedPtr<GameObject>());
        return;
    }
}

void Trigger_WorldAction::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Trigger_WorldAction::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    const Shared<Monster_WorldBase> hitMonster = dynamic_pointer_cast<Monster_WorldBase>(otherOwner);
    if (hitMonster == nullptr)
        return;

    hitMonster->OnBeginOverlap(other, self);

    if (self)
    {
        self->Set_IsActive(false); // 같은 프레임 다중 몬스터 중복 판정 방지
        self->Clear_Overlap();
    }

    // 전투 시작은 Monster_WorldBase에서 처리하고 여기는 그냥 충돌체 판정용
    _pendingDestroy = true;
}

void Trigger_WorldAction::Set_LifeTimeSec(float lifeTimeSec)
{
    _lifeTimeSec = max(lifeTimeSec, 0.01f);
}

HRESULT Trigger_WorldAction::Ready_Components()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.7f, 0.8f);
    obbDesc.extents = Vec3(0.45f, 0.45f, 0.9f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _colliderCom, &obbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::WorldActionTrigger);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

Shared<Trigger_WorldAction> Trigger_WorldAction::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Trigger_WorldAction>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Trigger_WorldAction");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Trigger_WorldAction::Clone(void* arg)
{
    auto clone = make_shared<Trigger_WorldAction>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Trigger_WorldAction");
        return nullptr;
    }

    return clone;
}

void Trigger_WorldAction::Free()
{
    GameObject::Free();
}
