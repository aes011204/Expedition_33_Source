#include "Trigger_BGMZone.h"

#include "Bounding_AABB.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "ColliderCom.h"

REGISTER_GLOBAL_ENUM(Trigger_BGMZone::GestralBGMTrack, "GestralBGMTrack")

IMPLEMENT_REFLECTION(Trigger_BGMZone)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_BGMZone";
    info.category = "Trigger";

    using SelfType = Trigger_BGMZone;
    PROPERTY_ENUM("BGM Track", _bgmTrack, GestralBGMTrack);
    PROPERTY_FLOAT("Fade Out", _fadeOutDuration, 0.f, 5.f, 0.05f);

    return true;
}

Trigger_BGMZone::Trigger_BGMZone(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Trigger_BGMZone::Trigger_BGMZone(const Trigger_BGMZone& prototype)
    : GameObject(prototype)
    , _bgmTrack(prototype._bgmTrack)
    , _fadeOutDuration(prototype._fadeOutDuration)
{
}

HRESULT Trigger_BGMZone::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Trigger_BGMZone::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _savedBGM.clear();
    _hasSavedBGM = false;
    _playerOverlapCount = 0;

    return S_OK;
}

void Trigger_BGMZone::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Trigger_BGMZone::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (self.get() != _colliderCom.get())
        return;

    if (!Is_ActivePlayerCollider(other))
        return;

    ++_playerOverlapCount;
    if (_playerOverlapCount != 1)
        return;

    Play_OverrideBGM();
}

void Trigger_BGMZone::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);

    if (self.get() != _colliderCom.get())
        return;

    if (!Is_ActivePlayerCollider(other))
        return;

    _playerOverlapCount = max(0, _playerOverlapCount - 1);
    if (_playerOverlapCount != 0)
        return;

    Restore_PreviousBGM();
}

HRESULT Trigger_BGMZone::Ready_Components()
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = Vec3(0.f, 1.f, 0.f);
    aabbDesc.extents = Vec3(3.f, 2.f, 3.f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), _colliderCom, &aabbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

const wchar_t* Trigger_BGMZone::Resolve_BGMTrack() const
{
    switch (_bgmTrack)
    {
    case GestralBGMTrack::Market:
        return Constants::SoundBGM::GestralVillageMarket;

    case GestralBGMTrack::GolgraThrone:
        return Constants::SoundBGM::GestralVillageGolgraThrone;

    default:
        return nullptr;
    }
}

bool Trigger_BGMZone::Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const
{
    const Shared<GameObject> overlapOwner = other ? other->Get_Owner() : nullptr;
    if (overlapOwner == nullptr)
        return false;

    Shared<GameObject> heroOwner = overlapOwner;
    if (!CLIENT->Is_ActiveHero(heroOwner))
        heroOwner = overlapOwner->Get_Owner();

    return CLIENT->Is_ActiveHero(heroOwner);
}

void Trigger_BGMZone::Play_OverrideBGM()
{
    const wchar_t* bgm = Resolve_BGMTrack();
    if (bgm == nullptr || bgm[0] == L'\0')
        return;

    _savedBGM = GAME->Get_CurrentBGM();
    _hasSavedBGM = true;

    if (_savedBGM == bgm)
        return;

    GAME->Play_BGM(bgm, 1.f, true, _fadeOutDuration);
}

void Trigger_BGMZone::Restore_PreviousBGM()
{
    if (!_hasSavedBGM)
        return;

    if (_savedBGM.empty())
        GAME->Stop_SoundChannel(SoundChannel::BGM, _fadeOutDuration);
    else
        GAME->Play_BGM(_savedBGM, 1.f, true, _fadeOutDuration);

    _savedBGM.clear();
    _hasSavedBGM = false;
}

Shared<Trigger_BGMZone> Trigger_BGMZone::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Trigger_BGMZone>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Trigger_BGMZone");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Trigger_BGMZone::Clone(void* arg)
{
    auto clone = make_shared<Trigger_BGMZone>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Trigger_BGMZone");
        return nullptr;
    }

    return clone;
}

void Trigger_BGMZone::Free()
{
    GameObject::Free();
}
