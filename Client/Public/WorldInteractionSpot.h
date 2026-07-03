#pragma once

#include "GameObject.h"

NS_BEGIN(Client)

class Player_WorldBase;

class IWorldInteractionProvider
{
public:
    enum class WorldInteractionType
    {
        WallClimb,
        RopeTouch,
        RopeLook,
    };

    enum class RopeDirection
    {
        None,
        Up,
        Down,
        Launch,
    };

    enum class RopeInteractionMode
    {
        Touch,
        Look,
    };

    struct WorldInteractionRequest
    {
        WorldInteractionType type = WorldInteractionType::WallClimb;
        RopeDirection ropeDirection = RopeDirection::None;

        Vec3 startPoint = Vec3::Zero;
        Vec3 targetPoint = Vec3::Zero;
        Vec3 wallPoint = Vec3::Zero;
        Vec3 ropePoint = Vec3::Zero;
        Vec3 targetRopePoint = Vec3::Zero;
        Vec3 faceDir = Vec3::Look;

        float warpEndProgress = 0.9f;
    };

public:
    virtual ~IWorldInteractionProvider() = default;

    virtual bool Can_Interact(const Shared<Player_WorldBase>& activeHero) const = 0;
    virtual bool Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const = 0;
    virtual const wstring& Get_InteractionPrompt() const = 0;
    virtual InteractionPromptType Get_InteractionPromptType(const Shared<Player_WorldBase>&) const { return InteractionPromptType::None; }
    virtual int32 Get_InteractionPriority() const { return 0; }
};

class WorldInteractionSpot abstract : public GameObject, public IWorldInteractionProvider
{
public:
    WorldInteractionSpot(const ComPtr<Device>& device, const ComPtr<Context>& context);
    WorldInteractionSpot(const WorldInteractionSpot& prototype);
    ~WorldInteractionSpot() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

public:
    bool Can_Interact(const Shared<Player_WorldBase>& activeHero) const override;
    const wstring& Get_InteractionPrompt() const override { return _promptText; }
    int32 Get_InteractionPriority() const override { return _priority; }

protected:
    void Build_CommonRequest(WorldInteractionRequest& outRequest) const;
    Vec3 Resolve_WorldPoint(const Vec3& localOffset) const;
    Vec3 Resolve_WorldDirection(const Vec3& localDirection) const;
    void Register_CandidateIfActiveHero(const Shared<ColliderCom>& other);
    bool Is_ActiveHeroCollider(const Shared<ColliderCom>& other, Shared<Player_WorldBase>* outActiveHero = nullptr) const;

protected:
    Vec3 _startPointOffset = Vec3::Zero;
    Vec3 _targetPointOffset = Vec3(0.f, 2.f, 0.f);
    Vec3 _wallPointOffset = Vec3(0.f, 1.f, 0.f);
    Vec3 _ropePointOffset = Vec3(0.f, 1.f, 0.f);
    Vec3 _faceDirection = Vec3::Look;

    float _warpEndProgress = 0.9f;
    int32 _priority = 0;
    wstring _promptText = L"Interact";
};

NS_END
