#include "Transform.h"
#include "Shader.h"

namespace Engine::Internal
{
constexpr float transformEpsilon = 1e-6f;
constexpr float minEditableScale = 1e-3f;
constexpr float minEditableSpeedPerSec = 0.f;
constexpr float minEditableDegreePerSec = 0.f;

float Resolve_ScaleSign(float value, float fallback = 1.f)
{
    if (value < 0.f)
        return -1.f;

    if (value > 0.f)
        return 1.f;

    return fallback < 0.f ? -1.f : 1.f;
}
}

Transform::Transform(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context }
{
}

Transform::Transform(const Transform& prototype)
    : Component{ prototype }
    , _localMatrix{ prototype._localMatrix }
    , _localSignedScale{ prototype._localSignedScale }
    , _hasParent{ false }
    , _localRotationCache{ prototype._localRotationCache }
    , _localEulerDegreeCache{ prototype._localEulerDegreeCache }
    , _worldMatrix{ prototype._worldMatrix }
    , _worldRevision{ 0 }
    , _observedParentWorldRevision{ 0 }
{
    Set_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
    Set_Dirty(TRANSFORM_DIRTY_FLAG::WORLD);
}

Transform::~Transform()
{
    Free();
}

HRESULT Transform::Initialize_Prototype()
{
    _localMatrix = Matrix::Identity;
    _localSignedScale = Vec3::One;
    _parent.reset();
    _hasParent = false;
    _localRotationCache = Quat::Identity;
    _localEulerDegreeCache = Vec3::Zero;
    _speedPerSec = 0.f;
    _radianPerSec = 0.f;
    _worldMatrix = Matrix::Identity;
    _worldRevision = 0;
    _observedParentWorldRevision = 0;
    _dirtyMask = static_cast<uint8>(TRANSFORM_DIRTY_FLAG::ROTATION) |
                 static_cast<uint8>(TRANSFORM_DIRTY_FLAG::WORLD);

    return S_OK;
}

HRESULT Transform::Initialize(void* arg)
{
    if (nullptr == arg)
        return S_OK;

    const TRANSFORM_DESC* desc = static_cast<TRANSFORM_DESC*>(arg);

    Set_SpeedPerSec(desc->speedPerSec);
    Set_DegreePerSec(desc->degreePerSec);
    Set_Position(desc->position);
    Set_Scale(desc->scale);
    Set_RotationEuler(Vec3(desc->pitch, desc->yaw, desc->roll));
    Set_Dirty(TRANSFORM_DIRTY_FLAG::WORLD);

    return S_OK;
}

void Transform::Update_WorldMatrix()
{
    Ensure_WorldMatrixUpToDate();
}

HRESULT Transform::Bind_ShaderResource(Shader* shader, const char* resourceName)
{
    Ensure_WorldMatrixUpToDate();
    return shader->Bind_Matrix(resourceName, reinterpret_cast<const XMFLOAT4X4*>(&Get_WorldMatrix()));
}

const Matrix& Transform::Get_WorldMatrix()
{
    Ensure_WorldMatrixUpToDate();
    return _worldMatrix;
}

Matrix Transform::Get_LocalMatrix() const
{
    return _localMatrix;
}

Vec3 Transform::Get_State(State state)
{
    const Matrix& worldMatrix = Get_WorldMatrix();

    switch (state)
    {
    case State::Right:
        return Vec3(worldMatrix._11, worldMatrix._12, worldMatrix._13);
    case State::Up:
        return Vec3(worldMatrix._21, worldMatrix._22, worldMatrix._23);
    case State::Look:
        return Vec3(worldMatrix._31, worldMatrix._32, worldMatrix._33);
    case State::Position:
        return Vec3(worldMatrix._41, worldMatrix._42, worldMatrix._43);
    default:
        return Vec3::Zero;
    }
}

void Transform::Set_State(State state, const Vec3& value)
{
    if (State::Position == state)
    {
        Set_Position(value);
        return;
    }

    const Vec3 rotationAxis = Resolve_NormalizedAxis(state);
    Set_LocalState(state, value);

    if (value.LengthSquared() > Internal::transformEpsilon)
    {
        Vec3 axis = value;
        axis.Normalize();
        const float axisSign = axis.Dot(rotationAxis) < 0.f ? -1.f : 1.f;
        Set_SignedScale(state, value.Length() * axisSign);
    }

    Sanitize_LocalBasis();
}

Vec3 Transform::Get_Position() const
{
    return Get_LocalState(State::Position);
}

void Transform::Set_Position(const Vec3& position)
{
    Set_LocalState(State::Position, position);
    Mark_LocalChanged(LOCAL_CHANGE_KIND::ROTATION_UNCHANGED);
}

Vec3 Transform::Get_WorldPosition()
{
    return Get_State(State::Position);
}

void Transform::Set_WorldPosition(const Vec3& position)
{
    const Shared<Transform> parent = _parent.lock();
    if (!parent)
    {
        Set_Position(position);
        return;
    }

    parent->Ensure_WorldMatrixUpToDate();

    const Matrix parentWorld = parent->_worldMatrix;
    const float determinant = XMVectorGetX(XMMatrixDeterminant(parentWorld));
    if (determinant > -Internal::transformEpsilon && determinant < Internal::transformEpsilon)
        return;

    const Matrix inverseParentWorld = parentWorld.Invert();
    Set_Position(Vec3::Transform(position, inverseParentWorld));
}

Vec3 Transform::Get_Scale() const
{
    return _localSignedScale;
}

void Transform::Set_Scale(const Vec3& scale)
{
    Sync_RotationCache_FromLocalMatrix();

    const Vec3 rightAxis = Resolve_NormalizedAxis(State::Right);
    const Vec3 upAxis = Resolve_NormalizedAxis(State::Up);
    const Vec3 lookAxis = Resolve_NormalizedAxis(State::Look);

    const Vec3 clampedScale = Clamp_EditableScale(scale);
    _localSignedScale = clampedScale;

    Set_LocalState(State::Right, rightAxis * clampedScale.x);
    Set_LocalState(State::Up, upAxis * clampedScale.y);
    Set_LocalState(State::Look, lookAxis * clampedScale.z);

    Mark_LocalChanged(LOCAL_CHANGE_KIND::ROTATION_UNCHANGED);
}

void Transform::Set_ScaleX(float scaleX)
{
    Set_Scale(Vec3(scaleX, Get_Scale().y, Get_Scale().z));
}

void Transform::Set_ScaleY(float scaleY)
{
    Set_Scale(Vec3(Get_Scale().x, scaleY, Get_Scale().z));
}

void Transform::Set_ScaleZ(float scaleZ)
{
    Set_Scale(Vec3(Get_Scale().x, Get_Scale().y, scaleZ));
}

Vec3 Transform::Get_RotationEuler()
{
    Sync_RotationCache_FromLocalMatrix();
    return _localEulerDegreeCache;
}

void Transform::Set_RotationEuler(const Vec3& eulerDegree)
{
    _localEulerDegreeCache = eulerDegree;

    const Vec3 eulerRadian(
        XMConvertToRadians(eulerDegree.x),
        XMConvertToRadians(eulerDegree.y),
        XMConvertToRadians(eulerDegree.z)
    );

    _localRotationCache = Quat::CreateFromYawPitchRoll(eulerRadian.y, eulerRadian.x, eulerRadian.z);
    _localRotationCache.Normalize();

    Rebuild_LocalBasis_FromRotationCache();
    Clear_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
}

const Quat& Transform::Get_RotationQuaternion()
{
    Sync_RotationCache_FromLocalMatrix();
    return _localRotationCache;
}

void Transform::Set_RotationQuaternion(const Quat& rotation)
{
    if (rotation.LengthSquared() <= Internal::transformEpsilon)
        _localRotationCache = Quat::Identity;
    else
    {
        _localRotationCache = rotation;
        _localRotationCache.Normalize();
    }

    Rebuild_LocalBasis_FromRotationCache();
    Clear_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
}

float Transform::Get_SpeedPerSec() const
{
    return _speedPerSec;
}

void Transform::Set_SpeedPerSec(float speedPerSec)
{
    _speedPerSec = speedPerSec < Internal::minEditableSpeedPerSec ? Internal::minEditableSpeedPerSec : speedPerSec;
}

float Transform::Get_DegreePerSec() const
{
    return XMConvertToDegrees(_radianPerSec);
}

void Transform::Set_DegreePerSec(float degreePerSec)
{
    const float clampedDegreePerSec = degreePerSec < Internal::minEditableDegreePerSec ? Internal::minEditableDegreePerSec : degreePerSec;
    _radianPerSec = XMConvertToRadians(clampedDegreePerSec);
}

void Transform::Set_Parent(const Shared<Transform>& parent)
{
    Ensure_WorldMatrixUpToDate();
    const Vec3 previousSignedScale = Get_Scale();
    const Matrix currentWorld = _worldMatrix;

    if (parent)
    {
        parent->Ensure_WorldMatrixUpToDate();

        const Matrix parentWorld = parent->_worldMatrix;
        const float determinant = XMVectorGetX(XMMatrixDeterminant(parentWorld));
        if (determinant > -Internal::transformEpsilon && determinant < Internal::transformEpsilon)
            return;

        _parent = parent;
        _hasParent = true;
        _localMatrix = currentWorld * parentWorld.Invert();
    }
    else
    {
        _parent.reset();
        _hasParent = false;
        _localMatrix = currentWorld;
    }

    _observedParentWorldRevision = 0;
    Sync_SignedScaleFromLocalMatrix(previousSignedScale);
    Sanitize_LocalBasis();
}

void Transform::Add_LocalRotation(const Quat& deltaRotation)
{
    Quat rotation = Get_RotationQuaternion();
    rotation = rotation * deltaRotation;

    if (rotation.LengthSquared() <= Internal::transformEpsilon)
        return;

    rotation.Normalize();
    Set_RotationQuaternion(rotation);
}

Vec3 Transform::Get_WorldForward()
{
    Vec3 forward = Get_State(State::Look);
    if (forward.LengthSquared() > Internal::transformEpsilon)
        forward.Normalize();

    return forward;
}

Vec3 Transform::Get_WorldRight()
{
    Vec3 right = Get_State(State::Right);
    if (right.LengthSquared() > Internal::transformEpsilon)
        right.Normalize();

    return right;
}

Vec3 Transform::Get_WorldUp()
{
    Vec3 up = Get_State(State::Up);
    if (up.LengthSquared() > Internal::transformEpsilon)
        up.Normalize();

    return up;
}

Vec3 Transform::Get_LocalForward()
{
    Sync_RotationCache_FromLocalMatrix();
    return Vec3::Transform(Vec3::Look, _localRotationCache);
}

Vec3 Transform::Get_LocalRight()
{
    Sync_RotationCache_FromLocalMatrix();
    return Vec3::Transform(Vec3::Right, _localRotationCache);
}

Vec3 Transform::Get_LocalUp()
{
    Sync_RotationCache_FromLocalMatrix();
    return Vec3::Transform(Vec3::Up, _localRotationCache);
}

void Transform::Scaling(float scaleX, float scaleY, float scaleZ)
{
    const Vec3 currentScale = Get_Scale();
    Set_Scale(Vec3(currentScale.x * scaleX, currentScale.y * scaleY, currentScale.z * scaleZ));
}

void Transform::ScaleX(float scaleX)
{
    Set_ScaleX(Get_Scale().x * scaleX);
}

void Transform::ScaleY(float scaleY)
{
    Set_ScaleY(Get_Scale().y * scaleY);
}

void Transform::ScaleZ(float scaleZ)
{
    Set_ScaleZ(Get_Scale().z * scaleZ);
}

void Transform::Go_Straight(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() + Get_WorldForward() * _speedPerSec * timeDelta);
}

void Transform::Go_Backward(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() - Get_WorldForward() * _speedPerSec * timeDelta);
}

void Transform::Go_Right(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() + Get_WorldRight() * _speedPerSec * timeDelta);
}

void Transform::Go_Left(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() - Get_WorldRight() * _speedPerSec * timeDelta);
}

void Transform::Go_Up(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() + Vec3::Up * _speedPerSec * timeDelta);
}

void Transform::Go_Down(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() - Vec3::Up * _speedPerSec * timeDelta);
}

void Transform::Rotate(const Vec3& axis, float degree)
{
    Vec3 axisValue = axis;
    if (axisValue.LengthSquared() <= Internal::transformEpsilon)
        return;

    axisValue.Normalize();
    Set_RotationQuaternion(Quat::CreateFromAxisAngle(axisValue, XMConvertToRadians(degree)));
}

void Transform::Turn(const Vec3& axis, float timeDelta)
{
    Vec3 axisValue = axis;
    if (axisValue.LengthSquared() <= Internal::transformEpsilon)
        return;

    axisValue.Normalize();
    Add_LocalRotation(Quat::CreateFromAxisAngle(axisValue, _radianPerSec * timeDelta));
}

void Transform::LookAt(const Vec3& at)
{
    const Vec3 position = Get_Position();
    const Vec3 preservedScale = Get_Scale();

    Vec3 direction = at - position;
    if (direction.LengthSquared() <= Internal::transformEpsilon)
        return;

    direction.Normalize();

    _localMatrix = Matrix::CreateWorld(position, direction, Vec3::Up);
    Set_Scale(preservedScale);
    Sanitize_LocalBasis();
}

void Transform::Ensure_WorldMatrixUpToDate()
{
    const Shared<Transform> parent = _parent.lock();
    if (parent && !_hasParent)
        _hasParent = true;
    else if (!parent && _hasParent)
        _hasParent = false;

    uint64 parentRevision = 0;
    if (parent)
    {
        parent->Ensure_WorldMatrixUpToDate();
        parentRevision = parent->_worldRevision;
    }

    const bool parentWorldChanged = parentRevision != _observedParentWorldRevision;
    if (!Has_Dirty(TRANSFORM_DIRTY_FLAG::WORLD) && !parentWorldChanged)
        return;

    if (parent)
        _worldMatrix = _localMatrix * parent->_worldMatrix;
    else
        _worldMatrix = _localMatrix;

    _observedParentWorldRevision = parentRevision;
    ++_worldRevision;
    Clear_Dirty(TRANSFORM_DIRTY_FLAG::WORLD);
}

void Transform::Mark_LocalChanged(LOCAL_CHANGE_KIND changeKind)
{
    Set_Dirty(TRANSFORM_DIRTY_FLAG::WORLD);

    if (LOCAL_CHANGE_KIND::ROTATION_CHANGED == changeKind)
        Set_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
}

bool Transform::Has_Dirty(TRANSFORM_DIRTY_FLAG dirty) const
{
    return (_dirtyMask & static_cast<uint8>(dirty)) != 0;
}

void Transform::Set_Dirty(TRANSFORM_DIRTY_FLAG dirty)
{
    _dirtyMask |= static_cast<uint8>(dirty);
}

void Transform::Clear_Dirty(TRANSFORM_DIRTY_FLAG dirty)
{
    _dirtyMask &= static_cast<uint8>(~static_cast<uint8>(dirty));
}

float Transform::Clamp_EditableScale(float scale)
{
    if (abs(scale) <= Internal::transformEpsilon)
        return Internal::minEditableScale;

    if (abs(scale) < Internal::minEditableScale)
        return Internal::Resolve_ScaleSign(scale) * Internal::minEditableScale;

    return scale;
}

Vec3 Transform::Clamp_EditableScale(const Vec3& scale)
{
    return Vec3(
        Clamp_EditableScale(scale.x),
        Clamp_EditableScale(scale.y),
        Clamp_EditableScale(scale.z)
    );
}

float Transform::Get_SignedScale(State state) const
{
    switch (state)
    {
    case State::Right:
        return _localSignedScale.x;
    case State::Up:
        return _localSignedScale.y;
    case State::Look:
        return _localSignedScale.z;
    default:
        return 1.f;
    }
}

void Transform::Set_SignedScale(State state, float signedScale)
{
    const float clampedScale = Clamp_EditableScale(signedScale);

    switch (state)
    {
    case State::Right:
        _localSignedScale.x = clampedScale;
        break;
    case State::Up:
        _localSignedScale.y = clampedScale;
        break;
    case State::Look:
        _localSignedScale.z = clampedScale;
        break;
    default:
        break;
    }
}

void Transform::Sync_SignedScaleFromLocalMatrix(Vec3 fallbackScale)
{
    fallbackScale = Clamp_EditableScale(fallbackScale);

    const Vec3 right = Get_LocalState(State::Right);
    const Vec3 up = Get_LocalState(State::Up);
    const Vec3 look = Get_LocalState(State::Look);

    Set_SignedScale(State::Right, right.Length() * Internal::Resolve_ScaleSign(fallbackScale.x));
    Set_SignedScale(State::Up, up.Length() * Internal::Resolve_ScaleSign(fallbackScale.y));
    Set_SignedScale(State::Look, look.Length() * Internal::Resolve_ScaleSign(fallbackScale.z));
}

Vec3 Transform::Get_LocalState(State state) const
{
    switch (state)
    {
    case State::Right:
        return Vec3(_localMatrix._11, _localMatrix._12, _localMatrix._13);
    case State::Up:
        return Vec3(_localMatrix._21, _localMatrix._22, _localMatrix._23);
    case State::Look:
        return Vec3(_localMatrix._31, _localMatrix._32, _localMatrix._33);
    case State::Position:
        return Vec3(_localMatrix._41, _localMatrix._42, _localMatrix._43);
    default:
        return Vec3::Zero;
    }
}

void Transform::Set_LocalState(State state, const Vec3& value)
{
    switch (state)
    {
    case State::Right:
        _localMatrix._11 = value.x;
        _localMatrix._12 = value.y;
        _localMatrix._13 = value.z;
        break;
    case State::Up:
        _localMatrix._21 = value.x;
        _localMatrix._22 = value.y;
        _localMatrix._23 = value.z;
        break;
    case State::Look:
        _localMatrix._31 = value.x;
        _localMatrix._32 = value.y;
        _localMatrix._33 = value.z;
        break;
    case State::Position:
        _localMatrix._41 = value.x;
        _localMatrix._42 = value.y;
        _localMatrix._43 = value.z;
        break;
    default:
        break;
    }
}

Vec3 Transform::Resolve_NormalizedAxis(State state) const
{
    Vec3 axis = Get_LocalState(state);
    const float signedScale = Get_SignedScale(state);

    if (abs(signedScale) > Internal::transformEpsilon && axis.LengthSquared() > Internal::transformEpsilon)
    {
        axis /= signedScale;

        if (axis.LengthSquared() > Internal::transformEpsilon)
        {
            axis.Normalize();
            return axis;
        }
    }

    Quat rotation = _localRotationCache;
    if (rotation.LengthSquared() <= Internal::transformEpsilon)
        rotation = Quat::Identity;
    else
        rotation.Normalize();

    const Matrix rotationMatrix = Matrix::CreateFromQuaternion(rotation);

    switch (state)
    {
    case State::Right:
        axis = Vec3(rotationMatrix._11, rotationMatrix._12, rotationMatrix._13);
        break;
    case State::Up:
        axis = Vec3(rotationMatrix._21, rotationMatrix._22, rotationMatrix._23);
        break;
    case State::Look:
        axis = Vec3(rotationMatrix._31, rotationMatrix._32, rotationMatrix._33);
        break;
    default:
        return Vec3::Zero;
    }

    if (axis.LengthSquared() <= Internal::transformEpsilon)
    {
        switch (state)
        {
        case State::Right:
            return Vec3::Right;
        case State::Up:
            return Vec3::Up;
        case State::Look:
            return Vec3::Look;
        default:
            return Vec3::Zero;
        }
    }

    axis.Normalize();
    return axis;
}

void Transform::Sanitize_LocalBasis()
{
    const Vec3 scale = Clamp_EditableScale(Get_Scale());
    _localSignedScale = scale;

    const Vec3 right = Resolve_NormalizedAxis(State::Right);
    Vec3 up = Resolve_NormalizedAxis(State::Up);
    Vec3 look = Resolve_NormalizedAxis(State::Look);

    up -= right * up.Dot(right);
    if (up.LengthSquared() <= Internal::transformEpsilon)
    {
        Vec3 fallbackUp = Vec3::Up;
        const float rightDotUp = right.Dot(fallbackUp);
        if (rightDotUp > 0.999f || rightDotUp < -0.999f)
            fallbackUp = Vec3::Look;

        up = fallbackUp - right * fallbackUp.Dot(right);
    }

    if (up.LengthSquared() <= Internal::transformEpsilon)
        up = Vec3::Up;
    else
        up.Normalize();

    look = right.Cross(up);
    if (look.LengthSquared() <= Internal::transformEpsilon)
    {
        Vec3 fallbackLook = Vec3::Look;
        const float rightDotLook = right.Dot(fallbackLook);
        if (rightDotLook > 0.999f || rightDotLook < -0.999f)
            fallbackLook = Vec3::Up;

        look = right.Cross(fallbackLook);
    }

    if (look.LengthSquared() <= Internal::transformEpsilon)
        look = Vec3::Look;
    else
        look.Normalize();

    up = look.Cross(right);
    if (up.LengthSquared() <= Internal::transformEpsilon)
        up = Vec3::Up;
    else
        up.Normalize();

    Set_LocalState(State::Right, right * scale.x);
    Set_LocalState(State::Up, up * scale.y);
    Set_LocalState(State::Look, look * scale.z);

    Mark_LocalChanged(LOCAL_CHANGE_KIND::ROTATION_CHANGED);
}

void Transform::Rebuild_LocalBasis_FromRotationCache()
{
    const Vec3 preservedPosition = Get_Position();
    const Vec3 preservedScale = Clamp_EditableScale(Get_Scale());
    _localSignedScale = preservedScale;

    if (_localRotationCache.LengthSquared() <= Internal::transformEpsilon)
        _localRotationCache = Quat::Identity;
    else
        _localRotationCache.Normalize();

    const Matrix rotationMatrix = Matrix::CreateFromQuaternion(_localRotationCache);

    Vec3 right(rotationMatrix._11, rotationMatrix._12, rotationMatrix._13);
    Vec3 up(rotationMatrix._21, rotationMatrix._22, rotationMatrix._23);
    Vec3 look(rotationMatrix._31, rotationMatrix._32, rotationMatrix._33);

    right.Normalize();
    up.Normalize();
    look.Normalize();

    Set_LocalState(State::Right, right * preservedScale.x);
    Set_LocalState(State::Up, up * preservedScale.y);
    Set_LocalState(State::Look, look * preservedScale.z);
    Set_LocalState(State::Position, preservedPosition);

    Mark_LocalChanged(LOCAL_CHANGE_KIND::ROTATION_CHANGED);
}

void Transform::Sync_RotationCache_FromLocalMatrix()
{
    if (!Has_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION))
        return;

    const Vec3 right = Resolve_NormalizedAxis(State::Right);
    Vec3 up = Resolve_NormalizedAxis(State::Up);
    Vec3 look = Resolve_NormalizedAxis(State::Look);

    up -= right * up.Dot(right);
    if (up.LengthSquared() <= Internal::transformEpsilon)
    {
        Vec3 fallbackUp = Vec3::Up;
        const float rightDotUp = right.Dot(fallbackUp);
        if (rightDotUp > 0.999f || rightDotUp < -0.999f)
            fallbackUp = Vec3::Look;

        up = fallbackUp - right * fallbackUp.Dot(right);
    }

    if (up.LengthSquared() <= Internal::transformEpsilon)
        up = Vec3::Up;
    else
        up.Normalize();

    look = right.Cross(up);
    if (look.LengthSquared() <= Internal::transformEpsilon)
        look = Vec3::Look;
    else
        look.Normalize();

    up = look.Cross(right);
    if (up.LengthSquared() <= Internal::transformEpsilon)
        up = Vec3::Up;
    else
        up.Normalize();

    Matrix rotationMatrix = Matrix::Identity;
    rotationMatrix._11 = right.x;
    rotationMatrix._12 = right.y;
    rotationMatrix._13 = right.z;
    rotationMatrix._21 = up.x;
    rotationMatrix._22 = up.y;
    rotationMatrix._23 = up.z;
    rotationMatrix._31 = look.x;
    rotationMatrix._32 = look.y;
    rotationMatrix._33 = look.z;

    Quat rotation = Quat::CreateFromRotationMatrix(rotationMatrix);
    if (rotation.LengthSquared() <= Internal::transformEpsilon)
        rotation = Quat::Identity;
    else
        rotation.Normalize();

    _localRotationCache = rotation;
    _localEulerDegreeCache = Extract_EulerDegreeFromRotationMatrix(rotationMatrix);
    Clear_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
}

Vec3 Transform::Extract_EulerDegreeFromRotationMatrix(const Matrix& rotationMatrix)
{
    Quat rotation = Quat::CreateFromRotationMatrix(rotationMatrix);
    if (rotation.LengthSquared() <= Internal::transformEpsilon)
        rotation = Quat::Identity;
    else
        rotation.Normalize();

    const Vec3 eulerRadian = rotation.ToEuler();
    return Vec3(
        XMConvertToDegrees(eulerRadian.x),
        XMConvertToDegrees(eulerRadian.y),
        XMConvertToDegrees(eulerRadian.z)
    );
}

Shared<Transform> Transform::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Transform>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Transform");
        return nullptr;
    }

    return instance;
}

Shared<Component> Transform::Clone(void* arg)
{
    auto instance = make_shared<Transform>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Transform");
        return nullptr;
    }

    return instance;
}

void Transform::Free()
{
    __super::Free();
}
