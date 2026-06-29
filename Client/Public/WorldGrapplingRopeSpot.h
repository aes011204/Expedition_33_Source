#pragma once

#include "WorldInteractionSpot.h"

NS_BEGIN(Client)

class WorldGrapplingRopeSpot final : public WorldInteractionSpot
{
    GENERATED_GAMEOBJECT(WorldGrapplingRopeSpot, ObjectType::Trigger)

public:
    WorldGrapplingRopeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context);
    WorldGrapplingRopeSpot(const WorldGrapplingRopeSpot& prototype);
    ~WorldGrapplingRopeSpot() override = default;

public:
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;
    bool Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const override;
    void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    bool Can_Interact(const Shared<Player_WorldBase>& activeHero) const override;
    bool Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const override;

private:
    HRESULT Ready_Components();
    HRESULT Ready_AABBCollider(
        const wstring& componentTag,
        const Vec3& center,
        const Vec3& extents,
        Shared<ColliderCom>& outCollider);
    void Refresh_CandidateFromLockRay(const Shared<Player_WorldBase>& activeHero);
    bool Is_GateCollider(const Shared<ColliderCom>& collider) const;
    bool Is_CameraLockRayHittingGrapplePoint() const;
    bool Try_GetCameraLockRay(Vec3& outOrigin, Vec3& outDirection) const;

private:
    Shared<ColliderCom> _gateCollider{};
    Shared<ColliderCom> _grapplePointCollider{};

    bool _hasActiveHeroOverlap = false;

    Vec3 _gateColliderCenter = Vec3{ 0.f, 0.f, 0.f };
    Vec3 _gateColliderExtents = Vec3{ 1.5f, 2.f, 1.5f };

    Vec3 _grapplePointColliderCenter = Vec3{ 0.f, 3.f, 0.f };
    Vec3 _grapplePointColliderExtents = Vec3{ 0.45f, 0.45f, 0.45f };
    float _maxLockRayDistance = 12.f;

public:
    static Shared<WorldGrapplingRopeSpot> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
