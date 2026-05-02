#include "UIObject.h"
#include "Shader.h"

UIObject::UIObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: GameObject { device, context }
{
}

UIObject::UIObject(const UIObject& prototype)
	: GameObject { prototype }
{
}

HRESULT UIObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT UIObject::Initialize(void* arg)
{
	UI_DESC* desc = static_cast<UI_DESC*>(arg);

	_positionX = desc->x;
	_positionY = desc->y;
	_sizeX = desc->sizeX;
	_sizeY = desc->sizeY;

	if (FAILED(__super::Initialize(arg)))
		return E_FAIL;

	D3D11_VIEWPORT ViewPortDesc{};
	uint32 numViewports = 1;
	_context->RSGetViewports(&numViewports, &ViewPortDesc);

	_viewPortWidth = ViewPortDesc.Width;
	_viewPortHeight = ViewPortDesc.Height;

	Update_Transform();

	XMStoreFloat4x4(&_transformMatrices[ETOI(D3DTS::View)], XMMatrixIdentity());
	XMStoreFloat4x4(&_transformMatrices[ETOI(D3DTS::Proj)], XMMatrixOrthographicLH(_viewPortWidth, _viewPortHeight, 0.f, 1.f));

	return S_OK;
}

void UIObject::Priority_Update(float timeDelta)
{
}

void UIObject::Update(float timeDelta)
{
}

void UIObject::Late_Update(float timeDelta)
{
}

HRESULT UIObject::Render()
{
	return S_OK;
}

void UIObject::Update_Transform()
{
	_transformCom->Set_Scale(Vec3(_sizeX, _sizeY, 1.f));
	_transformCom->Set_Position(Vec3(
		_positionX - _viewPortWidth * 0.5f,
		-_positionY + _viewPortHeight * 0.5f,
		0.f));
}

HRESULT UIObject::Bind_ShaderResource(Shader* shader, const char* constantName, D3DTS transformState)
{
	return shader->Bind_Matrix(constantName, &_transformMatrices[ETOI(transformState)]);
}

void UIObject::Free()
{
	__super::Free();
}


