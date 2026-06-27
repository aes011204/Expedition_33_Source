#pragma once
#include "Player_State.h"

NS_BEGIN(Engine)
struct PhysXQueryHit;
NS_END

NS_BEGIN(Client)

class Player_ParkourAction final : public Player_State
{
public:
    Player_ParkourAction(const Shared<Player_WorldBase>& owner);
    ~Player_ParkourAction() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    static Shared<Player_ParkourAction> Create(const Shared<Player_WorldBase>& owner);
    enum class ParkourActionType
    {
        LowMantle,
        HighMantle,
    };

    bool Try_PrepareAction(ParkourActionType actionType);
    void Clear_PreparedAction();
    void Free() override;

private:
    struct ParkourMoveRequest
    {
        ParkourActionType actionType = ParkourActionType::LowMantle;
        Vec3 startPoint = Vec3::Zero;
        Vec3 targetPoint = Vec3::Zero;
        Vec3 faceDir = Vec3::Look;
    };

    bool Build_ParkourRequest(ParkourActionType actionType, ParkourMoveRequest& outRequest) const;
    bool Is_EnvironmentHit(const PhysXQueryHit& hit) const;

private:
    ParkourMoveRequest _request{};
    Vec3 _warpStartPos = Vec3::Zero;
    bool _hasWarp = false;
    bool _hasPreparedRequest = false;
};

NS_END
