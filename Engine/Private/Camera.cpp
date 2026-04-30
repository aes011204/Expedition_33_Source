#include "Camera.h"
#include "GameInstance.h"

Camera::Camera(ComPtr<Device> device, ComPtr<Context> context)
	: GameObject { device, context }
{
}

Camera::Camera(const Camera& Prototype)
	: GameObject{ Prototype }
	, _fovy{ Prototype._fovy }
	, _near{ Prototype._near }
	, _far{ Prototype._far }
	, _aspect{ Prototype._aspect }
{
}

HRESULT Camera::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Camera::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	CAMERA_DESC* pDesc = static_cast<CAMERA_DESC*>(pArg);

	_transformCom->Set_State(State::Position, XMLoadFloat4(&pDesc->vEye));
	_transformCom->LookAt(XMLoadFloat4(&pDesc->vAt));

	_fovy = pDesc->fFovy;
	_near = pDesc->fNear;
	_far = pDesc->fFar;
	_aspect = static_cast<float>(GAME->Get_ViewportWidth()) /
		static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

	Update_TransformMatrices();

	return S_OK;
}

void Camera::Priority_Update(float fTimeDelta)
{
}

void Camera::Update(float fTimeDelta)
{
}

void Camera::Late_Update(float fTimeDelta)
{
}

HRESULT Camera::Render()
{
	return S_OK;
}

void Camera::Update_TransformMatrices()
{
	_aspect = static_cast<float>(GAME->Get_ViewportWidth()) /
		static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

	GAME->Set_Transform(D3DTS::View,
		XMMatrixInverse(nullptr, XMLoadFloat4x4(_transformCom->Get_WorldMatrixPtr())));

	GAME->Set_Transform(D3DTS::Proj,
		XMMatrixPerspectiveFovLH(_fovy, _aspect, _near, _far));
}

void Camera::Free()
{
	__super::Free();
}
