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
    HRESULT Initialize(void* arg) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    bool Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const override;

private:
    HRESULT Ready_Components();

private:
    Shared<ColliderCom> _colliderCom{};

    Vec3 _colliderCenter = Vec3(0.f, 1.f, 0.f);
    Vec3 _colliderExtents = Vec3(0.5f, 1.f, 0.5f);

public:
    static Shared<WorldLedgeSpot> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
