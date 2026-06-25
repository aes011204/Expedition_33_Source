#pragma once

#include "WorldInteractionSpot.h"

NS_BEGIN(Client)

class WorldLedgeSpot final : public WorldInteractionSpot
{
    GENERATED_GAMEOBJECT(WorldLedgeSpot, ObjectType::Trigger)

public:
    WorldLedgeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context);
    WorldLedgeSpot(const WorldLedgeSpot& prototype);
    ~WorldLedgeSpot() override = default;

public:
    bool Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const override;

public:
    static Shared<WorldLedgeSpot> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
