#pragma once

#include "Component.h"
#include "MovementCom.h"

NS_BEGIN(Engine)
    class MovementCom;
NS_END

NS_BEGIN(Client)

class InputCom;
class PlayerStateMachineCom;

class PlayerControllerCom : public Component
{
    GENERATED_COMPONENT(PlayerControllerCom)

public:
    struct RigidControl
    {
        bool rigidMovement{ false };
        bool rigidJump{ false };
        bool rigidLook{ false };
        Vec3 rigidLookDir{ Vec3::Zero };
        Vec3 rigidVelocity{ Vec3::Zero };
    };

    struct FollowControl
    {
        bool enabled = false;       // follower 제어 모드인지
        bool hasMoveIntent = false;
        bool sprintFollow  = false; // 너무 멀어져 sprint 추격 중인지
        bool blocked = false;       // 직전 이동 시 실제로 못 움직였는지
        Vec3 worldDir = Vec3::Zero; // follower가 이동해야 할 월드 방향
    };

public:
    PlayerControllerCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PlayerControllerCom(const PlayerControllerCom& Prototype);
    virtual ~PlayerControllerCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

    void    Update_Controller(float timeDelta);
    void    Update_InputState();
    void    Update_Movement(float timeDelta);

public:
    Vec3    Get_Velocity() const;

public:
    bool    Has_MoveInput() const;
    bool    Is_WalkToggled() const;
    bool    Is_Grounded() const;
    bool    Is_RightMouseClicked() const;

    const MovementCom::MovementInput& Get_MovementInput() const;
    bool    Get_UseControlYaw() const { return _bUseControllerRotationYaw; }
    void    Set_UseControlYaw(bool check) { _bUseControllerRotationYaw = check; }

    // 입력을 받을 수 있는지. 조작중인 메인 플레이어만 입력 들어오게
    void    Set_ControlEnabled(bool enabled) { _controlEnabled = enabled; }
    bool    Is_ControlEnabled() const { return _controlEnabled; }

public:
    const RigidControl& Get_RigidControl() const { return _rigidControl; }
    void Set_RigidControl(const RigidControl& rigidControl) { _rigidControl = rigidControl; }

public: /* Follower */ 
    void Set_FollowControl(const FollowControl& followControl) { _followControl = followControl; }
    void Clear_FollowControl() { _followControl = {}; }

    bool Is_FollowControlled() const { return _followControl.enabled; }
    bool Has_LocomotionIntent() const;
    bool Should_SprintMove() const;
    bool Should_WalkMove() const;
    bool Is_MovementBlocked() const;

    // 점프/월드 액션 같은 직접 입력 허용 여부, follower에는 막히게
    bool Allows_ActionInput() const; 


private:
    void Check_RigidInput();

private:
    // Owner가 어차피 weak라 순환참조 걱정없어서 Shared 사용
    Shared<MovementCom>             _movementCom{ nullptr };
    Shared<InputCom>                _inputCom{ nullptr };

private:
    // 카메라 회전값에 따라서 플레이에 Yaw를 회전시킬 것인지?
    // 일단 33원정대 스타일에서는 필요없을거같음.
    bool                 _bUseControllerRotationYaw = true;
    float                _testRefectionValue = 1.f;

    bool                 _controlEnabled = true;

    MovementCom::MovementInput  _currentInput{};
    RigidControl                _rigidControl{};
    bool                        _isWalkToggled = false;

    FollowControl               _followControl{};

public:
    static Shared<PlayerControllerCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
