#include "pch.h"
#include "Level_ModelEditor.h"
#include "GameInstance.h"

#include "ModelEditorCamera.h"
#include "ModelImGui_Manager.h"
#include "ModelViewerObject.h"

NS_BEGIN(ModelEditor)

    Level_ModelEditor::Level_ModelEditor(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Level(device, context)
{
}

HRESULT Level_ModelEditor::Initialize()
{
	if (FAILED(Level::Initialize()))
		return E_FAIL;

	if (FAILED(Ready_Lights()))
		return E_FAIL;

	if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_Viewer(TEXT("Layer_Viewer"))))
		return E_FAIL;

	return S_OK;
}

void Level_ModelEditor::Update(float timeDelta)
{
	Level::Update(timeDelta);
}

HRESULT Level_ModelEditor::Render()
{
	if (FAILED(Level::Render()))
		return E_FAIL;

	return S_OK;
}

HRESULT Level_ModelEditor::Ready_Layer_Camera(const wstring& layerTag)
{
	ModelEditorCamera::ModelEditorCameraDesc camDesc{};
    camDesc.eye = Vec4{ 0.f, 10.f, -20.f, 1.f };
    camDesc.at = Vec4{ 0.f, 0.f, 0.f, 1.f };
	camDesc.fovy = XMConvertToRadians(60.0f);
	camDesc.nearPlane = 0.1f;
	camDesc.farPlane = 50000.f;
	camDesc.mouseSensor = 0.1f;
	camDesc.speedPerSec = 8.f;
	camDesc.yaw = 0.f;
	camDesc.pitch = 0.f;

	if (FAILED(GAME->Add_GameObject(0, L"Prototype_GameObject_ModelEditorCamera", 0, layerTag, &camDesc)))
	{
		LOG_ERROR("Failed to add ModelEditorCamera in Level_ModelEditor");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Level_ModelEditor::Ready_Layer_Viewer(const wstring& layerTag)
{
	
    Shared<GameObject> viewerObject = nullptr;
    if (FAILED(GAME->Add_GameObject(0, L"Prototype_GameObject_ModelViewerObject", 0, layerTag, viewerObject)))
    {
        LOG_ERROR("Failed to add ModelViewerObject in Level_ModelEditor");
        return E_FAIL;
    }

    ModelImGui_Manager::GetInstance()->Set_CurrentModelObject(static_pointer_cast<ModelViewerObject>(viewerObject));

	return S_OK;
}

HRESULT Level_ModelEditor::Ready_Lights()
{
	Engine::LIGHT_DESC lightDesc{};
	lightDesc.type = Engine::LightType::Directional;
	lightDesc.direction = Vec4(1.f, -1.f, 1.f, 0.f);
	lightDesc.direction.Normalize();
	lightDesc.diffuse = Vec4(1.f, 1.f, 1.f, 1.f);
	lightDesc.ambient = Vec4(0.7f, 0.7f, 0.7f, 1.f);
	lightDesc.specular = Vec4(1.f, 1.f, 1.f, 1.f);

	if (FAILED(GAME->Add_Light(lightDesc)))
	{
		LOG_ERROR("Failed to add Light in Level_ModelEditor");
		return E_FAIL;
	}

	return S_OK;
}

Shared<Level_ModelEditor> Level_ModelEditor::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<Level_ModelEditor>(device, context);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : Level_ModelEditor");
		return nullptr;
	}

	return instance;
}

void Level_ModelEditor::Free()
{
	Level::Free();
}

NS_END
