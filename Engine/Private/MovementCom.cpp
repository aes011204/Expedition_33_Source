#include "MovementCom.h"
#include "GameObject.h"
#include "TransformCom.h"
#include "Camera.h"
#include "ColliderCom.h"
#include "Helper_Math.h"
#include "PhysX_Types.h"

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
    , _isGrounded{ Prototype._isGrounded }
    , _isJumping{ Prototype._isJumping }
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
    _velocity = Vec3::Zero;
    _currentTurnSpeedDegree = 0.f;

    _isGrounded = true;
    _isJumping = false;

    return S_OK;
}

void MovementCom::BeginPlay()
{
    Component::BeginPlay();

    auto owner = Get_Owner();
    CHECK_NULL(owner);

    _transformCom = owner->Get_Transform();
    CHECK_NULL(_transformCom);

    _colliderCom = owner->Get_Component<ColliderCom>();
}

void MovementCom::Update_Movement(float timeDelta)
{
    CHECK_NULL(_transformCom);

    Update_Rotation(timeDelta);
    Start_Jump();

    Apply_Velocity(timeDelta);
    Resolve_Ground_WithSnap();
}

bool MovementCom::Is_Moving() const
{
    if (_followMove.enabled)
        return _followMove.worldDir.LengthSquared() > 0.f;

    return _input.moveX != 0.f || _input.moveZ != 0.f;
}

void MovementCom::Force_SetGroundY(float groundY)
{
    _moveDesc.groundY = groundY;
}

void MovementCom::Force_SetPositionY(float posY)
{
    CHECK_NULL(_transformCom);

    Vec3 position = _transformCom->Get_WorldPosition();
    position.y = posY;
    _transformCom->Set_WorldPosition(position);
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

    // 바닥에 충돌 중이 아니면, 점프 불가능
    if (!_isGrounded)
        return;

    if (!_input.isJumpDown)
        return;

    _velocity.y = _moveDesc.jumpVelocity;
    _isGrounded = false;
    _isJumping = true;
}

void MovementCom::Update_Rotation(float timeDelta)
{
    Vec3 desiredDir = Vec3::Zero;
    if (!Resolve_MoveDirection(desiredDir))
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
    _transformCom->Add_LocalRotation(deltaRotation);
}

void MovementCom::Update_HorizontalVelocity()
{
    Vec3 desiredDir = Vec3::Zero;
    if (!Resolve_MoveDirection(desiredDir))
    {
        _velocity.x = 0.f;
        _velocity.z = 0.f;
        return;
    }

    float moveSpeed = 0.f;

    if (_followMove.enabled)
    {
        moveSpeed = _followMove.sprintFollow  ? _moveDesc.sprintSpeed : _moveDesc.runSpeed;
    }
    else if (_input.moveX != 0.f || _input.moveZ != 0.f)
    {
        if (_input.isRunPressed)
        {
            moveSpeed = _moveDesc.sprintSpeed;
        }
        else if (_input.isWalkDown)
        {
            moveSpeed = _moveDesc.walkSpeed;
        }
        else
        {
            moveSpeed = _moveDesc.runSpeed;
        }
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
    Update_HorizontalVelocity();
    Update_VerticalVelocity(timeDelta);

    const Vec3 displacement = _velocity * timeDelta;

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

    if (position.y > _moveDesc.groundY)
        return;

    position.y = _moveDesc.groundY;
    _transformCom->Set_WorldPosition(position);

    _velocity.y = 0.f;
    _isGrounded = true;
    _isJumping = false;
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
//
//    Vec3 cameraForward = cameraTransform->Get_WorldForward();
//    Vec3 cameraRight = cameraTransform->Get_WorldRight();

    Vec3 cameraForward = Vec3::Zero;
    Vec3 cameraRight = Vec3::Zero;

    if(!activeCamera->Try_GetFlatMoveBasis(cameraForward, cameraRight))
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
        position += pushOut * (_moveDesc.skinWidth + 0.001f);
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

    const float moveDistance = max(0.f, hit.distance - _moveDesc.skinWidth);
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
        max(0.05f, min(shapeDesc.extents.x, shapeDesc.extents.z) - _moveDesc.skinWidth);

    const Vec3 probeOrigin =
        feetCenter + Vec3(0.f, _moveDesc.groundSnapUp + footProbeRadius, 0.f);

    PhysXQueryHit hit{};
    const float probeDistance =
        _moveDesc.groundSnapUp + _moveDesc.groundSnapDown + footProbeRadius;
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
        (_moveDesc.groundSnapUp + footProbeRadius) - max(0.f, hit.distance - _moveDesc.skinWidth);
    if (targetDeltaY > _moveDesc.groundSnapUp + 0.001f ||
        targetDeltaY < -_moveDesc.groundSnapDown - 0.001f)
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
}

bool MovementCom::Is_WalkableGround(const Vec3& normal) const
{
    return normal.y >= _moveDesc.walkableNormalY;
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
    Component::Free();
}
