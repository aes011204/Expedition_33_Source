#include "Item.h"

#include "Bounding_OBB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "EffectCom.h"
#include "Player_WorldBase.h"

namespace
{
    constexpr wchar_t kItemCrystalEffectName[] = L"Effects\\Assets\\Item\\Crystal.effect.json";
    constexpr float kItemVisualBaseOffsetY = 0.7f;
    constexpr float kItemHoverFrequency = 1.8f;
    constexpr float kItemHoverAmplitude = 0.06f;
}

Item::Item(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Item::Item(const Item& prototype)
    : GameObject(prototype)
    , _itemId(prototype._itemId)
    , _displayName(prototype._displayName)
    , _amount(prototype._amount)
    , _pickupEffectName(prototype._pickupEffectName)
{
}

HRESULT Item::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Item::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _spawnedLevelIndex = GAME->Current_LevelIndex();
    _hasPlayerOverlap = false;
    _pickedUp = false;
    _visualHoverTimeSec = 0.f;
    _spawnRevealActive = false;
    _spawnRevealElapsed = 0.f;
    _spawnRevealDuration = 0.f;

    UNREFERENCED_PARAMETER(arg);
    return S_OK;
}

void Item::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    Update_VisualHover(timeDelta);
    Update_SpawnReveal(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);

    if (_pickedUp || !_hasPlayerOverlap)
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

    CLIENT->Request_InteractionPrompt(this, InteractionPromptType::Picking);

    if (GAME->KeyDown(KEY_TYPE::E))
        Try_Pickup();
}

void Item::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Item::Start_SpawnReveal(float durationSec)
{
    _spawnRevealActive = durationSec > 0.f;
    _spawnRevealElapsed = 0.f;
    _spawnRevealDuration = max(durationSec, 0.001f);

    if (_effectCom)
        _effectCom->Set_AttachedEffectMaterialReveal(0.f, 1.f);
}

void Item::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (_pickedUp)
        return;

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (!_hasPlayerOverlap)
    {
        _hasPlayerOverlap = true;
        CLIENT->AddItemInteractionOverlap();
    }
}

void Item::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnStayOverlap(self, other);

    if (_pickedUp)
        return;

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (!_hasPlayerOverlap)
    {
        _hasPlayerOverlap = true;
        CLIENT->AddItemInteractionOverlap();
    }
}

void Item::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (_hasPlayerOverlap)
    {
        _hasPlayerOverlap = false;
        CLIENT->RemoveItemInteractionOverlap();
        CLIENT->Clear_InteractionPrompt(this);
    }
}

json Item::To_Json() const
{
    json data = GameObject::To_Json();
    json& customProperties = data["custom_properties"];

    customProperties["item_id"] = _itemId;
    customProperties["display_name"] = _displayName;
    customProperties["amount"] = _amount;

    return data;
}

void Item::From_Json(const json& data)
{
    GameObject::From_Json(data);

    if (_hasPlayerOverlap)
    {
        _hasPlayerOverlap = false;
        CLIENT->RemoveItemInteractionOverlap();
        CLIENT->Clear_InteractionPrompt(this);
    }

    _pickedUp = false;
    _spawnedLevelIndex = GAME->Current_LevelIndex();
    _visualHoverTimeSec = 0.f;
    _spawnRevealActive = false;
    _spawnRevealElapsed = 0.f;
    _spawnRevealDuration = 0.f;

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();
    }

    if (_effectCom)
    {
        _visualEffectBaseLocalOffset = _effectCom->Get_InspectorEffectSettings().localOffset;
        _effectCom->Set_AttachedEffectMaterialReveal(1.f, 0.f);
    }

    if (!data.contains("custom_properties") || !data["custom_properties"].is_object())
        return;

    const json& customProperties = data["custom_properties"];
    _itemId = customProperties.value("item_id", string{});
    _displayName = customProperties.value("display_name", string{});
    _amount = customProperties.value("amount", 1u);
}

HRESULT Item::Ready_Components()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.7f, 0.f);
    obbDesc.extents = Vec3(0.9f, 0.7f, 0.9f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _colliderCom, &obbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Item);
    _colliderCom->Set_IsActive(true);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    if (_effectCom)
    {
        EffectCom::InspectorEffectSettings settings = _effectCom->Get_InspectorEffectSettings();
        settings.autoPlay = true;
        if (settings.effectName.empty())
            settings.effectName = kItemCrystalEffectName;
        settings.target = EffectCom::EffectPlayTarget::OwnerRoot;
        settings.layerTag = L"Layer_Effect";
        settings.localOffset = Vec3(0.f, kItemVisualBaseOffsetY, 0.f);
        settings.localRotation = Vec3::Zero;
        settings.scale = Vec3::One;
        settings.followTarget = false;
        settings.requiresManualRelease = true;

        _effectCom->Set_InspectorEffectSettings(settings);
        _visualEffectBaseLocalOffset = settings.localOffset;
    }

    return S_OK;
}

void Item::Update_VisualHover(float timeDelta)
{
    _visualHoverTimeSec += max(timeDelta, 0.f);

    if (_effectCom == nullptr)
        return;

    Vec3 hoverOffset = _visualEffectBaseLocalOffset;
    hoverOffset.y += sinf(_visualHoverTimeSec * kItemHoverFrequency) * kItemHoverAmplitude;
    _effectCom->Set_InspectorEffectLocalOffset(hoverOffset);
}

void Item::Update_SpawnReveal(float timeDelta)
{
    if (!_spawnRevealActive || _effectCom == nullptr)
        return;

    _spawnRevealElapsed += max(timeDelta, 0.f);
    const float revealRatio = clamp(_spawnRevealElapsed / _spawnRevealDuration, 0.f, 1.f);

    _effectCom->Set_AttachedEffectMaterialReveal(revealRatio, 1.f - revealRatio);

    if (revealRatio >= 1.f)
        _spawnRevealActive = false;
}

void Item::Try_Pickup()
{
    if (_pickedUp)
        return;

    _pickedUp = true;

    Apply_PickupReward();

    if (_hasPlayerOverlap)
    {
        _hasPlayerOverlap = false;
        CLIENT->RemoveItemInteractionOverlap();
        CLIENT->Clear_InteractionPrompt(this);
    }

    const string& logName = _displayName.empty() ? _itemId : _displayName;
    LOG_INFO("[ItemPickup] type={} id={} name={} amount={}", Get_ItemTypeName(), _itemId, logName, _amount);

    if (!_pickupEffectName.empty())
    {
        PlayEffectDesc eatEffectDesc{};
        eatEffectDesc.effectName = _pickupEffectName;
        eatEffectDesc.spawnBasisTarget = GetSharedPtr<GameObject>();
        eatEffectDesc.localOffset = Vec3(0.f, 0.7f, 0.f);
        CLIENT->Play_Effect(eatEffectDesc);
    }

    if (_effectCom)
        _effectCom->Stop_AllAttachedEffects();

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(false);
        _colliderCom->Clear_Overlap();
    }

    GAME->Remove_GameObject(_spawnedLevelIndex, GetSharedPtr<GameObject>());
}

void Item::Free()
{
    if (_effectCom)
        _effectCom->Stop_AllAttachedEffects();

    if (_hasPlayerOverlap)
    {
        _hasPlayerOverlap = false;
        CLIENT->RemoveItemInteractionOverlap();
        CLIENT->Clear_InteractionPrompt(this);
    }

    GameObject::Free();
}
