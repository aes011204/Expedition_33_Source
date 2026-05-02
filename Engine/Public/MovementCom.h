#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class TransformCom;

class ENGINE_DLL MovementCom : public Component
{
    GENERATED_COMPONENT(MovementCom)

public:
    struct MoveDesc
    {
        float walkSpeed = 4.f;
        float runSpeed = 7.f;
        float turnSpeedDegree = 180.f;

        float jumpVelocity = 8.f;

        float gravity = -20.f;

        // 아직 groundY 추적이 안되니까 임시로 0으로 세팅
        float groundY = 0.f;
    };

    // 플레이어는 InputCom에서 들어온 값으로 세팅
    struct MoveInput
    {
        float moveAxis = 0.f;
        float turnAxis = 0.f;

        bool  isRunPressed = false;
        bool  isJumpDown = false;
    };

public:
    MovementCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MovementCom(const MovementCom& Prototype);
    virtual ~MovementCom() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

    void    Update_Movement(float timeDelta);

public:
    void Apply_Input(const MoveInput& input) { _input = input; }

    void Set_Transform(const Shared<TransformCom> transform)
    {
        _transformCom = transform;
    }

    const MoveDesc& Get_MoveDesc()      { return _moveDesc; }
    const MoveInput& Get_MoveInput()    { return _input;}

    bool Is_Grounded() const            { return _isGrounded; }
    bool Is_Jumping() const             { return _isJumping; }
    bool Is_Moving() const              { return _input.moveAxis != 0.f; }
    bool Is_Turning() const             { return _input.turnAxis != 0.f; }
    bool Is_Running() const             { return _input.isRunPressed; }

    // 땅에 충돌되고 있을 때에만 가능
    bool Can_Jump() const               { return _isGrounded; }

    // 강제 상태 전환
    void Force_SetGroundY(float groundY);
    void Force_SetPositionY(float posY);

    void Stop_AllMovement();
    void Stop_HorizontalMovement();
    void Stop_VerticalMovement();

private:
    void Start_Jump();

    void Update_Rotation(float timeDelta);
    void Update_HorizontalVelocity();
    void Update_VerticalVelocity(float timeDelta);
    void Apply_Velocity(float timeDelta);

    void Resolve_Ground();

private:
    MoveDesc    _moveDesc{};
    MoveInput   _input{};

    Shared<TransformCom> _transformCom;

    Vec3 _velocity = Vec3::Zero;

    bool _isGrounded = true;
    bool _isJumping = false;

public:
    static Shared<MovementCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
