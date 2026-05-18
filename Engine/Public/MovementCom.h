#pragma once

#include "Component.h"
//#include "PhysX_Types.h"

NS_BEGIN(Engine)

class TransformCom;
class ColliderCom;
class PhysicsCharacterController;

struct PhysXQueryHit;

class ENGINE_DLL MovementCom : public Component
{
    GENERATED_COMPONENT(MovementCom)

public:
    enum class MoveBackend { Sweep, PhysXCCT, };

    struct MovementInput
    {
        float moveX = 0.f;
        float moveZ = 0.f;

        bool  isRunPressed = false;
        bool  isJumpDown = false;

        bool isWalkDown{ false };

        bool isInteractionDown{ false };
        bool isSuddenAttack{ false };
        
        bool isMouseLDown{ false };
        bool isMouseRPress{ false };
        bool isMouseRUp{ false };
    };

    struct SweepDesc
    {
        float skinWidth = 0.03f;
        float groundSnapUp = 0.35f;
        float groundSnapDown = 0.55f;
        float walkableNormalY = 0.65f;
        float groundY = 0.f;
    };

    struct CCTDesc
    {
        float radius = 0.32f;
        float height = 1.50f;
        float stepOffset = 0.35f;
        float contactOffset = 0.03f;
        float slopeLimitDeg = 49.0f;
        float groundLerpSpeed = 15.f;
    };

    struct MoveDesc
    {
        float walkSpeed = 4.f;
        float runSpeed = 7.f;
        float sprintSpeed = 10.f;
        float turnSpeedDegree = 180.f;
        float turnResponse = 12.f;

        float jumpVelocity = 40.f;

        float gravity = -20.f;

        MoveBackend backend = MoveBackend::Sweep;
        SweepDesc sweep{};
        CCTDesc cct{};
    };

    struct FollowMoveCommand
    {
        bool enabled = false;
        Vec3 worldDir = Vec3::Zero;
        bool sprintFollow = false;   // 거리 벌어져서 sprint로 전환?
    };

public:
    MovementCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MovementCom(const MovementCom& Prototype);
    virtual ~MovementCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

    void    Update_Movement(float timeDelta);

public:
    void Apply_Input(const MovementInput& input) { _input = input; }

    MoveBackend Get_MoveBackend() const { return _moveDesc.backend; }
    void Set_MoveBackend(MoveBackend backend);

    void Set_Transform(const Shared<TransformCom> transform)
    {
        _transformCom = transform;
    }

    const MoveDesc&         Get_MoveDesc()      { return _moveDesc; }
    const MovementInput&    Get_MoveInput()     { return _input;}

    bool Is_Grounded() const            { return _isGrounded; }
    bool Is_Jumping()  const            { return _isJumping; }
    bool Is_Moving()   const;
    bool Is_Running()  const            { return _input.isRunPressed; }

    // 땅에 충돌되고 있을 때에만 가능
    bool Can_Jump() const               { return _isGrounded; }

    // 강제 상태 전환
    void Force_SetGroundY(float groundY);
    void Force_SetPositionY(float posY);

    void Stop_AllMovement();
    void Stop_HorizontalMovement();
    void Stop_VerticalMovement();

    Vec3 Get_Velocity() { return _velocity; }
    void Set_Velocity(const Vec3& velocity) { _velocity = velocity; }

    void Set_FollowMoveCommand(const FollowMoveCommand& command) { _followMove = command; }
    void Clear_FollowMoveCommand() { _followMove = {}; }

private:
    void Start_Jump();

    void Update_Rotation(float timeDelta);
    void Update_HorizontalVelocity();
    void Update_VerticalVelocity(float timeDelta);

    void Apply_Velocity(float timeDelta);

    void Resolve_Ground();

    bool Resolve_MoveDirection(Vec3& outWorldDir) const;
    bool Resolve_CameraRelativeMoveDirection(Vec3& outWorldDir) const;

private:
    void Move_WithCollision(const Vec3& displacement);
    void Move_Horizontal(const Vec3& displacement);
    void Move_Vertical(const Vec3& displacement);

    bool Sweep_Move(
        const Vec3& displacement,
        bool passWalkableGround,
        PhysXQueryHit* outHit = nullptr,
        float* outMovedDistance = nullptr);

    void Resolve_Ground_WithSnap();
    bool Is_WalkableGround(const Vec3& normal) const;

    void Create_CharacterController();
    void Destroy_CharacterController();

    Vec3 Get_CCTFootPosition_FromTransform() const; // 현재 위치 -> CCT 발 위치 환산
    void Sync_Transform_FromCCT(const Vec3& footPosition, float timeDelta, bool smoothGrounding); // CCT 결과 -> Transform

private:
    MoveDesc         _moveDesc{};
    MovementInput    _input{};
    FollowMoveCommand _followMove{};

    Shared<TransformCom> _transformCom;

    Vec3 _velocity = Vec3::Zero;
    float _currentTurnSpeedDegree = 0.f;

    bool _isGrounded = true;
    bool _isJumping = false;

    Shared<ColliderCom> _colliderCom;
    Shared<PhysicsCharacterController> _characterController;

public:
    static Shared<MovementCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
