#include "Trigger_WorldDialogue.h"

#include "Bounding_AABB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "GameplayUI_Types.h"

IMPLEMENT_REFLECTION(Trigger_WorldDialogue)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_WorldDialogue";
    info.category = "Trigger";

    PROPERTY_WSTRING("_uiJsonPath", _uiJsonPath);
    PROPERTY_WSTRING("_rootUIName", _rootUIName);
    PROPERTY_WSTRING("_animationPath", _animationPath);
    PROPERTY_FLOAT("_autoCloseDelaySec", _autoCloseDelaySec, 0.f, 60.f, 0.1f);
    PROPERTY_BOOL("_triggerOnce", _triggerOnce);
    PROPERTY_BOOL("_closeOnExit", _closeOnExit);

    return true;
}

Trigger_WorldDialogue::Trigger_WorldDialogue(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Trigger_WorldDialogue::Trigger_WorldDialogue(const Trigger_WorldDialogue& prototype)
    : GameObject(prototype)
    , _uiJsonPath(prototype._uiJsonPath)
    , _rootUIName(prototype._rootUIName)
    , _animationPath(prototype._animationPath)
    , _autoCloseDelaySec(prototype._autoCloseDelaySec)
    , _triggerOnce(prototype._triggerOnce)
    , _closeOnExit(prototype._closeOnExit)
{
}

HRESULT Trigger_WorldDialogue::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Trigger_WorldDialogue::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _playerOverlapColliders.clear();
    _openedForCurrentOverlap = false;
    _played = false;

    return S_OK;
}

void Trigger_WorldDialogue::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (!Has_PlayerOverlap())
        return;

    if (_openedForCurrentOverlap)
        return;

    if (_triggerOnce && _played)
        return;

    Try_OpenUI();
}

void Trigger_WorldDialogue::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Trigger_WorldDialogue::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    if (!Is_ActivePlayerCollider(other))
        return;

    if (other != nullptr)
        _playerOverlapColliders.insert(other.get());

    if (!_openedForCurrentOverlap && (!_triggerOnce || !_played))
        Try_OpenUI();
}

void Trigger_WorldDialogue::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    if (!Is_ActivePlayerCollider(other))
        return;

    if (other != nullptr)
        _playerOverlapColliders.erase(other.get());

    if (Has_PlayerOverlap())
        return;

    if (_closeOnExit && _openedForCurrentOverlap)
        Close_UI();

    _openedForCurrentOverlap = false;

    if (_triggerOnce && _played)
        Disable_TriggerCollider();
}

HRESULT Trigger_WorldDialogue::Ready_Components()
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = Vec3(0.f, 1.f, 0.f);
    aabbDesc.extents = Vec3(2.5f, 1.5f, 2.5f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), _colliderCom, &aabbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

void Trigger_WorldDialogue::Set_UIRequest(
    const wstring& uiJsonPath,
    const wstring& rootUIName,
    const wstring& animationPath,
    float autoCloseDelaySec)
{
    _uiJsonPath = uiJsonPath;
    _rootUIName = rootUIName;
    _animationPath = animationPath;

    if (autoCloseDelaySec >= 0.f)
        _autoCloseDelaySec = max(0.f, autoCloseDelaySec);
}

void Trigger_WorldDialogue::Set_TriggerOptions(bool triggerOnce, bool closeOnExit)
{
    _triggerOnce = triggerOnce;
    _closeOnExit = closeOnExit;
}

bool Trigger_WorldDialogue::Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const
{
    const Shared<GameObject> overlapOwner = other ? other->Get_Owner() : nullptr;
    if (overlapOwner == nullptr)
        return false;

    Shared<GameObject> heroOwner = overlapOwner;
    if (!CLIENT->Is_ActiveHero(heroOwner))
        heroOwner = overlapOwner->Get_Owner();

    return CLIENT->Is_ActiveHero(heroOwner);
}

bool Trigger_WorldDialogue::Can_OpenUI() const
{
    if (_uiJsonPath.empty() || _rootUIName.empty())
        return false;

    if (CLIENT->Is_WorldControlSuspended())
        return false;

    if (GAME->Is_WorldGameplayInputBlocked())
        return false;

    if (CLIENT->IsDialogueActive() || CLIENT->IsDialogueInteractLocked())
        return false;

    return true;
}

bool Trigger_WorldDialogue::Try_OpenUI()
{
    if (!Can_OpenUI())
        return false;

    EventDialogueUIRequest request{};
    request.uiJsonPath = _uiJsonPath;
    request.rootUIName = _rootUIName;
    request.animationPath = _animationPath;
    request.autoCloseDelaySec = _autoCloseDelaySec;

    if (!CLIENT->Open_EventDialogueUI(request))
        return false;

    _openedForCurrentOverlap = true;
    _played = true;

    if (_triggerOnce && !_closeOnExit)
        Disable_TriggerCollider();

    return true;
}

void Trigger_WorldDialogue::Close_UI()
{
    CLIENT->Close_EventDialogueUI();
}

void Trigger_WorldDialogue::Disable_TriggerCollider()
{
    if (_colliderCom == nullptr)
        return;

    _colliderCom->Set_IsActive(false);
    _colliderCom->Clear_Overlap();
    _playerOverlapColliders.clear();
}

Shared<Trigger_WorldDialogue> Trigger_WorldDialogue::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Trigger_WorldDialogue>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Trigger_WorldDialogue");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Trigger_WorldDialogue::Clone(void* arg)
{
    auto clone = make_shared<Trigger_WorldDialogue>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Trigger_WorldDialogue");
        return nullptr;
    }

    return clone;
}

void Trigger_WorldDialogue::Free()
{
    if (_openedForCurrentOverlap && _closeOnExit)
        Close_UI();

    _playerOverlapColliders.clear();

    GameObject::Free();
}
