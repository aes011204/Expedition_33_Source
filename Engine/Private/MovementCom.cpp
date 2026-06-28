#include "MovementCom.h"
#include "GameObject.h"
#include "TransformCom.h"
#include "Camera.h"
#include "ColliderCom.h"
#include "Debug_Profile.h"
#include "Helper_Math.h"
#include "PhysicsCharacterController.h"
#include "PhysX_Types.h"

namespace
{
    constexpr float kJumpInputGraceSec = 0.10f;
    constexpr float kGroundJumpGraceSec = 0.08f;
    constexpr float kCCTGroundDetachGraceSec = 0.12f;
    constexpr float kCCTGroundDetachFallSpeed = -3.5f;
    constexpr float kCCTGroundStickSpeed = 6.f;
    constexpr float kInputBlockedStopDamping = 14.f;
    constexpr float kStillGroundJitterSpeedSq = 0.01f;
    constexpr float kStillGroundXZDeadZone = 0.02f;
    constexpr float kStillGroundYDeadZone = 0.06f;
}

IMPLEMENT_REFLECTION(MovementCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Walk Speed", _moveDesc.walkSpeed, 0.f, 30.f, 0.1f);
    PROPERTY_FLOAT("Run Speed", _moveDesc.runSpeed, 0.f, 30.f, 0.1f);
    PROPERTY_FLOAT("Sprint Speed", _moveDesc.sprintSpeed, 0.f, 30.f, 0.1f);
    PROPERTY_FLOAT("Turn Speed Degree", _moveDesc.turnSpeedDegree, 0.f, 720.f, 1.f);
    PROPERTY_FLOAT("Turn Response", _moveDesc.turnResponse, 0.f, 30.f, 0.1f);

    PROPERTY_FLOAT("Jump Velocity", _moveDesc.jumpVelocity, 0.f, 100.f, 0.1f);
    PROPERTY_FLOAT("Gravity", _moveDesc.gravity, -100.f, 0.f, 0.1f);

    PROPERTY_VEC3("Velocity", _velocity, 10.f);

    return true;
}

MovementCom::MovementCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

MovementCom::MovementCom(const MovementCom& Prototype)
    : Component(Prototype)
    , _moveDesc{ Prototype._moveDesc }
    , _input{ Prototype._input }
    , _transformCom{ nullptr }
    , _velocity{ Prototype._velocity }
    , _currentTurnSpeedDegree{ 0.f }
    , _idleGroundProbeAccum{ 0.f }
    , _jumpInputGraceRemain{ 0.f }
    , _groundJumpGraceRemain{ 0.f }
    , _cctGroundDetachGraceRemain{ 0.f }
#ifdef _DEBUG
    , _idleGroundProbeDebugLogAccum{ 0.f }
#endif
    , _isGrounded{ Prototype._isGrounded }
    , _isJumping{ Prototype._isJumping }
    , _didInputJumpStart{ false }
    , _jumpStartHorizontalVelocity{ Vec3::Zero }
{
}

HRESULT MovementCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT MovementCom::Initialize(void* arg)
{
    if (arg != nullptr)
        _moveDesc = *static_cast<MoveDesc*>(arg);
    else
        _moveDesc = {};

    _input = {};
    _followMove = {};

    _transformCom.reset();
    _colliderCom.reset();
    _characterController.reset();

    _velocity = Vec3::Zero;
    _currentTurnSpeedDegree = 0.f;
    _idleGroundProbeAccum = 0.f;
    _jumpInputGraceRemain = 0.f;
    _groundJumpGraceRemain = 0.f;
    _cctGroundDetachGraceRemain = 0.f;

#ifdef _DEBUG
    _idleGroundProbeDebugLogAccum = 0.f;
#endif

    _isGrounded = true;
    _isJumping = false;
    _didInputJumpStart = false;
    _jumpStartHorizontalVelocity = Vec3::Zero;
    _suppressCCTTransformSync = false;

    return S_OK;
}

void MovementCom::BeginPlay()
{
    Component::BeginPlay();

    auto owner = Get_Owner();
    CHECK_NULL(owner);

    _transformCom = owner->Get_Transform();
    CHECK_NULL(_transformCom);

    if (_colliderCom == nullptr)
        _colliderCom = owner->Get_Component<ColliderCom>();

    if (_moveDesc.backend == MoveBackend::PhysXCCT)
        Create_CharacterController();
}

void MovementCom::Update_Movement(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("Movement::Update");

    if (!_transformCom)
        return;

    _didInputJumpStart = false;

    if (_input.isJumpDown)
        _jumpInputGraceRemain = kJumpInputGraceSec;
    else
        _jumpInputGraceRemain = max(0.f, _jumpInputGraceRemain - timeDelta);

    if (_isGrounded)
        _groundJumpGraceRemain = kGroundJumpGraceSec;
    else
        _groundJumpGraceRemain = max(0.f, _groundJumpGraceRemain - timeDelta);

    Update_Rotation(timeDelta);
    Start_Jump();

    Apply_Velocity(timeDelta);

    if (_moveDesc.backend == MoveBackend::Sweep || _characterController == nullptr)
    {
        const bool needsImmediateGroundProbe =
            !_isGrounded ||
            _isJumping ||
            fabsf(_velocity.y) > 0.0001f ||
            Is_Moving();

#ifdef _DEBUG
        if (needsImmediateGroundProbe && !Is_Moving())
        {
            _idleGroundProbeDebugLogAccum += timeDelta;

            if (_idleGroundProbeDebugLogAccum >= 1.f)
            {
                Shared<GameObject> owner = Get_Owner();
                const string ownerPrefabName = owner ? owner->Get_SourcePrefabName() : string{};

                LOG_DEBUG(
                    "MovementCom idle immediate probe this={} owner={} prefab='{}' grounded={} jumping={} velY={:.4f} moving={} followEnabled={} backend={}",
                    static_cast<const void*>(this),
                    static_cast<const void*>(owner.get()),
                    ownerPrefabName,
                    _isGrounded,
                    _isJumping,
                    _velocity.y,
                    Is_Moving(),
                    _followMove.enabled,
                    _moveDesc.backend == MoveBackend::Sweep ? "Sweep" : "PhysXCCT");

                _idleGroundProbeDebugLogAccum = 0.f;
            }
        }
        else
        {
            _idleGroundProbeDebugLogAccum = 0.f;
        }
#endif

        if (needsImmediateGroundProbe)
        {
            _idleGroundProbeAccum = 0.f;
            Resolve_Ground_WithSnap();
        }
        else
        {
            _idleGroundProbeAccum += timeDelta;

            if (_idleGroundProbeAccum >= 0.10f)
            {
                _idleGroundProbeAccum = 0.f;
                Resolve_Ground_WithSnap();
            }
        }
    }

}

void MovementCom::Move_ExternalDisplacement(const Vec3& displacement, float timeDelta)
{
    CHECK_NULL(_transformCom);

    if (displacement.LengthSquared() <= 0.000001f)
        return;

    const float dt = max(timeDelta, 0.0001f);

    if (_moveDesc.backend == MoveBackend::PhysXCCT)
    {
        if (_characterController == nullptr)
            Create_CharacterController();

        if (_characterController != nullptr)
        {
            const CharacterControllerMoveResult moveResult =
                _characterController->Move(displacement, dt);

            const bool smoothGrounding = moveResult.collisionDown;
            if (!_suppressCCTTransformSync)
                Sync_Transform_FromCCT(moveResult.footPosition, dt, smoothGrounding, false);

            if (moveResult.collisionDown)
            {
                _isGrounded = true;
                _isJumping = false;
                _velocity.y = 0.f;
                _jumpStartHorizontalVelocity = Vec3::Zero;
                _cctGroundDetachGraceRemain = kCCTGroundDetachGraceSec;
            }
            else if (!_isJumping && _isGrounded && _cctGroundDetachGraceRemain > 0.f)
            {
                _cctGroundDetachGraceRemain = max(0.f, _cctGroundDetachGraceRemain - dt);
            }
            else
            {
                _cctGroundDetachGraceRemain = 0.f;
                _isGrounded = false;
            }

            if (moveResult.collisionUp && _velocity.y > 0.f)
                _velocity.y = 0.f;

            return;
        }
    }

    if (_colliderCom == nullptr || _colliderCom->Get_PxShape() == nullptr)
    {
        _transformCom->Add_WorldPosition(displacement);
        return;
    }

    Move_WithCollision(displacement);
    Resolve_Ground_WithSnap();
}

void MovementCom::Set_MoveBackend(MoveBackend backend)
{
    if (_moveDesc.backend == backend)
    {
        if (backend == MoveBackend::PhysXCCT && _characterController == nullptr && _transformCom != nullptr)
        {
            Create_CharacterController(); // active로 다시 올라왔는데 CCT가 없으면 복구
        }

        return;
    }

    if (backend == MoveBackend::Sweep)
    {
        Destroy_CharacterController(); // follower는 비싼 CCT를 들고 있지 않게 한다. -> 이건 테스트
        _moveDesc.backend = MoveBackend::Sweep;
        return;
    }

    _moveDesc.backend = MoveBackend::PhysXCCT;

    if (_transformCom != nullptr && _characterController == nullptr)
    {
        Create_CharacterController(); // active hero가 됐을 때 현재 위치 기준으로 CCT 생성
    }
}

bool MovementCom::Is_Moving() const
{
    if (_followMove.enabled)
        return _followMove.worldDir.LengthSquared() > 0.f;

    return _input.moveX != 0.f || _input.moveZ != 0.f;
}

void MovementCom::Force_SetGroundY(float groundY)
{
    _moveDesc.sweep.groundY = groundY;
}

void MovementCom::Force_SetPositionY(float posY)
{
    CHECK_NULL(_transformCom);

    Vec3 position = _transformCom->Get_WorldPosition();
    position.y = posY;
    _transformCom->Set_WorldPosition(position);
}

void MovementCom::Sync_RuntimePositionToTransform()
{
    if (_transformCom == nullptr)
    {
        Shared<GameObject> owner = Get_Owner();
        _transformCom = owner ? owner->Get_Transform() : nullptr;
    }

    CHECK_NULL(_transformCom);

    if (_moveDesc.backend != MoveBackend::PhysXCCT)
        return;

    if (_characterController == nullptr)
        Create_CharacterController();

    if (_characterController == nullptr)
        return;

    _characterController->Set_FootPosition(Get_CCTFootPosition_FromTransform());
}

void MovementCom::Stop_AllMovement()
{
    _velocity = Vec3::Zero;
}

void MovementCom::Stop_HorizontalMovement()
{
    _velocity.x = 0.f;
    _velocity.z = 0.f;
}

void MovementCom::Stop_VerticalMovement()
{
    _velocity.y = 0.f;
}

void MovementCom::Start_Jump()
{
    CHECK_NULL(_transformCom);

    if (_groundJumpGraceRemain <= 0.f)
        return;

    if (_jumpInputGraceRemain <= 0.f)
        return;

    Update_HorizontalVelocity();

    _jumpStartHorizontalVelocity = _velocity;
    _jumpStartHorizontalVelocity.y = 0.f; // 점프 시작 순간의 공중 XZ를 그대로 유지한다.

    _velocity.y = _moveDesc.jumpVelocity;
    _isGrounded = false;
    _isJumping = true;
    _didInputJumpStart = true;
    _cctGroundDetachGraceRemain = 0.f; // 실제 점프가 시작되면 경사 보행 유예는 바로 비운다.

    _jumpInputGraceRemain = 0.f;
    _groundJumpGraceRemain = 0.f;
}

void MovementCom::Update_Rotation(float timeDelta)
{
    if (_input.isLockLook) return;

    if (_followMove.enabled && !_followMove.rotateFollow)
        return;

    Vec3 desiredDir = Vec3::Zero;

    if (_isJumping && _jumpStartHorizontalVelocity.LengthSquared() > 0.f)
    {
        desiredDir = _jumpStartHorizontalVelocity;
        desiredDir.y = 0.f;
        desiredDir.Normalize(); // 공중에서는 점프 시작 방향 기준으로만 yaw를 유지한다.
    }
    else if (_input.forcedDirection.LengthSquared() > 0.f)
    {
        desiredDir = _input.forcedDirection;
        desiredDir.y = 0.f;

        if (desiredDir.LengthSquared() > 0.f)
            desiredDir.Normalize();
        else
            desiredDir = Vec3::Look;
    }
    else if (!Resolve_MoveDirection(desiredDir))
    {
        _currentTurnSpeedDegree = Engine::Math::Lerp_Damp(_currentTurnSpeedDegree, 0.f, _moveDesc.turnResponse, timeDelta);
        return;
    }

    Vec3 forward = _transformCom->Get_WorldForward();
    forward.y = 0.f;

    if (forward.LengthSquared() > 0.f)
        forward.Normalize();
    else
        forward = Vec3::Look;

    float dot = forward.Dot(desiredDir);
    dot = std::clamp(dot, -1.f, 1.f);

    const float angle = acosf(dot);
    if (angle <= 0.0001f)
    {
        _currentTurnSpeedDegree = Engine::Math::Lerp_Damp(_currentTurnSpeedDegree, 0.f, _moveDesc.turnResponse, timeDelta);
        return;
    }

    Vec3 cross = forward.Cross(desiredDir);
    const float sign = (cross.y >= 0.f) ? 1.f : -1.f;

    const float angleRatio = std::clamp(angle / XMConvertToRadians(90.f), 0.f, 1.f);
    const float targetTurnSpeedDegree = _moveDesc.turnSpeedDegree * std::lerp(0.25f, 1.f, angleRatio);
    _currentTurnSpeedDegree = Engine::Math::Lerp_Damp(
        _currentTurnSpeedDegree,
        targetTurnSpeedDegree,
        _moveDesc.turnResponse,
        timeDelta);

    const float maxStep = XMConvertToRadians(_currentTurnSpeedDegree) * timeDelta;
    const float step = min(angle, maxStep);

    Quat deltaRotation = Quat::CreateFromAxisAngle(Vec3::Up, step * sign);
    // Movement yaw는 parent local 축이 아니라 World Up 기준으로 누적한다.
    _transformCom->Add_WorldRotationQuaternion(deltaRotation);
}

void MovementCom::Update_HorizontalVelocity(float timeDelta)
{
    if (_isJumping)
    {
        _velocity.x = _jumpStartHorizontalVelocity.x;
        _velocity.z = _jumpStartHorizontalVelocity.z;
        return; // 점프 중에는 입력을 다시 읽지 않고 jump start XZ를 그대로 쓴다.
    }

    if (_input.isLockLook)
    {
        Vec3 velocity = _velocity;
        Vec3 look = _transformCom->Get_WorldForward();
        look.y = 0.f;
        look.Normalize();
        Vec3 right = _transformCom->Get_WorldRight();
        right.y = 0.f;
        right.Normalize();

        _velocity.x = right.x * velocity.x + look.x * velocity.z;
        _velocity.z = right.z * velocity.x + look.z * velocity.z;

        return;
    }

    Vec3 desiredDir = Vec3::Zero;
    if (!Resolve_MoveDirection(desiredDir))
    {
        if (_input.smoothStopOnNoMoveInput)
        {
            Vec3 horizontalVelocity(_velocity.x, 0.f, _velocity.z);
            horizontalVelocity = Engine::Math::Lerp_Damp(
                horizontalVelocity,
                Vec3::Zero,
                kInputBlockedStopDamping,
                timeDelta);

            if (horizontalVelocity.LengthSquared() <= kStillGroundJitterSpeedSq)
                horizontalVelocity = Vec3::Zero;

            _velocity.x = horizontalVelocity.x;
            _velocity.z = horizontalVelocity.z;
            return;
        }

        _velocity.x = 0.f;
        _velocity.z = 0.f;
        return;
    }

    float moveSpeed = 0.f;

    if (_followMove.enabled)
    {
        moveSpeed =
            _followMove.walkFollow ? _moveDesc.walkSpeed :
            (_followMove.sprintFollow ? _moveDesc.sprintSpeed : _moveDesc.runSpeed);
    }
    else if (_input.moveX != 0.f || _input.moveZ != 0.f)
    {
        moveSpeed =
            _input.isRunPressed ? _moveDesc.sprintSpeed :
            (_input.isWalkDown ? _moveDesc.walkSpeed : _moveDesc.runSpeed);
    }

    _velocity.x = desiredDir.x * moveSpeed;
    _velocity.z = desiredDir.z * moveSpeed;
}

void MovementCom::Update_VerticalVelocity(float timeDelta)
{
    if (_isGrounded)
        return;

    _velocity.y += _moveDesc.gravity * timeDelta;
}

void MovementCom::Apply_Velocity(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("Movement::ApplyVelocity");

    Update_HorizontalVelocity(timeDelta);
    Update_VerticalVelocity(timeDelta);

    Vec3 displacement = _velocity * timeDelta;

    if (_moveDesc.backend == MoveBackend::PhysXCCT && _characterController != nullptr)
    {
        ENGINE_PROFILE_SCOPE_DETAIL("Movement::ApplyVelocity::CCT");

        // 정지 상태에서는 일단 스킵
        if (_isGrounded && !_isJumping && displacement.LengthSquared() <= 0.000001f)
            return;

        const Vec3 horizontalVelocity(_velocity.x, 0.f, _velocity.z);
        const bool shouldStickToGround =
            _isGrounded &&
            !_isJumping &&
            _velocity.y <= 0.f &&
            horizontalVelocity.LengthSquared() > kStillGroundJitterSpeedSq;

        if (shouldStickToGround)
        {
            const float stickDistance = min(
                _moveDesc.sweep.groundSnapDown,
                max(_moveDesc.cct.contactOffset, kCCTGroundStickSpeed * timeDelta));
            displacement.y = min(displacement.y, -stickDistance); // 내려가는 경사에서는 CCT move 자체가 먼저 바닥을 밟게 한다.
        }

        const CharacterControllerMoveResult moveResult =
            _characterController->Move(displacement, timeDelta);

        Vec3 resolvedFootPosition = moveResult.footPosition;
        const bool snappedToGround =
            !moveResult.collisionDown &&
            !_isJumping &&
            _isGrounded &&
            Try_SnapCCTToGround(resolvedFootPosition);

        const bool groundedAfterMove = moveResult.collisionDown || snappedToGround;
        const float currentFootY = _transformCom->Get_WorldPosition().y;
        const bool isDescendingGroundAdjust =
            groundedAfterMove &&
            resolvedFootPosition.y < currentFootY - 0.001f;
        const bool smoothGrounding =
            groundedAfterMove &&
            !isDescendingGroundAdjust &&
            _velocity.y <= 0.f;
        const bool suppressSmallGroundAdjust =
            smoothGrounding &&
            horizontalVelocity.LengthSquared() <= kStillGroundJitterSpeedSq;

        if (!_suppressCCTTransformSync)
            Sync_Transform_FromCCT(resolvedFootPosition, timeDelta, smoothGrounding, suppressSmallGroundAdjust);

        const bool hitWalkableDown = groundedAfterMove && _velocity.y <= 0.f;
        const bool canUseDetachGrace =
            !_isJumping &&                          // 실제 점프 중에는 공중 상태를 가리면 안 된다.
            _isGrounded &&                          // 직전까지 grounded였던 경우만 경사 보행 보호를 건다.
            _velocity.y > kCCTGroundDetachFallSpeed; // 충분히 빠르게 내려가면 진짜 낙하로 본다.

        if (hitWalkableDown)
        {
            _isGrounded = true;
            _velocity.y = 0.f;      // 착지 후 하강 속도 제거
            _isJumping = false;
            _jumpStartHorizontalVelocity = Vec3::Zero;
            _cctGroundDetachGraceRemain = kCCTGroundDetachGraceSec; // 지면을 밟은 직후에는 다음 한두 프레임 ground flicker를 흡수한다.
        }
        else if (canUseDetachGrace && _cctGroundDetachGraceRemain > 0.f)
        {
            _cctGroundDetachGraceRemain = max(0.f, _cctGroundDetachGraceRemain - timeDelta);
            _isGrounded = true; // 약한 경사 보행 중 잠깐 비는 down flag는 계속 grounded로 본다.

            if (_velocity.y < 0.f)
                _velocity.y = 0.f; // 가짜 낙하 속도가 Jump 상태 소비로 이어지지 않게 막는다.
        }
        else
        {
            _cctGroundDetachGraceRemain = 0.f;
            _isGrounded = false;
        }

        if (moveResult.collisionUp && _velocity.y > 0.f)
            _velocity.y = 0.f;      // 머리 충돌 시 상승 속도 차단

        return;
    }

    ENGINE_PROFILE_SCOPE_DETAIL("Movement::ApplyVelocity::Sweep");

    if (_colliderCom == nullptr || _colliderCom->Get_PxShape() == nullptr)
    {
        Vec3 position = _transformCom->Get_WorldPosition();
        position += displacement;
        _transformCom->Set_WorldPosition(position);
        return;
    }

    Move_WithCollision(displacement);
}

void MovementCom::Resolve_Ground()
{
    Vec3 position = _transformCom->Get_WorldPosition();

    if (position.y > _moveDesc.sweep.groundY)
        return;

    position.y = _moveDesc.sweep.groundY;
    _transformCom->Set_WorldPosition(position);

    _velocity.y = 0.f;
    _isGrounded = true;
    _isJumping = false;
    _jumpStartHorizontalVelocity = Vec3::Zero;
}

bool MovementCom::Resolve_MoveDirection(Vec3& outWorldDir) const
{
    outWorldDir = Vec3::Zero;

    if (_followMove.enabled)
    {
        outWorldDir = _followMove.worldDir;
        outWorldDir.y = 0.f;

        if (outWorldDir.LengthSquared() <= 0.f)
            return false;

        outWorldDir.Normalize();
        return true;
    }

    return Resolve_CameraRelativeMoveDirection(outWorldDir);
}

bool MovementCom::Resolve_CameraRelativeMoveDirection(Vec3& outWorldDir) const
{
    outWorldDir = Vec3::Zero;

    Vec2 moveAxis(_input.moveX, _input.moveZ);
    if (moveAxis.LengthSquared() <= 0.f)
        return false;

    if (moveAxis.LengthSquared() > 1.f)
        moveAxis.Normalize();

    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr)
        return false;
    
    Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();
    if (cameraTransform == nullptr)
        return false;
    
    //    Vec3 cameraForward = cameraTransform->Get_WorldForward();
    //    Vec3 cameraRight = cameraTransform->Get_WorldRight();

    Vec3 cameraForward = Vec3::Zero;
    Vec3 cameraRight = Vec3::Zero;

    if (!activeCamera->Try_GetFlatMoveBasis(cameraForward, cameraRight))
        return false;

    cameraForward.y = 0.f;
    cameraRight.y = 0.f;

    if (cameraForward.LengthSquared() > 0.f)
        cameraForward.Normalize();
    else
        cameraForward = Vec3::Look;

    if (cameraRight.LengthSquared() > 0.f)
        cameraRight.Normalize();
    else
        cameraRight = Vec3::Right;

    outWorldDir = cameraRight * moveAxis.x + cameraForward * moveAxis.y;
    outWorldDir.y = 0.f;

    if (outWorldDir.LengthSquared() <= 0.f)
        return false;

    outWorldDir.Normalize();
    return true;
}

void MovementCom::Move_WithCollision(const Vec3& displacement)
{
    Move_Vertical(Vec3(0.f, displacement.y, 0.f));
    Move_Horizontal(Vec3(displacement.x, 0.f, displacement.z));
}

void MovementCom::Move_Horizontal(const Vec3& displacement)
{
    if (displacement.LengthSquared() <= 0.f)
        return;

    PhysXQueryHit hit{};
    float movedDistance = 0.f;

    // 수평 이동은 벽에 닿으면 우선 충돌 직전까지만 전진한다.
    const bool blocked = Sweep_Move(displacement, false, &hit, &movedDistance);
    if (!blocked)
        return;

    Vec3 moveDir = displacement;
    const float totalDistance = moveDir.Length();
    if (totalDistance <= 0.f)
        return;

    moveDir /= totalDistance;

    const float remainDistance = max(0.f, totalDistance - movedDistance);
    if (remainDistance <= 0.0001f)
        return;

    Vec3 remain = moveDir * remainDistance;

    // 남은 이동량에서 벽 법선 방향 성분을 제거해서 벽면을 따라 미끄러지게 만든다.
    Vec3 slide = remain - hit.normal * remain.Dot(hit.normal);
    slide.y = 0.f;

    const float intoWall = slide.Dot(hit.normal);
    if (intoWall < 0.f)
    {
        slide -= hit.normal * intoWall;
    }

    if (slide.LengthSquared() <= 0.0001f)
        return;

    Vec3 pushOut = hit.normal;
    pushOut.y = 0.f;

    if (pushOut.LengthSquared() > 0.f)
    {
        pushOut.Normalize();

        Vec3 position = _transformCom->Get_WorldPosition();
        position += pushOut * (_moveDesc.sweep.skinWidth + 0.001f);
        _transformCom->Set_WorldPosition(position);
    }

    Sweep_Move(slide, false);
}

void MovementCom::Move_Vertical(const Vec3& displacement)
{
    Sweep_Move(displacement, false);
}

bool MovementCom::Sweep_Move(const Vec3& displacement, bool passWalkableGround, PhysXQueryHit* outHit,
    float* outMovedDistance)
{
    if (outHit)
        *outHit = {};

    if (outMovedDistance)
        *outMovedDistance = 0.f;

    if (displacement.LengthSquared() <= 0.f)
        return false;

    Vec3 direction = displacement;
    const float distance = direction.Length();
    if (distance <= 0.f)
        return false;

    direction /= distance;

    PhysXQueryHit hit{};
    const bool blocked = GAME->Sweep_Collider(
        _colliderCom.get(),
        _transformCom->Get_WorldMatrix(),
        direction,
        distance,
        hit);

    if (!blocked)
    {
        Vec3 position = _transformCom->Get_WorldPosition();
        position += displacement;
        _transformCom->Set_WorldPosition(position);

        if (outMovedDistance)
            *outMovedDistance = distance; // 끝까지 전진했으면 전체 거리만큼 이동

        return false;
    }

    if (passWalkableGround && Is_WalkableGround(hit.normal))
    {
        Vec3 position = _transformCom->Get_WorldPosition();
        position += displacement;
        _transformCom->Set_WorldPosition(position);

        if (outHit)
            *outHit = hit;

        if (outMovedDistance)
            *outMovedDistance = distance;

        return true;
    }

    const float moveDistance = max(0.f, hit.distance - _moveDesc.sweep.skinWidth);
    if (moveDistance > 0.f)
    {
        Vec3 position = _transformCom->Get_WorldPosition();
        position += direction * moveDistance;
        _transformCom->Set_WorldPosition(position);
    }

    if (outHit)
        *outHit = hit; // 벽 법선을 Move_Horizontal에서 slide 계산에 쓴다.

    if (outMovedDistance)
        *outMovedDistance = moveDistance; // 충돌 전 실제 전진 거리만 넘긴다.

    if (direction.y < 0.f && Is_WalkableGround(hit.normal))
    {
        _velocity.y = 0.f;
        _isGrounded = true;
        _isJumping = false;
        _jumpStartHorizontalVelocity = Vec3::Zero;
    }
    else if (direction.y > 0.f)
    {
        _velocity.y = min(0.f, _velocity.y);
    }

    return true;
}

void MovementCom::Resolve_Ground_WithSnap()
{
    if (_colliderCom == nullptr || _colliderCom->Get_PxShape() == nullptr)
    {
        Resolve_Ground();
        return;
    }

    if (_velocity.y > 0.f)
        return;

    const ColliderShapeDesc& shapeDesc = _colliderCom->Get_ShapeDesc();

    const Vec3 feetCenter =
        _transformCom->Get_WorldPosition() +
        Vec3(0.f, shapeDesc.center.y - shapeDesc.extents.y, 0.f);

    // 몸통 전체가 아니라 발밑 작은 구체로만 바닥을 찾는다.
    const float footProbeRadius =
        max(0.05f, min(shapeDesc.extents.x, shapeDesc.extents.z) - _moveDesc.sweep.skinWidth);

    const Vec3 probeOrigin =
        feetCenter + Vec3(0.f, _moveDesc.sweep.groundSnapUp + footProbeRadius, 0.f);

    PhysXQueryHit hit{};
    const float probeDistance =
        _moveDesc.sweep.groundSnapUp + _moveDesc.sweep.groundSnapDown + footProbeRadius;
    const bool hasGround = GAME->Sweep_Sphere(
        probeOrigin,
        Vec3(0.f, -1.f, 0.f),
        probeDistance,
        footProbeRadius,
        Collision_Preset::Player_Body,
        hit);

    if (!hasGround || !Is_WalkableGround(hit.normal))
    {
        _isGrounded = false;
        return;
    }

    const float targetDeltaY =
        (_moveDesc.sweep.groundSnapUp + footProbeRadius) - max(0.f, hit.distance - _moveDesc.sweep.skinWidth);
    if (targetDeltaY > _moveDesc.sweep.groundSnapUp + 0.001f ||
        targetDeltaY < -_moveDesc.sweep.groundSnapDown - 0.001f)
    {
        _isGrounded = false;
        return;
    }

    if (fabsf(targetDeltaY) > 0.0001f)
    {
        Vec3 position = _transformCom->Get_WorldPosition();
        position.y += targetDeltaY;
        _transformCom->Set_WorldPosition(position);
    }

    _velocity.y = 0.f;
    _isGrounded = true;
    _isJumping = false;
    _jumpStartHorizontalVelocity = Vec3::Zero;
}

bool MovementCom::Is_WalkableGround(const Vec3& normal) const
{
    return normal.y >= _moveDesc.sweep.walkableNormalY;
}

void MovementCom::Create_CharacterController()
{
    CharacterControllerDesc desc{};
    desc.radius = _moveDesc.cct.radius;
    desc.height = _moveDesc.cct.height;
    desc.stepOffset = _moveDesc.cct.stepOffset;
    desc.contactOffset = _moveDesc.cct.contactOffset;
    desc.slopeLimitDeg = _moveDesc.cct.slopeLimitDeg;
    desc.groundLerpSpeed = _moveDesc.cct.groundLerpSpeed;
    desc.preset = _colliderCom ? _colliderCom->Get_CollisionPreset() : Collision_Preset::Player_WorldBody;

    _characterController = PhysicsCharacterController::Create(
        GAME->Get_PhysXManager(), desc, Get_CCTFootPosition_FromTransform());

    if (_characterController == nullptr)
    {
        LOG_WARN("MovementCom: failed to create PhysX CCT, falling back to sweep movement.");
        _moveDesc.backend = MoveBackend::Sweep; // CCT 생성 실패 시 기존 sweep 경로로 되돌린다.
    }
}

void MovementCom::Destroy_CharacterController()
{
    if (_characterController == nullptr)
        return;

    _characterController->Free(); 
    _characterController.reset();
}

Vec3 MovementCom::Get_CCTFootPosition_FromTransform() const
{
    CHECK_NULL(_transformCom, Vec3::Zero);
    return _transformCom->Get_WorldPosition();
}

bool MovementCom::Try_SnapCCTToGround(Vec3& inOutFootPosition)
{
    if (_colliderCom == nullptr || _colliderCom->Get_PxShape() == nullptr)
        return false;

    if (_velocity.y > 0.f)
        return false;

    const ColliderShapeDesc& shapeDesc = _colliderCom->Get_ShapeDesc();

    const float footProbeRadius =
        max(0.05f, min(shapeDesc.extents.x, shapeDesc.extents.z) - _moveDesc.sweep.skinWidth);

    const Vec3 probeOrigin =
        inOutFootPosition + Vec3(0.f, _moveDesc.sweep.groundSnapUp + footProbeRadius, 0.f);

    PhysXQueryHit hit{};
    const float probeDistance =
        _moveDesc.sweep.groundSnapUp + _moveDesc.sweep.groundSnapDown + footProbeRadius;

    const bool hasGround = GAME->Sweep_Sphere(
        probeOrigin,
        Vec3(0.f, -1.f, 0.f),
        probeDistance,
        footProbeRadius,
        _colliderCom->Get_CollisionPreset(),
        hit);

    if (!hasGround || !Is_WalkableGround(hit.normal))
        return false;

    const float targetDeltaY =
        (_moveDesc.sweep.groundSnapUp + footProbeRadius) - max(0.f, hit.distance - _moveDesc.sweep.skinWidth);
    if (targetDeltaY > 0.001f ||
        targetDeltaY < -_moveDesc.sweep.groundSnapDown - 0.001f)
    {
        return false;
    }

    if (fabsf(targetDeltaY) <= 0.0001f)
        return true;

    inOutFootPosition.y += targetDeltaY; // CCT 자체 foot도 같이 내려야 다음 프레임 ground 판정이 이어진다.

    if (_characterController != nullptr)
        _characterController->Set_FootPosition(inOutFootPosition);

    return true;
}

void MovementCom::Sync_Transform_FromCCT(const Vec3& footPosition, float timeDelta, bool smoothGrounding, bool suppressSmallGroundAdjust)
{
    CHECK_NULL(_transformCom);

    Vec3 position = _transformCom->Get_WorldPosition();

    if (suppressSmallGroundAdjust)
    {
        if (fabsf(footPosition.x - position.x) > kStillGroundXZDeadZone)
            position.x = footPosition.x;

        if (fabsf(footPosition.z - position.z) > kStillGroundXZDeadZone)
            position.z = footPosition.z;

        if (fabsf(footPosition.y - position.y) > kStillGroundYDeadZone)
        {
            if (smoothGrounding)
            {
                const float t = clamp(timeDelta * _moveDesc.cct.groundLerpSpeed, 0.f, 1.f);
                position.y = lerp(position.y, footPosition.y, t);
            }
            else
            {
                position.y = footPosition.y;
            }
        }

        _transformCom->Set_WorldPosition(position);
        return; // 제자리/저속 grounded 상태의 작은 CCT 흔들림은 카메라까지 전달하지 않는다.
    }

    position.x = footPosition.x;
    position.z = footPosition.z;

    if (smoothGrounding)
    {
        const float t = clamp(timeDelta * _moveDesc.cct.groundLerpSpeed, 0.f, 1.f);
        position.y = lerp(position.y, footPosition.y, t); // 지면에 붙어 있을 때만 Y를 부드럽게 반영
    }
    else
    {
        position.y = footPosition.y; // 공중에서는 보간하지 않고 즉시 반영
    }

    _transformCom->Set_WorldPosition(position);
}

Shared<MovementCom> MovementCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MovementCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MovementCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> MovementCom::Clone(void* arg)
{
    auto clone = make_shared<MovementCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MovementCom");
        return nullptr;
    }

    return clone;
}

void MovementCom::Free()
{
    Destroy_CharacterController();

    Component::Free();
}
