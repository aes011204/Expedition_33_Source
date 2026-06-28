#pragma once

#include "Item.h"

NS_BEGIN(Client)

class Item_Pictos : public Item
{
    GENERATED_GAMEOBJECT(Item_Pictos, ObjectType::Item)

public:
    enum class PictosPickupType
    {
        EnergisingTurn,
        GlassCannon,
        WarmingUp,
        Roulette,
        AugmentedAttack,
        AugmentedAim,
    };

public:
    Item_Pictos(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Item_Pictos(const Item_Pictos& prototype);
    ~Item_Pictos() override = default;

protected:
    const char* Get_ItemTypeName() const override { return "Pictos"; }
    void Apply_PickupReward() override;

private:
    string Resolve_PictosId() const;

private:
    PictosPickupType _pictosType = PictosPickupType::EnergisingTurn;

public:
    static Shared<Item_Pictos> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
