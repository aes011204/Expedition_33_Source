#include "TransformCom.h"
#include "ShaderCom.h"

namespace
{
constexpr float transformEpsilon = 1e-6f;
constexpr float minEditableScale = 1e-4f;
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

TransformCom::TransformCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context }
{
}

TransformCom::TransformCom(const TransformCom& prototype)
    : Component{ prototype }
    , _localMatrix{ prototype._localMatrix }
    , _localSignedScale{ prototype._localSignedScale }
    , _localRotationCache{ prototype._localRotationCache }
    , _localEulerDegreeCache{ prototype._localEulerDegreeCache }
    , _worldMatrix{ prototype._worldMatrix }
{
    Set_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
    Set_Dirty(TRANSFORM_DIRTY_FLAG::WORLD);
}

HRESULT TransformCom::Initialize_Prototype()
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

HRESULT TransformCom::Initialize(void* arg)
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

void TransformCom::Update_WorldMatrix()
{
    Ensure_WorldMatrixUpToDate();
}

HRESULT TransformCom::Bind_ShaderResource(ShaderCom* shader, const char* resourceName)
{
    Ensure_WorldMatrixUpToDate();
    return shader->Bind_Matrix(resourceName, reinterpret_cast<const XMFLOAT4X4*>(&Get_WorldMatrix()));
}

json TransformCom::To_Json() const
{
    json root = __super::To_Json();

    TransformCom* self = const_cast<TransformCom*>(this);

    const Vec3 worldPosition = self->Get_WorldPosition();
    const Vec3 worldRotation = self->Get_WorldRotationEuler();
    const Vec3 localScale = self->Get_Scale();

    root["position"] = { worldPosition.x, worldPosition.y, worldPosition.z };
    root["rotation"] = { worldRotation.x, worldRotation.y, worldRotation.z };
    root["scale"] = { localScale.x, localScale.y, localScale.z };

    return root;
}

void TransformCom::From_Json(const json& data)
{
    __super::From_Json(data);

    const bool hasTransformParent = !_parent.expired();

    if (data.contains("scale"))
    {
        const json& arr = data["scale"];
        if (arr.is_array() && arr.size() >= 3)
        {
            Set_Scale(
                Vec3(
                    arr[0].get<float>(),
                    arr[1].get<float>(),
                    arr[2].get<float>()
                )
            );
        }
    }

    if (data.contains("rotation"))
    {
        const json& arr = data["rotation"];
        if (arr.is_array() && arr.size() >= 3)
        {
            const Vec3 serializedRotation(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()
            );

            // Parent가 없으면 serialized Euler를 복원해 Inspector 표시값을 보존한다.
            if (hasTransformParent)
                Set_WorldRotationEuler(serializedRotation);
            else
                Set_RotationEuler(serializedRotation);
        }
    }

    if (data.contains("position"))
    {
        const json& arr = data["position"];
        if (arr.is_array() && arr.size() >= 3)
        {
            const Vec3 serializedPosition(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()
            );

            if (hasTransformParent)
                Set_WorldPosition(serializedPosition);
            else
                Set_Position(serializedPosition);
        }
    }
}

const Matrix& TransformCom::Get_WorldMatrix()
{
    Ensure_WorldMatrixUpToDate();
    return _worldMatrix;
}

Matrix TransformCom::Get_LocalMatrix() const
{
    return _localMatrix;
}

Vec3 TransformCom::Get_State(State state)
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

Vec3 TransformCom::Get_Position() const
{
    return Get_LocalState(State::Position);
}

void TransformCom::Set_Position(const Vec3& position)
{
    Set_LocalState(State::Position, position);
    Mark_LocalChanged(LOCAL_CHANGE_KIND::ROTATION_UNCHANGED);
}

void TransformCom::Add_Position(const Vec3& deltaPosition)
{
    Set_Position(Get_Position() + deltaPosition);
}

Vec3 TransformCom::Get_WorldPosition()
{
    return Get_State(State::Position);
}

void TransformCom::Set_WorldPosition(const Vec3& position)
{
    const Shared<TransformCom> parent = _parent.lock();
    if (!parent)
    {
        Set_Position(position);
        return;
    }

    parent->Ensure_WorldMatrixUpToDate();

    const Matrix parentWorld = parent->_worldMatrix;
    const float determinant = XMVectorGetX(XMMatrixDeterminant(parentWorld));
    if (determinant > -transformEpsilon && determinant < transformEpsilon)
        return;

    const Matrix inverseParentWorld = parentWorld.Invert();
    Set_Position(Vec3::Transform(position, inverseParentWorld));
}

void TransformCom::Add_WorldPosition(const Vec3& deltaPosition)
{
    Set_WorldPosition(Get_WorldPosition() + deltaPosition);
}

Vec3 TransformCom::Get_Scale() const
{
    return _localSignedScale;
}

void TransformCom::Set_Scale(const Vec3& scale)
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

void TransformCom::Set_ScaleX(float scaleX)
{
    Set_Scale(Vec3(scaleX, Get_Scale().y, Get_Scale().z));
}

void TransformCom::Set_ScaleY(float scaleY)
{
    Set_Scale(Vec3(Get_Scale().x, scaleY, Get_Scale().z));
}

void TransformCom::Set_ScaleZ(float scaleZ)
{
    Set_Scale(Vec3(Get_Scale().x, Get_Scale().y, scaleZ));
}

Vec3 TransformCom::Get_RotationEuler()
{
    Sync_RotationCache_FromLocalMatrix();
    return _localEulerDegreeCache;
}

void TransformCom::Set_RotationEuler(const Vec3& eulerDegree)
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

void TransformCom::Add_LocalRotationEuler(const Vec3& deltaEulerDegree)
{
    const Vec3 deltaEulerRadian(
        XMConvertToRadians(deltaEulerDegree.x),
        XMConvertToRadians(deltaEulerDegree.y),
        XMConvertToRadians(deltaEulerDegree.z)
    );

    Add_LocalRotationQuaternion(
        Quat::CreateFromYawPitchRoll(
            deltaEulerRadian.y,
            deltaEulerRadian.x,
            deltaEulerRadian.z
        )
    );
}

Vec3 TransformCom::Get_WorldRotationEuler()
{
    Matrix worldMatrix = Get_WorldMatrix();

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!worldMatrix.Decompose(scale, rotation, translation))
        return Vec3::Zero;

    if (rotation.LengthSquared() <= transformEpsilon)
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

void TransformCom::Set_WorldRotationEuler(const Vec3& eulerDegree)
{
    const Vec3 eulerRadian(
        XMConvertToRadians(eulerDegree.x),
        XMConvertToRadians(eulerDegree.y),
        XMConvertToRadians(eulerDegree.z)
    );

    const Quat desiredWorldRotation =
        Quat::CreateFromYawPitchRoll(eulerRadian.y, eulerRadian.x, eulerRadian.z);

    Set_WorldRotationQuaternion(desiredWorldRotation);
}

void TransformCom::Add_WorldRotationEuler(const Vec3& deltaEulerDegree)
{
    const Vec3 deltaEulerRadian(
        XMConvertToRadians(deltaEulerDegree.x),
        XMConvertToRadians(deltaEulerDegree.y),
        XMConvertToRadians(deltaEulerDegree.z)
    );

    Add_WorldRotationQuaternion(
        Quat::CreateFromYawPitchRoll(
            deltaEulerRadian.y,
            deltaEulerRadian.x,
            deltaEulerRadian.z
        )
    );
}

const Quat& TransformCom::Get_RotationQuaternion()
{
    Sync_RotationCache_FromLocalMatrix();
    return _localRotationCache;
}

void TransformCom::Set_RotationQuaternion(const Quat& rotation)
{
    if (rotation.LengthSquared() <= transformEpsilon)
        _localRotationCache = Quat::Identity;
    else
    {
        _localRotationCache = rotation;
        _localRotationCache.Normalize();
    }

    // gizmo는 quaternion 경로를 타므로, 여기서 Euler 표시 캐시까지 같이 갱신해야
    // 인스펙터 숫자가 실제 회전값과 같은 프레임에 따라온다.
    const Matrix rotationMatrix = Matrix::CreateFromQuaternion(_localRotationCache);
    _localEulerDegreeCache = Extract_EulerDegreeFromRotationMatrix(rotationMatrix);

    Rebuild_LocalBasis_FromRotationCache();
    Clear_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
}

void TransformCom::Add_LocalRotationQuaternion(const Quat& deltaRotation)
{
    Quat rotation = Get_RotationQuaternion();
    rotation = rotation * deltaRotation;

    if (rotation.LengthSquared() <= transformEpsilon)
        return;

    rotation.Normalize();
    Set_RotationQuaternion(rotation);
}

Quat TransformCom::Get_WorldRotationQuaternion()
{
    Matrix worldMatrix = Get_WorldMatrix();

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!worldMatrix.Decompose(scale, rotation, translation))
        return Quat::Identity;

    if (rotation.LengthSquared() <= transformEpsilon)
        return Quat::Identity;

    rotation.Normalize();
    return rotation;
}

void TransformCom::Set_WorldRotationQuaternion(const Quat& rotation)
{
    Quat desiredWorldRotation = rotation;

    if (desiredWorldRotation.LengthSquared() <= transformEpsilon)
        desiredWorldRotation = Quat::Identity;
    else
        desiredWorldRotation.Normalize();

    const Shared<TransformCom> parent = _parent.lock();
    if (!parent)
    {
        Set_RotationQuaternion(desiredWorldRotation);
        return;
    }

    Matrix parentWorld = parent->Get_WorldMatrix();

    Vec3 parentScale = Vec3::One;
    Quat parentWorldRotation = Quat::Identity;
    Vec3 parentTranslation = Vec3::Zero;

    if (!parentWorld.Decompose(parentScale, parentWorldRotation, parentTranslation))
    {
        Set_RotationQuaternion(desiredWorldRotation);
        return;
    }

    if (parentWorldRotation.LengthSquared() <= transformEpsilon)
        parentWorldRotation = Quat::Identity;
    else
        parentWorldRotation.Normalize();

    const Matrix desiredWorldRotationMatrix = Matrix::CreateFromQuaternion(desiredWorldRotation);
    const Matrix parentWorldRotationMatrix = Matrix::CreateFromQuaternion(parentWorldRotation);
    const Matrix localRotationMatrix = desiredWorldRotationMatrix * parentWorldRotationMatrix.Invert();

    Quat localRotation = Quat::CreateFromRotationMatrix(localRotationMatrix);
    if (localRotation.LengthSquared() <= transformEpsilon)
        localRotation = Quat::Identity;
    else
        localRotation.Normalize();

    Set_RotationQuaternion(localRotation);
}

void TransformCom::Add_WorldRotationQuaternion(const Quat& deltaRotation)
{
    Quat worldRotation = Get_WorldRotationQuaternion();
    worldRotation = worldRotation * deltaRotation;

    if (worldRotation.LengthSquared() <= transformEpsilon)
        return;

    worldRotation.Normalize();
    Set_WorldRotationQuaternion(worldRotation);
}

float TransformCom::Get_SpeedPerSec() const
{
    return _speedPerSec;
}

void TransformCom::Set_SpeedPerSec(float speedPerSec)
{
    _speedPerSec = speedPerSec < minEditableSpeedPerSec ? minEditableSpeedPerSec : speedPerSec;
}

float TransformCom::Get_DegreePerSec() const
{
    return XMConvertToDegrees(_radianPerSec);
}

void TransformCom::Set_DegreePerSec(float degreePerSec)
{
    const float clampedDegreePerSec = degreePerSec < minEditableDegreePerSec ? minEditableDegreePerSec : degreePerSec;
    _radianPerSec = XMConvertToRadians(clampedDegreePerSec);
}

Vec3 TransformCom::Get_WorldForward()
{
    Vec3 forward = Get_State(State::Look);
    if (forward.LengthSquared() > transformEpsilon)
        forward.Normalize();

    return forward;
}

Vec3 TransformCom::Get_WorldRight()
{
    Vec3 right = Get_State(State::Right);
    if (right.LengthSquared() > transformEpsilon)
        right.Normalize();

    return right;
}

Vec3 TransformCom::Get_WorldUp()
{
    Vec3 up = Get_State(State::Up);
    if (up.LengthSquared() > transformEpsilon)
        up.Normalize();

    return up;
}

Vec3 TransformCom::Get_LocalForward()
{
    Sync_RotationCache_FromLocalMatrix();
    return Vec3::Transform(Vec3::Look, _localRotationCache);
}

Vec3 TransformCom::Get_LocalRight()
{
    Sync_RotationCache_FromLocalMatrix();
    return Vec3::Transform(Vec3::Right, _localRotationCache);
}

Vec3 TransformCom::Get_LocalUp()
{
    Sync_RotationCache_FromLocalMatrix();
    return Vec3::Transform(Vec3::Up, _localRotationCache);
}

void TransformCom::Set_Parent(const Shared<TransformCom>& parent)
{
    Ensure_WorldMatrixUpToDate();
    const Vec3 previousSignedScale = Get_Scale();
    const Matrix currentWorld = _worldMatrix;

    // self-parent/cyclic-parent는 world/local 변환을 정의할 수 없다.
    if (parent.get() == this)
    {
        LOG_WARN("TransformCom::Set_Parent rejected self-parent.");
        return;
    }

    for (Shared<TransformCom> cursor = parent; cursor; cursor = cursor->_parent.lock())
    {
        if (cursor.get() == this)
        {
            LOG_WARN("TransformCom::Set_Parent rejected cyclic parent chain.");
            return;
        }
    }

    if (parent)
    {
        parent->Ensure_WorldMatrixUpToDate();

        const Matrix parentWorld = parent->_worldMatrix;
        const float determinant = XMVectorGetX(XMMatrixDeterminant(parentWorld));
        if (abs(determinant) <= transformEpsilon)
        {
            LOG_WARN("TransformCom::Set_Parent rejected non-invertible parent world matrix.");
            return;
        }

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

void TransformCom::Scaling(float scaleX, float scaleY, float scaleZ)
{
    const Vec3 currentScale = Get_Scale();
    Set_Scale(Vec3(currentScale.x * scaleX, currentScale.y * scaleY, currentScale.z * scaleZ));
}

void TransformCom::ScaleX(float scaleX)
{
    Set_ScaleX(Get_Scale().x * scaleX);
}

void TransformCom::ScaleY(float scaleY)
{
    Set_ScaleY(Get_Scale().y * scaleY);
}

void TransformCom::ScaleZ(float scaleZ)
{
    Set_ScaleZ(Get_Scale().z * scaleZ);
}

void TransformCom::Go_Straight(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() + Get_WorldForward() * _speedPerSec * timeDelta);
}

void TransformCom::Go_Backward(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() - Get_WorldForward() * _speedPerSec * timeDelta);
}

void TransformCom::Go_Right(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() + Get_WorldRight() * _speedPerSec * timeDelta);
}

void TransformCom::Go_Left(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() - Get_WorldRight() * _speedPerSec * timeDelta);
}

void TransformCom::Go_Up(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() + Vec3::Up * _speedPerSec * timeDelta);
}

void TransformCom::Go_Down(float timeDelta)
{
    Set_WorldPosition(Get_WorldPosition() - Vec3::Up * _speedPerSec * timeDelta);
}

void TransformCom::Rotate(const Vec3& axis, float degree)
{
    Vec3 axisValue = axis;
    if (axisValue.LengthSquared() <= transformEpsilon)
        return;

    axisValue.Normalize();
    Set_RotationQuaternion(Quat::CreateFromAxisAngle(axisValue, XMConvertToRadians(degree)));
}

void TransformCom::LookAt(const Vec3& at)
{
    const Vec3 worldPosition = Get_WorldPosition();
    const Vec3 preservedScale = Get_Scale();
    const Shared<TransformCom> parent = _parent.lock();

    // SimpleMath::CreateWorld는 카메라 forward 관례를 써서 target 방향을 반대로 넣는다.
    Vec3 direction = worldPosition - at;
    if (direction.LengthSquared() <= transformEpsilon)
        return;

    direction.Normalize();

    // forward와 up이 거의 평행하면 basis가 무너지므로 보조 up축을 바꾼다.
    Vec3 upAxis = Vec3::Up;
    const float upDot = direction.Dot(upAxis);
    constexpr float parallelDotThreshold = 1.f - 1e-3f;
    if (abs(upDot) > parallelDotThreshold)
        upAxis = Vec3::Look;

    const Matrix desiredWorld = Matrix::CreateWorld(worldPosition, direction, upAxis);

    if (parent)
    {
        parent->Ensure_WorldMatrixUpToDate();

        const Matrix parentWorld = parent->_worldMatrix;
        const float determinant = XMVectorGetX(XMMatrixDeterminant(parentWorld));
        if (abs(determinant) <= transformEpsilon)
            return;

        _localMatrix = desiredWorld * parentWorld.Invert();
    }
    else
        _localMatrix = desiredWorld;

    Set_Scale(preservedScale);
    Sanitize_LocalBasis();
}

void TransformCom::Ensure_WorldMatrixUpToDate()
{
    const Shared<TransformCom> parent = _parent.lock();
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

void TransformCom::Mark_LocalChanged(LOCAL_CHANGE_KIND changeKind)
{
    Set_Dirty(TRANSFORM_DIRTY_FLAG::WORLD);

    if (LOCAL_CHANGE_KIND::ROTATION_CHANGED == changeKind)
        Set_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
}

bool TransformCom::Has_Dirty(TRANSFORM_DIRTY_FLAG dirty) const
{
    return (_dirtyMask & static_cast<uint8>(dirty)) != 0;
}

void TransformCom::Set_Dirty(TRANSFORM_DIRTY_FLAG dirty)
{
    _dirtyMask |= static_cast<uint8>(dirty);
}

void TransformCom::Clear_Dirty(TRANSFORM_DIRTY_FLAG dirty)
{
    _dirtyMask &= static_cast<uint8>(~static_cast<uint8>(dirty));
}

float TransformCom::Clamp_EditableScale(float scale)
{
    if (abs(scale) <= transformEpsilon)
        return minEditableScale;

    if (abs(scale) < minEditableScale)
        return Resolve_ScaleSign(scale) * minEditableScale;

    return scale;
}

Vec3 TransformCom::Clamp_EditableScale(const Vec3& scale)
{
    return Vec3(
        Clamp_EditableScale(scale.x),
        Clamp_EditableScale(scale.y),
        Clamp_EditableScale(scale.z)
    );
}

float TransformCom::Get_SignedScale(State state) const
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

void TransformCom::Set_SignedScale(State state, float signedScale)
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

void TransformCom::Sync_SignedScaleFromLocalMatrix(Vec3 fallbackScale)
{
    fallbackScale = Clamp_EditableScale(fallbackScale);

    const Vec3 right = Get_LocalState(State::Right);
    const Vec3 up = Get_LocalState(State::Up);
    const Vec3 look = Get_LocalState(State::Look);

    Set_SignedScale(State::Right, right.Length() * Resolve_ScaleSign(fallbackScale.x));
    Set_SignedScale(State::Up, up.Length() * Resolve_ScaleSign(fallbackScale.y));
    Set_SignedScale(State::Look, look.Length() * Resolve_ScaleSign(fallbackScale.z));
}

Vec3 TransformCom::Get_LocalState(State state) const
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

void TransformCom::Set_LocalState(State state, const Vec3& value)
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

Vec3 TransformCom::Resolve_NormalizedAxis(State state) const
{
    Vec3 axis = Get_LocalState(state);
    const float signedScale = Get_SignedScale(state);

    if (abs(signedScale) > transformEpsilon && axis.LengthSquared() > transformEpsilon)
    {
        axis /= signedScale;

        if (axis.LengthSquared() > transformEpsilon)
        {
            axis.Normalize();
            return axis;
        }
    }

    Quat rotation = _localRotationCache;
    if (rotation.LengthSquared() <= transformEpsilon)
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

    if (axis.LengthSquared() <= transformEpsilon)
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

void TransformCom::Sanitize_LocalBasis()
{
    const Vec3 scale = Clamp_EditableScale(Get_Scale());
    _localSignedScale = scale;

    const Vec3 right = Resolve_NormalizedAxis(State::Right);
    Vec3 up = Resolve_NormalizedAxis(State::Up);
    Vec3 look = Resolve_NormalizedAxis(State::Look);

    up -= right * up.Dot(right);
    if (up.LengthSquared() <= transformEpsilon)
    {
        Vec3 fallbackUp = Vec3::Up;
        const float rightDotUp = right.Dot(fallbackUp);
        if (rightDotUp > 0.999f || rightDotUp < -0.999f)
            fallbackUp = Vec3::Look;

        up = fallbackUp - right * fallbackUp.Dot(right);
    }

    if (up.LengthSquared() <= transformEpsilon)
        up = Vec3::Up;
    else
        up.Normalize();

    look = right.Cross(up);
    if (look.LengthSquared() <= transformEpsilon)
    {
        Vec3 fallbackLook = Vec3::Look;
        const float rightDotLook = right.Dot(fallbackLook);
        if (rightDotLook > 0.999f || rightDotLook < -0.999f)
            fallbackLook = Vec3::Up;

        look = right.Cross(fallbackLook);
    }

    if (look.LengthSquared() <= transformEpsilon)
        look = Vec3::Look;
    else
        look.Normalize();

    up = look.Cross(right);
    if (up.LengthSquared() <= transformEpsilon)
        up = Vec3::Up;
    else
        up.Normalize();

    Set_LocalState(State::Right, right * scale.x);
    Set_LocalState(State::Up, up * scale.y);
    Set_LocalState(State::Look, look * scale.z);

    Mark_LocalChanged(LOCAL_CHANGE_KIND::ROTATION_CHANGED);
}

void TransformCom::Rebuild_LocalBasis_FromRotationCache()
{
    const Vec3 preservedPosition = Get_Position();
    const Vec3 preservedScale = Clamp_EditableScale(Get_Scale());
    _localSignedScale = preservedScale;

    if (_localRotationCache.LengthSquared() <= transformEpsilon)
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

void TransformCom::Sync_RotationCache_FromLocalMatrix()
{
    if (!Has_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION))
        return;

    const Vec3 right = Resolve_NormalizedAxis(State::Right);
    Vec3 up = Resolve_NormalizedAxis(State::Up);
    Vec3 look = Resolve_NormalizedAxis(State::Look);

    up -= right * up.Dot(right);
    if (up.LengthSquared() <= transformEpsilon)
    {
        Vec3 fallbackUp = Vec3::Up;
        const float rightDotUp = right.Dot(fallbackUp);
        if (rightDotUp > 0.999f || rightDotUp < -0.999f)
            fallbackUp = Vec3::Look;

        up = fallbackUp - right * fallbackUp.Dot(right);
    }

    if (up.LengthSquared() <= transformEpsilon)
        up = Vec3::Up;
    else
        up.Normalize();

    look = right.Cross(up);
    if (look.LengthSquared() <= transformEpsilon)
        look = Vec3::Look;
    else
        look.Normalize();

    up = look.Cross(right);
    if (up.LengthSquared() <= transformEpsilon)
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
    if (rotation.LengthSquared() <= transformEpsilon)
        rotation = Quat::Identity;
    else
        rotation.Normalize();

    _localRotationCache = rotation;
    _localEulerDegreeCache = Extract_EulerDegreeFromRotationMatrix(rotationMatrix);
    Clear_Dirty(TRANSFORM_DIRTY_FLAG::ROTATION);
}

Vec3 TransformCom::Extract_EulerDegreeFromRotationMatrix(const Matrix& rotationMatrix)
{
    Quat rotation = Quat::CreateFromRotationMatrix(rotationMatrix);
    if (rotation.LengthSquared() <= transformEpsilon)
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

Shared<TransformCom> TransformCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<TransformCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : TransformCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> TransformCom::Clone(void* arg)
{
    auto instance = make_shared<TransformCom>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : TransformCom");
        return nullptr;
    }

    return instance;
}

void TransformCom::Free()
{
    __super::Free();
}
