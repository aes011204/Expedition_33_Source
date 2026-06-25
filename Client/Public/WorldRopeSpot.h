#pragma once

#include "WorldInteractionSpot.h"

NS_BEGIN(Client)

class WorldRopeSpot final : public WorldInteractionSpot
{
    GENERATED_GAMEOBJECT(WorldRopeSpot, ObjectType::Trigger)

public:
    WorldRopeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context);
    WorldRopeSpot(const WorldRopeSpot& prototype);
    ~WorldRopeSpot() override = default;

public:
    void Update(float timeDelta) override;
    bool Can_Interact(const Shared<Player_WorldBase>& activeHero) const override;
    bool Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const override;

private:
    bool Is_CameraLookingAtZone() const;

private:
    RopeInteractionMode _ropeMode = RopeInteractionMode::Touch;
    RopeDirection _ropeDirection = RopeDirection::Up;

    Vec3 _lookZoneOffset = Vec3(0.f, 1.f, 0.f);
    float _lookZoneRadius = 0.75f;
    float _maxLookDistance = 12.f;

public:
    static Shared<WorldRopeSpot> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
