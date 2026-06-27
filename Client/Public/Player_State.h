#pragma once
#include "State.h"
#include "MovementCom.h"
#include "WorldInteractionSpot.h"

NS_BEGIN(Client)

class Player_WorldBase;
class Body_Player;

class PlayerControllerCom;
class PlayerStateMachineCom;
class InputCom;

class Player_State abstract : public State
{
public:
    Player_State(const Shared<Player_WorldBase>& owner);
    ~Player_State() override = default;

public:
    virtual HRESULT Initialize() override;
    virtual void Begin_Play() override;
    virtual void Refresh_BodyPlayer();

public:
    virtual bool EnterInvoke() override;
    virtual void Update(float timeDelta) override;
    virtual void LateUpdate(float timeDelta) override;
    virtual void ExitInvoke() override;

protected:
    bool Has_MoveInput() const;
    bool Is_WalkToggled() const;
    const MovementCom::MovementInput& Get_MovementInput() const;
    Vec3 Get_Velocity() const;

    bool Has_LocomotionIntent() const;   // active 입력 또는 follower 이동 의도
    bool IsSprintMove() const;      
    bool IsWalkMove() const;
    bool Is_MovementBlocked() const;     // follower blocked 상태
    bool IsFalling() const;
    bool IsFreeAimShoot() const; // FreeAim 상태에서 좌클
    float Get_ParkourLowProbeDistance() const;
    float Get_ParkourHighProbeDistance() const;
    Vec3 Get_ParkourLowProbeCenter() const;
    Vec3 Get_ParkourHighProbeCenter() const;

    void Draw_ParkourDebugRays() const;

    bool Try_BuildWorldInteractionRequest(WorldInteractionSpot::WorldInteractionRequest& outRequest, Shared<GameObject>& outSpot) const;
    bool IsEnterWorldAction() const; 
    bool IsEnterFreeAim() const;
    bool IsEnterJump() const;       
    bool IsEnterMove() const;       
    bool IsEnterIdle() const;       

protected:
    Weak<Player_WorldBase>            _owner{};
    PlayerControllerCom*              _controllerCom{ nullptr };
    PlayerStateMachineCom*            _stateCom{ nullptr };
    MovementCom*                      _movementCom{ nullptr };
    Body_Player*                      _bodyPlayer{ nullptr };

public:
    virtual void Free() override;
    
};

NS_END
