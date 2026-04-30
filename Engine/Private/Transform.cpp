#include "Transform.h"
#include "Shader.h"

Transform::Transform(ComPtr<Device> device, ComPtr<Context> context)
	: Component{ device, context }
{
}

Transform::Transform(const Transform& Prototype)
	: Component{ Prototype }
	, _localPosition{ Prototype._localPosition }
	, _localRotation{ Prototype._localRotation }
	, _localScale{ Prototype._localScale }
	, _localEulerAngles{ Prototype._localEulerAngles }
	, _speedPerSec{ Prototype._speedPerSec }
	, _radianPerSec{ Prototype._radianPerSec }
{
	_isDirty = true;
}

XMVECTOR Transform::Get_State(State eState) const
{
	const Matrix& WorldMatrix = Get_WorldMatrix();

	switch (eState)
	{
	case State::RIGHT:
		return ToVector(Vec3(WorldMatrix._11, WorldMatrix._12, WorldMatrix._13));
	case State::UP:
		return ToVector(Vec3(WorldMatrix._21, WorldMatrix._22, WorldMatrix._23));
	case State::LOOK:
		return ToVector(Vec3(WorldMatrix._31, WorldMatrix._32, WorldMatrix._33));
	case State::POSITION:
		return ToVector(Vec3(WorldMatrix._41, WorldMatrix._42, WorldMatrix._43), 1.f);
	default:
		return XMVectorZero();
	}
}

void Transform::Set_State(State eState, FXMVECTOR vState)
{
	Matrix WorldMatrix = Get_WorldMatrix();
	const Vec3 vNewState = ToVec3(vState);

	switch (eState)
	{
	case State::RIGHT:
		WorldMatrix._11 = vNewState.x;
		WorldMatrix._12 = vNewState.y;
		WorldMatrix._13 = vNewState.z;
		break;
	case State::UP:
		WorldMatrix._21 = vNewState.x;
		WorldMatrix._22 = vNewState.y;
		WorldMatrix._23 = vNewState.z;
		break;
	case State::LOOK:
		WorldMatrix._31 = vNewState.x;
		WorldMatrix._32 = vNewState.y;
		WorldMatrix._33 = vNewState.z;
		break;
	case State::POSITION:
		WorldMatrix._41 = vNewState.x;
		WorldMatrix._42 = vNewState.y;
		WorldMatrix._43 = vNewState.z;
		break;
	default:
		return;
	}

	Decompose_WorldMatrix(WorldMatrix);
}

XMFLOAT3 Transform::Get_Scaled() const
{
	const Vec3 vScale = Get_LocalScale();
	return XMFLOAT3(vScale.x, vScale.y, vScale.z);
}

const XMFLOAT4X4* Transform::Get_WorldMatrixPtr() const
{
	return reinterpret_cast<const XMFLOAT4X4*>(&Get_WorldMatrix());
}

const Matrix& Transform::Get_WorldMatrix() const
{
	if (_isDirty)
		Update_WorldMatrix();

	return _worldMatrix;
}

Matrix Transform::Get_LocalMatrix() const
{
	return Matrix::CreateScale(_localScale)
		* Matrix::CreateFromQuaternion(_localRotation)
		* Matrix::CreateTranslation(_localPosition);
}

HRESULT Transform::Initialize_Prototype()
{
	_localPosition = Vec3::Zero;
	_localRotation = Quat::Identity;
	_localScale = Vec3::One;
	_localEulerAngles = Vec3::Zero;
	Mark_Dirty();

	return S_OK;
}

HRESULT Transform::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return S_OK;

	TRANSFORM_DESC* pDesc = static_cast<TRANSFORM_DESC*>(pArg);

	_speedPerSec = pDesc->fSpeedPerSec;
	_radianPerSec = XMConvertToRadians(pDesc->fDegreePerSec);
	_localPosition = pDesc->vPosition;
	_localScale = pDesc->vScale;
	Set_LocalRotation(pDesc->fPitch, pDesc->fYaw, pDesc->fRoll);

	Mark_Dirty();

	return S_OK;
}

void Transform::Set_LocalPosition(const Vec3& vPosition)
{
	_localPosition = vPosition;
	Mark_Dirty();
}

void Transform::Set_LocalPosition(FXMVECTOR vPosition)
{
	Set_LocalPosition(ToVec3(vPosition));
}

Vec3 Transform::Get_WorldPosition() const
{
	const Matrix& WorldMatrix = Get_WorldMatrix();
	return Vec3(WorldMatrix._41, WorldMatrix._42, WorldMatrix._43);
}

void Transform::Set_WorldPosition(const Vec3& vPosition)
{
	Set_LocalPosition(vPosition);
}

void Transform::Set_LocalRotation(const Quat& qRotation)
{
	_localRotation = qRotation;
	_localRotation.Normalize();

	const Vec3 vEuler = _localRotation.ToEuler();
	_localEulerAngles = Vec3(
		XMConvertToDegrees(vEuler.x),
		XMConvertToDegrees(vEuler.y),
		XMConvertToDegrees(vEuler.z));

	Mark_Dirty();
}

void Transform::Set_LocalRotation(float fPitch, float fYaw, float fRoll)
{
	const float fPitchRad = XMConvertToRadians(fPitch);
	const float fYawRad = XMConvertToRadians(fYaw);
	const float fRollRad = XMConvertToRadians(fRoll);

	Set_LocalRotation(Quat::CreateFromYawPitchRoll(fYawRad, fPitchRad, fRollRad));
	_localEulerAngles = Vec3(fPitch, fYaw, fRoll);
}

void Transform::Add_LocalRotation(const Quat& qDeltaRotation)
{
	_localRotation = _localRotation * qDeltaRotation;
	_localRotation.Normalize();

	const Vec3 vEuler = _localRotation.ToEuler();
	_localEulerAngles = Vec3(
		XMConvertToDegrees(vEuler.x),
		XMConvertToDegrees(vEuler.y),
		XMConvertToDegrees(vEuler.z));

	Mark_Dirty();
}

void Transform::Set_LocalScale(const Vec3& vScale)
{
	_localScale = vScale;
	Mark_Dirty();
}

Vec3 Transform::Get_WorldForward() const
{
	Vec3 vForward = ToVec3(Get_State(State::LOOK));
	if (vForward.LengthSquared() > 0.f)
		vForward.Normalize();

	return vForward;
}

Vec3 Transform::Get_WorldRight() const
{
	Vec3 vRight = ToVec3(Get_State(State::RIGHT));
	if (vRight.LengthSquared() > 0.f)
		vRight.Normalize();

	return vRight;
}

Vec3 Transform::Get_WorldUp() const
{
	Vec3 vUp = ToVec3(Get_State(State::UP));
	if (vUp.LengthSquared() > 0.f)
		vUp.Normalize();

	return vUp;
}

Vec3 Transform::Get_LocalForward() const
{
	return Vec3::Transform(Vec3(0.f, 0.f, 1.f), _localRotation);
}

Vec3 Transform::Get_LocalRight() const
{
	return Vec3::Transform(Vec3::Right, _localRotation);
}

Vec3 Transform::Get_LocalUp() const
{
	return Vec3::Transform(Vec3::Up, _localRotation);
}

void Transform::SetUp_Scale(float fScaleX, float fScaleY, float fScaleZ)
{
	Set_LocalScale(Vec3(fScaleX, fScaleY, fScaleZ));
}

void Transform::Scaling(float fScaleX, float fScaleY, float fScaleZ)
{
	Set_LocalScale(Vec3(_localScale.x * fScaleX, _localScale.y * fScaleY, _localScale.z * fScaleZ));
}

void Transform::Go_Straight(float fTimeDelta)
{
	Set_WorldPosition(Get_WorldPosition() + Get_WorldForward() * _speedPerSec * fTimeDelta);
}

void Transform::Go_Backward(float fTimeDelta)
{
	Set_WorldPosition(Get_WorldPosition() - Get_WorldForward() * _speedPerSec * fTimeDelta);
}

void Transform::Go_Right(float fTimeDelta)
{
	Set_WorldPosition(Get_WorldPosition() + Get_WorldRight() * _speedPerSec * fTimeDelta);
}

void Transform::Go_Left(float fTimeDelta)
{
	Set_WorldPosition(Get_WorldPosition() - Get_WorldRight() * _speedPerSec * fTimeDelta);
}

void Transform::Rotation(FXMVECTOR vAxis, float fDegree)
{
	Vec3 vAxisValue = ToVec3(vAxis);
	if (vAxisValue.LengthSquared() <= 0.f)
		return;

	vAxisValue.Normalize();
	Set_LocalRotation(Quat::CreateFromAxisAngle(vAxisValue, XMConvertToRadians(fDegree)));
}

void Transform::Turn(FXMVECTOR vAxis, float fTimeDelta)
{
	Vec3 vAxisValue = ToVec3(vAxis);
	if (vAxisValue.LengthSquared() <= 0.f)
		return;

	vAxisValue.Normalize();
	Add_LocalRotation(Quat::CreateFromAxisAngle(vAxisValue, _radianPerSec * fTimeDelta));
}

void Transform::LookAt(FXMVECTOR vAt)
{
	const Vec3 vTarget = ToVec3(vAt);
	Vec3 vDirection = vTarget - Get_WorldPosition();
	if (vDirection.LengthSquared() < 0.00001f)
		return;

	vDirection.Normalize();

	Matrix LookAtMatrix = Matrix::CreateLookAt(Vec3::Zero, vDirection, Vec3::Up);
	LookAtMatrix = LookAtMatrix.Invert();
	Set_LocalRotation(Quat::CreateFromRotationMatrix(LookAtMatrix));
}

HRESULT Transform::Bind_ShaderResource(Shader* pShader, const char* pConstantName)
{
	return pShader->Bind_Matrix(pConstantName, Get_WorldMatrixPtr());
}

void Transform::Mark_Dirty()
{
	_isDirty = true;
}

void Transform::Update_WorldMatrix() const
{
	_worldMatrix = Get_LocalMatrix();
	_isDirty = false;
}

void Transform::Decompose_WorldMatrix(const Matrix& WorldMatrix)
{
	Vec3 vScale = Vec3::One;
	Vec3 vTranslation = Vec3::Zero;
	Quat qRotation = Quat::Identity;

	Matrix DecomposeMatrix = WorldMatrix;
	if (DecomposeMatrix.Decompose(vScale, qRotation, vTranslation))
	{
		_localScale = vScale;
		_localPosition = vTranslation;
		Set_LocalRotation(qRotation);
	}
}

Vec3 Transform::ToVec3(FXMVECTOR vValue)
{
	return Vec3(XMVectorGetX(vValue), XMVectorGetY(vValue), XMVectorGetZ(vValue));
}

XMVECTOR Transform::ToVector(const Vec3& vValue, float fW)
{
	return XMVectorSet(vValue.x, vValue.y, vValue.z, fW);
}

Shared<Transform> Transform::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Transform> pInstance = MakeSharedFromRaw(new Transform(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Transform");
		Safe_Release(pInstance);
	}

	return pInstance;
}

Shared<Component> Transform::Clone(void* pArg)
{
	Shared<Transform> pInstance = MakeSharedFromRaw(new Transform(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Transform");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void Transform::Free()
{
	__super::Free();
}
