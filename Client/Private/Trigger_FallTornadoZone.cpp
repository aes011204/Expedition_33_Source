#include "Trigger_FallTornadoZone.h"

#include "Bounding_AABB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "EffectCom.h"

IMPLEMENT_REFLECTION(Trigger_FallTornadoZone)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_FallTornadoZone";
    info.category = "Trigger";

    using SelfType = Trigger_FallTornadoZone;
    PROPERTY_VEC3("Effect Scale", _effectScale);
    PROPERTY_VEC3("Effect Local Offset", _effectLocalOffset);
    PROPERTY_FLOAT("Stop World Y", _stopWorldY, -10000.f, 0.f, 1.f);

    return true;
}

Trigger_FallTornadoZone::Trigger_FallTornadoZone(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Trigger_FallTornadoZone::Trigger_FallTornadoZone(const Trigger_FallTornadoZone& prototype)
    : GameObject(prototype)
{
}

HRESULT Trigger_FallTornadoZone::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Trigger_FallTornadoZone::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _hasTriggered = false;
    _trackedPlayer.reset();

    return S_OK;
}

void Trigger_FallTornadoZone::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);

    if (!_hasTriggered)
    {
        return;
    }

    const Shared<GameObject> player = _trackedPlayer.lock();
    if (!player)
    {
        _hasTriggered = false;
        return;
    }

    const float playerWorldY = player->Get_Transform()->Get_WorldPosition().y;

    if (!_isFollowing && playerWorldY <= _spawnWorldY - _followStartDropDistance)
    {
        if (const Shared<EffectCom> activeEffectCom = _activeEffectCom.lock())
        {
            activeEffectCom->Stop_Effect(_fallTornadoHandle);
        }

        if (const Shared<EffectCom> playerEffectCom = player->Get_Component<EffectCom>())
        {
            Play_FallTornadoEffect(playerEffectCom, true);
        }

        _isFollowing = true;
    }

    if (playerWorldY <= _stopWorldY)
    {
        Stop_FallTornadoEffect();
    }
}

void Trigger_FallTornadoZone::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (self.get() != _colliderCom.get())
        return;

    if (!Is_ActivePlayerCollider(other))
        return;

    if (_hasTriggered)
        return;

    _hasTriggered = true;
    _trackedPlayer = other->Get_Owner();
}

void Trigger_FallTornadoZone::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);
}

void Trigger_FallTornadoZone::BeginPlay()
{
    GameObject::BeginPlay();

    _spawnWorldY = _transformCom->Get_WorldPosition().y;
    Play_FallTornadoEffect(_effectCom, false);
}

HRESULT Trigger_FallTornadoZone::Ready_Components()
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = Vec3(0.f, 1.f, 0.f);
    aabbDesc.extents = Vec3(3.f, 2.f, 3.f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), _colliderCom, &aabbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    return S_OK;
}

bool Trigger_FallTornadoZone::Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const
{
    const Shared<GameObject> overlapOwner = other ? other->Get_Owner() : nullptr;
    if (overlapOwner == nullptr)
        return false;

    Shared<GameObject> heroOwner = overlapOwner;
    if (!CLIENT->Is_ActiveHero(heroOwner))
        heroOwner = overlapOwner->Get_Owner();

    return CLIENT->Is_ActiveHero(heroOwner);
}

void Trigger_FallTornadoZone::Play_FallTornadoEffect(const Shared<EffectCom>& effectCom, bool followTarget)
{
    if (!effectCom)
    {
        return;
    }

    EffectCom::EffectPlayDesc desc{};
    desc.effectName = L"Effects/Assets/Map/FX_TornadoPaint_EX_Vertical.effect.json";
    desc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    desc.followTarget = followTarget;
    desc.requiresManualRelease = true;
    desc.scale = _effectScale;
    desc.localOffset = _effectLocalOffset;

    effectCom->Play_Effect(desc, &_fallTornadoHandle);
    _activeEffectCom = effectCom;
}

void Trigger_FallTornadoZone::Stop_FallTornadoEffect()
{
    if (const Shared<EffectCom> effectCom = _activeEffectCom.lock())
    {
        effectCom->Stop_Effect(_fallTornadoHandle);
    }

    _fallTornadoHandle = {};
    _activeEffectCom.reset();
    _trackedPlayer.reset();
}

Shared<Trigger_FallTornadoZone> Trigger_FallTornadoZone::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Trigger_FallTornadoZone>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Trigger_FallTornadoZone");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Trigger_FallTornadoZone::Clone(void* arg)
{
    auto clone = make_shared<Trigger_FallTornadoZone>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Trigger_FallTornadoZone");
        return nullptr;
    }

    return clone;
}

void Trigger_FallTornadoZone::Free()
{
    GameObject::Free();
}
