#include "LevelChange_Portal.h"

#include "Bounding_Sphere.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "EffectCom.h"
#include "Player_WorldBase.h"

namespace
{
    constexpr wchar_t kDefaultPortalEffectName[] = L"Effects\\Assets\\Map\\FX_Portal.effect.json";
}

IMPLEMENT_REFLECTION(LevelChange_Portal)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "LevelChange_Portal";
    info.category = "Trigger";

    using SelfType = LevelChange_Portal;
    PROPERTY_ENUM("목적지 레벨", _targetLevel, LevelType);
    PROPERTY_WSTRING("포탈 이펙트", _portalEffectName);
    PROPERTY_FLOAT("이펙트 스케일", _effectScale, 0.f, 10.f, 0.01f);

    return true;
}

LevelChange_Portal::LevelChange_Portal(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

LevelChange_Portal::LevelChange_Portal(const LevelChange_Portal& prototype)
    : GameObject(prototype)
    , _targetLevel(prototype._targetLevel)
    , _gameEntryRevealLevel(prototype._gameEntryRevealLevel)
    , _portalEffectName(prototype._portalEffectName)
    , _effectScale(prototype._effectScale)
{
}

HRESULT LevelChange_Portal::Initialize_Prototype()
{
    return S_OK;
}

HRESULT LevelChange_Portal::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _transitionRequested = false;
    return S_OK;
}

void LevelChange_Portal::BeginPlay()
{
    GameObject::BeginPlay();

    Play_PortalEffect();
}

void LevelChange_Portal::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);

    if (_transitionRequested || !Has_ActiveHeroOverlap())
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    if (CLIENT->Is_WorldControlSuspended() ||
        CLIENT->IsDialogueActive() ||
        CLIENT->IsDialogueInteractLocked() ||
        GAME->Is_WorldGameplayInputBlocked())
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    CLIENT->Request_InteractionPrompt(this, InteractionPromptType::Cross);

    if (!GAME->KeyDown(KEY_TYPE::E))
        return;

    _transitionRequested = true;
    CLIENT->Clear_InteractionPrompt(this);

    if (_targetLevel == LevelType::GameEntry)
        GAME->Stop_SoundChannel(SoundChannel::BGM, 0.f);

    if (!CLIENT->Request_LevelTransition(_targetLevel, _gameEntryRevealLevel))
    {
        LOG_INFO("LevelChange_Portal: level transition request failed. target={}",
            magic_enum::enum_name(_targetLevel));
        _transitionRequested = false;
    }
}

void LevelChange_Portal::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void LevelChange_Portal::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (other != nullptr)
        _playerOverlapColliders.insert(other.get());
}

void LevelChange_Portal::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnStayOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (other != nullptr)
        _playerOverlapColliders.insert(other.get());
}

void LevelChange_Portal::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (other != nullptr)
        _playerOverlapColliders.erase(other.get());

    if (!Has_ActiveHeroOverlap())
        CLIENT->Clear_InteractionPrompt(this);
}

void LevelChange_Portal::Set_GameEntryRevealLevel(LevelType levelType)
{
    _gameEntryRevealLevel = levelType;
}

HRESULT LevelChange_Portal::Ready_Components()
{
    Bounding_Sphere::FBoundingSphereDesc sphereDesc{};
    sphereDesc.center = Vec3(0.f, 1.f, 0.f);
    sphereDesc.radius = 1.5f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), _colliderCom, &sphereDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    return S_OK;
}

void LevelChange_Portal::Play_PortalEffect()
{
    if (_effectCom == nullptr)
        return;

    _effectCom->Stop_AllAttachedEffects();

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    if (_portalEffectName.empty())
        desc.effectName = kDefaultPortalEffectName;
    else
        desc.effectName = _portalEffectName;
    desc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    desc.layerTag = L"Layer_Effect";
    desc.localOffset = Vec3(0.f, 1.5f, 0.f);
    desc.followTarget = true;
    desc.scale = Vec3(_effectScale);
    desc.requiresManualRelease = true;

    _effectCom->Play_Effect(desc);
}

bool LevelChange_Portal::Has_ActiveHeroOverlap() const
{
    for (ColliderCom* collider : _playerOverlapColliders)
    {
        if (collider == nullptr)
            continue;

        if (CLIENT->Is_ActiveHero(collider->Get_Owner()))
            return true;
    }

    return false;
}

Shared<LevelChange_Portal> LevelChange_Portal::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<LevelChange_Portal>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : LevelChange_Portal");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> LevelChange_Portal::Clone(void* arg)
{
    auto clone = make_shared<LevelChange_Portal>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : LevelChange_Portal");
        return nullptr;
    }

    return clone;
}

void LevelChange_Portal::Free()
{
    _playerOverlapColliders.clear();
    CLIENT->Clear_InteractionPrompt(this);

    GameObject::Free();
}
