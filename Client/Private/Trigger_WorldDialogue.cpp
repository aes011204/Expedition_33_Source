#include "Trigger_WorldDialogue.h"

#include "Bounding_AABB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "GameplayUI_Types.h"
#include "Helper_String.h"

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

    const bool activePlayer = Is_ActivePlayerCollider(other);
#if defined(_DEBUG) || defined(_EDITOR)
    Debug_LogOverlapEvent("Begin", other, activePlayer);
#endif

    if (!activePlayer)
        return;

    if (other != nullptr)
        _playerOverlapColliders.insert(other.get());

    if (!_openedForCurrentOverlap && (!_triggerOnce || !_played))
        Try_OpenUI();
}

void Trigger_WorldDialogue::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnStayOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const bool activePlayer = Is_ActivePlayerCollider(other);
#if defined(_DEBUG) || defined(_EDITOR)
    Debug_LogOverlapEvent("Stay", other, activePlayer);
#endif

    if (!activePlayer)
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

    const bool activePlayer = Is_ActivePlayerCollider(other);
#if defined(_DEBUG) || defined(_EDITOR)
    Debug_LogOverlapEvent("End", other, activePlayer);
#endif

    if (!activePlayer)
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

const char* Trigger_WorldDialogue::Get_OpenBlockReason() const
{
    if (_uiJsonPath.empty() || _rootUIName.empty())
        return "empty uiJsonPath/rootUIName";

    if (CLIENT->Is_WorldControlSuspended())
        return "world control suspended";

    if (GAME->Is_WorldGameplayInputBlocked())
        return "world gameplay input blocked";

    if (CLIENT->IsDialogueActive() || CLIENT->IsDialogueInteractLocked())
        return "dialogue active or interact locked";

    return nullptr;
}

bool Trigger_WorldDialogue::Can_OpenUI() const
{
    if (Get_OpenBlockReason() != nullptr)
        return false;

    return true;
}

bool Trigger_WorldDialogue::Try_OpenUI()
{
    const char* blockReason = Get_OpenBlockReason();
    if (blockReason != nullptr)
    {
#if defined(_DEBUG) || defined(_EDITOR)
        Debug_LogOpenBlocked(blockReason);
#endif
        return false;
    }

    EventDialogueUIRequest request{};
    request.uiJsonPath = _uiJsonPath;
    request.rootUIName = _rootUIName;
    request.animationPath = _animationPath;
    request.autoCloseDelaySec = _autoCloseDelaySec;

    if (!CLIENT->Open_EventDialogueUI(request))
    {
#if defined(_DEBUG) || defined(_EDITOR)
        Debug_LogOpenBlocked("Open_EventDialogueUI returned false");
#endif
        return false;
    }

    _openedForCurrentOverlap = true;
    _played = true;
#if defined(_DEBUG) || defined(_EDITOR)
    _debugLastOpenBlockReason = nullptr;
    LOG_INFO(
        "[WorldDialogueTrigger] Opened ui='{}' root='{}' anim='{}'",
        String::ToString(_uiJsonPath),
        String::ToString(_rootUIName),
        String::ToString(_animationPath));
#endif

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

#if defined(_DEBUG) || defined(_EDITOR)
void Trigger_WorldDialogue::Debug_LogOverlapEvent(
    const char* eventName,
    const Shared<ColliderCom>& other,
    bool activePlayer)
{
    if (!activePlayer)
    {
        if (_debugRejectedOverlapLogCount >= 12)
            return;

        ++_debugRejectedOverlapLogCount;
    }
    else if (eventName != nullptr && eventName[0] == 'S')
    {
        if (_debugStayOverlapLogCount >= 5)
            return;

        ++_debugStayOverlapLogCount;
    }

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    const Shared<GameObject> parentOwner = otherOwner ? otherOwner->Get_Owner() : nullptr;

    const string otherOwnerName =
        otherOwner ? String::ToString(otherOwner->Get_Name()) : string("null");
    const string parentOwnerName =
        parentOwner ? String::ToString(parentOwner->Get_Name()) : string("null");

    Vec3 position{};
    Vec3 scale{ 1.f, 1.f, 1.f };
    if (_transformCom != nullptr)
    {
        position = _transformCom->Get_WorldPosition();
        scale = _transformCom->Get_Scale();
    }

    LOG_INFO(
        "[WorldDialogueTrigger][{}] ui='{}' activeHero={} opened={} played={} otherPreset={} otherOwner='{}' parentOwner='{}' pos=({}, {}, {}) scale=({}, {}, {})",
        eventName != nullptr ? eventName : "Unknown",
        String::ToString(_uiJsonPath),
        activePlayer,
        _openedForCurrentOverlap,
        _played,
        other ? static_cast<int32>(other->Get_CollisionPreset()) : -1,
        otherOwnerName,
        parentOwnerName,
        position.x,
        position.y,
        position.z,
        scale.x,
        scale.y,
        scale.z);
}

void Trigger_WorldDialogue::Debug_LogOpenBlocked(const char* reason)
{
    if (_debugLastOpenBlockReason == reason)
        return;

    _debugLastOpenBlockReason = reason;

    LOG_WARN(
        "[WorldDialogueTrigger] Open blocked. reason='{}' ui='{}' root='{}' overlapCount={} opened={} played={}",
        reason != nullptr ? reason : "unknown",
        String::ToString(_uiJsonPath),
        String::ToString(_rootUIName),
        _playerOverlapColliders.size(),
        _openedForCurrentOverlap,
        _played);
}
#endif

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
