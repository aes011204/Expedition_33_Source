#include "UIObject.h"
#include "Shader.h"

UIObject::UIObject(ComPtr<Device> device, ComPtr<Context> context)
	: GameObject { device, context }
{
}

UIObject::UIObject(const UIObject& Prototype)
	: GameObject { Prototype }
{
}

HRESULT UIObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT UIObject::Initialize(void* pArg)
{
	UI_DESC* pDesc = static_cast<UI_DESC*>(pArg);

	_positionX = pDesc->fX;
	_positionY = pDesc->fY;
	_sizeX = pDesc->fSizeX;
	_sizeY = pDesc->fSizeY;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	D3D11_VIEWPORT ViewPortDesc{};
	uint32 iNumViewports = 1;
	_context->RSGetViewports(&iNumViewports, &ViewPortDesc);

	_viewPortWidth = ViewPortDesc.Width;
	_viewPortHeight = ViewPortDesc.Height;

	Update_Transform();

	XMStoreFloat4x4(&_transformMatrices[ETOI(D3DTS::View)], XMMatrixIdentity());
	XMStoreFloat4x4(&_transformMatrices[ETOI(D3DTS::Proj)], XMMatrixOrthographicLH(_viewPortWidth, _viewPortHeight, 0.f, 1.f));

	return S_OK;
}

void UIObject::Priority_Update(float fTimeDelta)
{
}

void UIObject::Update(float fTimeDelta)
{
}

void UIObject::Late_Update(float fTimeDelta)
{
}

HRESULT UIObject::Render()
{
	return S_OK;
}

void UIObject::Update_Transform()
{
	_transformCom->SetUp_Scale(_sizeX, _sizeY, 1.f);
	_transformCom->Set_State(State::Position,
		XMVectorSet(_positionX - _viewPortWidth * 0.5f, -_positionY + _viewPortHeight * 0.5f, 0.f, 1.f));
}

HRESULT UIObject::Bind_ShaderResource(Shader* pShader, const char* pConstantName, D3DTS eTransformState)
{
	return pShader->Bind_Matrix(pConstantName, &_transformMatrices[ETOI(eTransformState)]);
}

void UIObject::Free()
{
	__super::Free();
}


