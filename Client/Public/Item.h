#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class Item abstract : public GameObject
{
public:
    Item(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Item(const Item& prototype);
    ~Item() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

    json    To_Json() const override;
    void    From_Json(const json& data) override;

protected:
    virtual const char* Get_ItemTypeName() const = 0;
    virtual void Apply_PickupReward() {};

private:
    HRESULT Ready_Components();
    void    Try_Pickup();

protected:
    Shared<ColliderCom> _colliderCom{};
    string              _itemId{};
    string              _displayName{};
    uint32              _amount = 1;

private:
    bool                _hasPlayerOverlap = false;
    bool                _pickedUp = false;
    uint32              _spawnedLevelIndex = 0;

public:
    void    Free() override;
};

NS_END
