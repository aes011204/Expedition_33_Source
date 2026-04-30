#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL Transform final : public Component
{
public:
	typedef struct tagTransformDesc
	{
		float fSpeedPerSec = 0.f;
		float fDegreePerSec = 0.f;
		Vec3  vPosition = Vec3::Zero;
		Vec3  vScale = Vec3::One;
		float fPitch = 0.f;
		float fYaw = 0.f;
		float fRoll = 0.f;
	}TRANSFORM_DESC;

public:
	Transform(ComPtr<Device> device, ComPtr<Context> context);
	Transform(const Transform& Prototype);
	virtual ~Transform() { Free(); }

public:
	XMVECTOR Get_State(State eState) const;

	void Set_State(State eState, FXMVECTOR vState);

	XMFLOAT3 Get_Scaled() const;

	const XMFLOAT4X4* Get_WorldMatrixPtr() const;

	const Matrix& Get_WorldMatrix() const;

	Matrix Get_LocalMatrix() const;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;

public:
	Vec3 Get_LocalPosition() const { return _localPosition; }

	void Set_LocalPosition(const Vec3& vPosition);

	void Set_LocalPosition(FXMVECTOR vPosition);

	Vec3 Get_WorldPosition() const;

	void Set_WorldPosition(const Vec3& vPosition);

	void Set_LocalRotation(const Quat& qRotation);

	void Set_LocalRotation(float fPitch, float fYaw, float fRoll);

	void Add_LocalRotation(const Quat& qDeltaRotation);

	Vec3 Get_LocalEulerAngles() const { return _localEulerAngles; }

	Vec3 Get_LocalScale() const { return _localScale; }

	void Set_LocalScale(const Vec3& vScale);

	Vec3 Get_WorldForward() const;

	Vec3 Get_WorldRight() const;

	Vec3 Get_WorldUp() const;

	Vec3 Get_LocalForward() const;

	Vec3 Get_LocalRight() const;

	Vec3 Get_LocalUp() const;

public:
	void SetUp_Scale(float fScaleX, float fScaleY, float fScaleZ);

	void Scaling(float fScaleX, float fScaleY, float fScaleZ);

	void Go_Straight(float fTimeDelta);

	void Go_Backward(float fTimeDelta);

	void Go_Right(float fTimeDelta);

	void Go_Left(float fTimeDelta);

	void Rotation(FXMVECTOR vAxis, float fDegree);

	void Turn(FXMVECTOR vAxis, float fTimeDelta);

	void LookAt(FXMVECTOR vAt);

public:
	HRESULT Bind_ShaderResource(class Shader* pShader, const char* pConstantName);

private:
	void Mark_Dirty();

	void Update_WorldMatrix() const;

	void Decompose_WorldMatrix(const Matrix& WorldMatrix);

	static Vec3 ToVec3(FXMVECTOR vValue);

	static XMVECTOR ToVector(const Vec3& vValue, float fW = 0.f);

private:
	Vec3 _localPosition = Vec3::Zero;
	Quat _localRotation = Quat::Identity;
	Vec3 _localScale = Vec3::One;
	Vec3 _localEulerAngles = Vec3::Zero;
	float _speedPerSec = 0.f;
	float _radianPerSec = 0.f;
	mutable Matrix _worldMatrix = Matrix::Identity;
	mutable bool _isDirty = true;

public:
	static Shared<Transform> Create(ComPtr<Device> device, ComPtr<Context> context);

	virtual Shared<Component> Clone(void* pArg) override;

	virtual void Free() override;
};

NS_END

