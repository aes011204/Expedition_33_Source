#include "Item.h"

#include "Bounding_OBB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "Player_WorldBase.h"

Item::Item(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Item::Item(const Item& prototype)
    : GameObject(prototype)
    , _itemId(prototype._itemId)
    , _displayName(prototype._displayName)
    , _amount(prototype._amount)
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

    UNREFERENCED_PARAMETER(arg);
    return S_OK;
}

void Item::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (_pickedUp || !_hasPlayerOverlap)
        return;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->IsDialogueActive() || CLIENT->IsDialogueInteractLocked())
        return;

    if (GAME->KeyDown(KEY_TYPE::F))
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
    }

    _pickedUp = false;
    _spawnedLevelIndex = GAME->Current_LevelIndex();

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();
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

    return S_OK;
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
    }

    const string& logName = _displayName.empty() ? _itemId : _displayName;
    LOG_INFO("[ItemPickup] type={} id={} name={} amount={}", Get_ItemTypeName(), _itemId, logName, _amount);

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(false);
        _colliderCom->Clear_Overlap();
    }

    GAME->Remove_GameObject(_spawnedLevelIndex, GetSharedPtr<GameObject>());
}

void Item::Free()
{
    if (_hasPlayerOverlap)
    {
        _hasPlayerOverlap = false;
        CLIENT->RemoveItemInteractionOverlap();
    }

    GameObject::Free();
}
