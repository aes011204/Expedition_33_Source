#include "Trigger_WorldDialogue.h"

#include "Bounding_AABB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"

IMPLEMENT_REFLECTION(Trigger_WorldDialogue)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_WorldDialogue";
    info.category = "Trigger";

    PROPERTY_WSTRING("Line 1", _line1);
    PROPERTY_WSTRING("Line 2", _line2);
    PROPERTY_WSTRING("Line 3", _line3);
    PROPERTY_WSTRING("Portrait 1", _portrait1);
    PROPERTY_WSTRING("Portrait 2", _portrait2);
    PROPERTY_WSTRING("Portrait 3", _portrait3);
    PROPERTY_FLOAT("Line Interval", _lineIntervalSec, 0.f, 10.f, 0.05f);
    PROPERTY_FLOAT("Auto Close Delay", _autoCloseDelaySec, 0.f, 30.f, 0.1f);
    PROPERTY_BOOL("Trigger Once", _triggerOnce);
    PROPERTY_BOOL("Close On Exit", _closeOnExit);

    return true;
}

Trigger_WorldDialogue::Trigger_WorldDialogue(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Trigger_WorldDialogue::Trigger_WorldDialogue(const Trigger_WorldDialogue& prototype)
    : GameObject(prototype)
    , _line1(prototype._line1)
    , _line2(prototype._line2)
    , _line3(prototype._line3)
    , _portrait1(prototype._portrait1)
    , _portrait2(prototype._portrait2)
    , _portrait3(prototype._portrait3)
    , _lineIntervalSec(prototype._lineIntervalSec)
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
    _sequenceActive = false;
    _played = false;
    _nextLineIndex = 0;
    _lineElapsedSec = 0.f;
    _afterLastLineElapsedSec = 0.f;

    return S_OK;
}

void Trigger_WorldDialogue::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (!_sequenceActive)
    {
        if (Has_PlayerOverlap() && (!_triggerOnce || !_played) && Can_StartSequence())
            Start_Sequence();

        return;
    }

    if (_nextLineIndex < 3)
    {
        _lineElapsedSec += timeDelta;
        if (_lineElapsedSec >= max(0.f, _lineIntervalSec))
            Push_NextLine();

        return;
    }

    if (_autoCloseDelaySec <= 0.f)
        return;

    _afterLastLineElapsedSec += timeDelta;
    if (_afterLastLineElapsedSec >= _autoCloseDelaySec)
        Stop_Sequence(true);
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

    if (!_sequenceActive && (!_triggerOnce || !_played) && Can_StartSequence())
        Start_Sequence();
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

    if (_closeOnExit && !Has_PlayerOverlap())
        Stop_Sequence(true);
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

void Trigger_WorldDialogue::Set_Line(uint32 index, const wstring& text, const wstring& portraitTexturePath)
{
    switch (index)
    {
    case 0:
        _line1 = text;
        _portrait1 = portraitTexturePath;
        break;
    case 1:
        _line2 = text;
        _portrait2 = portraitTexturePath;
        break;
    case 2:
        _line3 = text;
        _portrait3 = portraitTexturePath;
        break;
    default:
        break;
    }
}

void Trigger_WorldDialogue::Set_Timing(float lineIntervalSec, float autoCloseDelaySec)
{
    _lineIntervalSec = max(0.f, lineIntervalSec);
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

bool Trigger_WorldDialogue::Has_AnyLine() const
{
    return !_line1.empty() || !_line2.empty() || !_line3.empty();
}

bool Trigger_WorldDialogue::Can_StartSequence() const
{
    if (!Has_AnyLine())
        return false;

    if (CLIENT->Is_WorldControlSuspended())
        return false;

    if (GAME->Is_WorldGameplayInputBlocked())
        return false;

    if (CLIENT->IsDialogueActive() || CLIENT->IsDialogueInteractLocked())
        return false;

    return true;
}

void Trigger_WorldDialogue::Start_Sequence()
{
    _sequenceActive = true;
    _nextLineIndex = 0;
    _lineElapsedSec = 0.f;
    _afterLastLineElapsedSec = 0.f;

    CLIENT->Clear_WorldDialogue();

    if (!Push_NextLine())
    {
        Stop_Sequence(true);
        return;
    }

    _played = true;

    if (_triggerOnce && _colliderCom)
    {
        _colliderCom->Set_IsActive(false);
        _colliderCom->Clear_Overlap();
    }
}

bool Trigger_WorldDialogue::Push_NextLine()
{
    while (_nextLineIndex < 3)
    {
        const uint32 lineIndex = _nextLineIndex++;
        const wstring& text = Get_LineText(lineIndex);
        if (text.empty())
            continue;

        CLIENT->Push_WorldDialogueLine(text, Get_LinePortrait(lineIndex));
        _lineElapsedSec = 0.f;
        _afterLastLineElapsedSec = 0.f;
        return true;
    }

    _lineElapsedSec = 0.f;
    _afterLastLineElapsedSec = 0.f;
    return false;
}

void Trigger_WorldDialogue::Stop_Sequence(bool clearDialogue)
{
    _sequenceActive = false;
    _nextLineIndex = 0;
    _lineElapsedSec = 0.f;
    _afterLastLineElapsedSec = 0.f;

    if (clearDialogue)
        CLIENT->Clear_WorldDialogue();
}

const wstring& Trigger_WorldDialogue::Get_LineText(uint32 index) const
{
    static const wstring empty{};

    switch (index)
    {
    case 0:
        return _line1;
    case 1:
        return _line2;
    case 2:
        return _line3;
    default:
        return empty;
    }
}

const wstring& Trigger_WorldDialogue::Get_LinePortrait(uint32 index) const
{
    static const wstring empty{};

    switch (index)
    {
    case 0:
        return _portrait1;
    case 1:
        return _portrait2;
    case 2:
        return _portrait3;
    default:
        return empty;
    }
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
    if (_sequenceActive)
        CLIENT->Clear_WorldDialogue();

    _playerOverlapColliders.clear();

    GameObject::Free();
}
