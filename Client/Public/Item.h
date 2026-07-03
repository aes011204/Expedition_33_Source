#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class EffectCom;

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
    void    Update_VisualHover(float timeDelta);
    void    Try_Pickup();

protected:
    Shared<ColliderCom> _colliderCom{};
    Shared<EffectCom>   _effectCom{};

    string              _itemId{};
    string              _displayName{};
    uint32              _amount = 1;

private:
    bool                _hasPlayerOverlap = false;
    bool                _pickedUp = false;
    uint32              _spawnedLevelIndex = 0;
    Vec3                _visualEffectBaseLocalOffset = Vec3(0.f, 0.7f, 0.f);
    float               _visualHoverTimeSec = 0.f;

public:
    void    Free() override;
};

NS_END
