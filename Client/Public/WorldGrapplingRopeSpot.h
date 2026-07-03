#pragma once

#include "WorldInteractionSpot.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

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
    InteractionPromptType Get_InteractionPromptType(const Shared<Player_WorldBase>& activeHero) const override;

private:
    HRESULT Ready_Components();
    HRESULT Ready_ModelComponent();
    void Register_ModelComponent();
    HRESULT Ready_AABBCollider(
        const wstring& componentTag,
        const Vec3& center,
        const Vec3& extents,
        Shared<ColliderCom>& outCollider);
    HRESULT Ready_SphereCollider(
        const wstring& componentTag,
        const Vec3& center,
        float radius,
        Shared<ColliderCom>& outCollider);
    HRESULT Ready_DebugSphereCollider(
        const wstring& componentTag,
        const Vec3& center,
        float radius,
        Shared<ColliderCom>& outCollider);
    void Sync_RuntimeTuning();
    void Sync_AABBCollider(
        const Shared<ColliderCom>& collider,
        Vec3& center,
        Vec3& extents,
        Vec3& lastSyncedCenter,
        Vec3& lastSyncedExtents,
        bool& hasSynced);
    void Sync_SphereCollider(
        const Shared<ColliderCom>& collider,
        Vec3& center,
        float& radius,
        Vec3& lastSyncedCenter,
        float& lastSyncedRadius,
        bool& hasSynced);
    void Refresh_CandidateFromLockRay(const Shared<Player_WorldBase>& activeHero);
    Vec3 Resolve_ColliderWorldCenter(const Shared<ColliderCom>& collider, const Vec3& fallbackLocalOffset) const;
    bool Is_ActiveHeroInsideGate(const Shared<Player_WorldBase>& activeHero) const;
    bool Is_GateCollider(const Shared<ColliderCom>& collider) const;
    bool Is_CameraLockRayHittingGrapplePoint() const;
    bool Try_GetCameraLockRay(Vec3& outOrigin, Vec3& outDirection) const;
    HRESULT Bind_ShaderResources();

private:
    Shared<ColliderCom> _gateCollider{};
    Shared<ColliderCom> _grapplePointCollider{};
    Shared<ColliderCom> _landingDebugCollider{};
    Shared<ModelCom> _model{};
    Shared<ShaderCom> _shader{};

    bool _hasActiveHeroOverlap = false;

    wstring _modelPath = L"Models/World/Grapple/Grapple.model";

    Vec3 _gateColliderCenter = Vec3{ 0.f, 0.f, 0.f };
    Vec3 _gateColliderExtents = Vec3{ 1.5f, 2.f, 1.5f };
    Vec3 _lastSyncedGateColliderCenter = Vec3::Zero;
    Vec3 _lastSyncedGateColliderExtents = Vec3::Zero;
    bool _hasSyncedGateCollider = false;

    Vec3 _grapplePointColliderCenter = Vec3{ 0.f, 3.f, 0.f };
    float _grapplePointColliderRadius = 0.45f;
    Vec3 _lastSyncedGrapplePointColliderCenter = Vec3::Zero;
    float _lastSyncedGrapplePointColliderRadius = 0.f;
    bool _hasSyncedGrapplePointCollider = false;

    Vec3 _landingDebugColliderCenter = Vec3{ 0.f, 3.f, 0.f };
    float _landingDebugColliderRadius = 0.35f;
    Vec3 _lastSyncedLandingDebugColliderCenter = Vec3::Zero;
    float _lastSyncedLandingDebugColliderRadius = 0.f;
    bool _hasSyncedLandingDebugCollider = false;
    float _maxLockRayDistance = 80.f;

public:
    static Shared<WorldGrapplingRopeSpot> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
