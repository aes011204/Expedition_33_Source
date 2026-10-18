#pragma once

#include "WorldInteractionSpot.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(Client)

class EffectCom;

class WorldUpDownRopeSpot final : public WorldInteractionSpot
{
    GENERATED_GAMEOBJECT(WorldUpDownRopeSpot, ObjectType::Trigger)

public:
    WorldUpDownRopeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context);
    WorldUpDownRopeSpot(const WorldUpDownRopeSpot& prototype);
    ~WorldUpDownRopeSpot() override = default;

public:
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
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
    enum class RopeEntry
    {
        None,
        Top,
        Bottom,
    };

private:
    HRESULT Ready_Components();
    HRESULT Ready_ModelComponent();
    HRESULT Ready_EffectComponent();
    void Register_ModelComponent();
    void Sync_RuntimeTuning();
    void Sync_EntryCollider(
        const Shared<ColliderCom>& collider,
        Vec3& center,
        Vec3& extents,
        Vec3& lastSyncedCenter,
        Vec3& lastSyncedExtents,
        bool& hasSynced);
    void Sync_RopeEffectSettings();
    HRESULT Ready_EntryCollider(
        const wstring& componentTag,
        const Vec3& center,
        const Vec3& extents,
        Shared<ColliderCom>& outCollider);
    HRESULT Bind_ShaderResources();
    void Play_RopeEffect();
    void Set_ActiveEntryFromCollider(const Shared<ColliderCom>& self, const Shared<ColliderCom>& other);
    RopeEntry Resolve_EntryFromCollider(const Shared<ColliderCom>& collider) const;
    bool Is_EntryActive() const;
    Vec3 Build_RopeIdlePoint(float idleY) const;

private:
    Shared<ColliderCom> _topEntryCollider{};
    Shared<ColliderCom> _bottomEntryCollider{};
    Shared<ModelCom> _model{};
    Shared<ShaderCom> _shader{};
    Shared<EffectCom> _effectCom{};

    RopeEntry _activeEntry = RopeEntry::None;

    bool _ropeEffectAutoPlay = true;
    wstring _ropeEffectName = L"Effects\\Assets\\Map\\FX_VerticalRope.effect.json";
    Vec3 _ropeEffectOffset = Vec3::Zero;
    Vec3 _ropeEffectRotation = Vec3::Zero;
    Vec3 _ropeEffectScale = Vec3::One;
    Vec2 _ropeCenterXZ = Vec2::Zero;

    Vec3 _topColliderCenter = Vec3(-2.f, 14.f, -3.5f);
    Vec3 _topColliderExtents = Vec3(0.6f, 0.6f, 0.6f);
    Vec3 _lastSyncedTopColliderCenter = Vec3::Zero;
    Vec3 _lastSyncedTopColliderExtents = Vec3::Zero;
    bool _hasSyncedTopCollider = false;
    float _topRopeIdleY = 17.f;
    Vec3 _topStartPointOffset = Vec3(0.f, 4.f, 0.f);
    Vec3 _topTargetPointOffset = Vec3(0.f, 0.f, 0.f);
    Vec3 _topRopePointOffset = Vec3(0.f, 4.f, 0.f);

    Vec3 _bottomColliderCenter = Vec3(0.f, 0.f, 0.f);
    Vec3 _bottomColliderExtents = Vec3(0.6f, 0.6f, 0.6f);
    Vec3 _lastSyncedBottomColliderCenter = Vec3::Zero;
    Vec3 _lastSyncedBottomColliderExtents = Vec3::Zero;
    bool _hasSyncedBottomCollider = false;
    float _bottomRopeIdleY = 0.f;
    Vec3 _bottomStartPointOffset = Vec3(0.f, 0.f, 0.f);
    Vec3 _bottomTargetPointOffset = Vec3(0.f, 4.f, 0.f);
    Vec3 _bottomRopePointOffset = Vec3(0.f, 0.f, 0.f);

public:
    static Shared<WorldUpDownRopeSpot> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
