#include "pch.h"
#include "Camera_Editor.h"
#include "GameInstance.h"

NS_BEGIN(ModelEditor)

Camera_Editor::Camera_Editor(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Camera(device, context)
{
}

Camera_Editor::Camera_Editor(const Camera_Editor& prototype)
	: Camera(prototype)
{
}

HRESULT Camera_Editor::Initialize_Prototype()
{
	return Camera::Initialize_Prototype();
}

HRESULT Camera_Editor::Initialize(void* arg)
{
	if (FAILED(Camera::Initialize(arg)))
		return E_FAIL;

	return S_OK;
}

void Camera_Editor::Priority_Update(float timeDelta)
{
	Camera::Priority_Update(timeDelta);
}

void Camera_Editor::Update(float timeDelta)
{
	// 에디터용 프리/오르빗 카메라 조작
	if (GAME->KeyPress(KEY_TYPE::RBUTTON))
	{
		Vec2 mouseDelta = GAME->Get_MouseDelta();

		_transformCom->Turn(Vec3(0.f, 1.f, 0.f), mouseDelta.x * _mouseSensitivity * timeDelta);
		_transformCom->Turn(_transformCom->Get_State(TransformCom::STATE_RIGHT), mouseDelta.y * _mouseSensitivity * timeDelta);
	}

	if (GAME->KeyPress(KEY_TYPE::W))
		_transformCom->Go_Straight(_camSpeed * timeDelta);
	if (GAME->KeyPress(KEY_TYPE::S))
		_transformCom->Go_Backward(_camSpeed * timeDelta);
	if (GAME->KeyPress(KEY_TYPE::A))
		_transformCom->Go_Left(_camSpeed * timeDelta);
	if (GAME->KeyPress(KEY_TYPE::D))
		_transformCom->Go_Right(_camSpeed * timeDelta);

	float wheel = GAME->Get_MouseWheel();
	if (wheel != 0.f)
	{
		_transformCom->Go_Straight(wheel * _camSpeed * 2.f * timeDelta);
		GAME->Set_MouseWheel(0.f);
	}

	Camera::Update(timeDelta);
}

void Camera_Editor::Late_Update(float timeDelta)
{
	Camera::Late_Update(timeDelta);
}

HRESULT Camera_Editor::Render()
{
	return S_OK;
}

Shared<GameObject> Camera_Editor::Clone(void* arg)
{
	auto instance = make_shared<Camera_Editor>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : Camera_Editor");
		return nullptr;
	}

	return instance;
}

void Camera_Editor::Free()
{
	Camera::Free();
}

Shared<Camera_Editor> Camera_Editor::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<Camera_Editor>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Camera_Editor");
		return nullptr;
	}

	return instance;
}

NS_END
