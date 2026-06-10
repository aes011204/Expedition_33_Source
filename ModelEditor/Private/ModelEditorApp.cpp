#include "pch.h"
#include "ModelEditorApp.h"
#include "ModelImGui_Manager.h"
#include "GameInstance.h"
#include "PostProcessPipeline.h"
#include "ModelEditorCamera.h"
#include "ModelViewerObject.h"
#include "Level_ModelEditor.h"

#include "ShaderCom.h"
#include "ComputeShaderCom.h"
#include "Vertex_Types.h"

#include <filesystem>

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

NS_BEGIN(ModelEditor)

HRESULT ModelEditorApp::Initialize()
{
	const std::filesystem::path assetRootPath(Get_ClientResourceRoot());
	
	ENGINE_DESC engineDesc{};
	engineDesc.hInstance = g_hInst;
	engineDesc.hWnd = g_hWnd;
	engineDesc.winMode = WinMode::Win;
	engineDesc.numLevels = 1; 
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	engineDesc.viewportWidth = rc.right - rc.left;
	engineDesc.viewportHeight = rc.bottom - rc.top;
	engineDesc.projectName = "ModelEditor";

	if (FAILED(GAME->Initialize_Engine(engineDesc, _device, _context)))
		return E_FAIL;

	if (FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring())))
		return E_FAIL;

	if (FAILED(ModelImGui_Manager::GetInstance()->Initialize(g_hWnd, _device, _context)))
		return E_FAIL;

	Shared<ShaderCom> vtxMeshShader =
		ShaderCom::Create(_device, _context,
			L"../../../Client/Bin/ShaderFiles/Shader_VtxMesh.hlsl",
			VTXMESH::Elements, VTXMESH::numElements, ShaderType::VtxMesh);
	if (vtxMeshShader == nullptr)
		return E_FAIL;
	if (FAILED(GAME->Add_Prototype(0, L"Shader_VtxMesh", vtxMeshShader)))
		return E_FAIL;

	Shared<ShaderCom> vtxAnimMeshShader =
		ShaderCom::Create(_device, _context,
			L"../../../Client/Bin/ShaderFiles/Shader_VtxAnimMesh.hlsl",
			VTXANIMMESH::Elements, VTXANIMMESH::numElements, ShaderType::VtxAnimMesh);
	if (vtxAnimMeshShader == nullptr)
		return E_FAIL;
	if (FAILED(GAME->Add_Prototype(0, L"Shader_VtxAnimMesh", vtxAnimMeshShader)))
		return E_FAIL;

    auto addComputeShader = [&](const wchar_t* prototypeTag, const wchar_t* shaderPath) -> HRESULT
    {
        Shared<ComputeShaderCom> shader =
            ComputeShaderCom::Create(_device, _context, shaderPath, "CS_Main");
        CHECK_NULL(shader, E_FAIL);
        return GAME->Add_Prototype(0, prototypeTag, shader);
    };

    CHECK_FAILED(addComputeShader(
        L"ComputeShader_BoneRemap",
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_BoneRemap.hlsl"), E_FAIL);

    CHECK_FAILED(addComputeShader(
        L"ComputeShader_AnimMeshSkinning",
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_AnimMeshSkinning.hlsl"), E_FAIL);

    CHECK_FAILED(addComputeShader(
        L"ComputeShader_MorphDeform",
        L"../../../Engine/Bin/ShaderFiles/Compute/Shader_Compute_MorphDeform.hlsl"), E_FAIL);

	if (FAILED(GAME->Add_Prototype(0, L"Prototype_GameObject_ModelEditorCamera", ModelEditorCamera::Create(_device, _context))))
		return E_FAIL;
	if (FAILED(GAME->Add_Prototype(0, L"Prototype_GameObject_ModelViewerObject", ModelViewerObject::Create(_device, _context))))
		return E_FAIL;

	if (FAILED(GAME->Change_Level(0, Level_ModelEditor::Create(_device, _context))))
		return E_FAIL;

    if (auto pp = GAME->Get_PostProcessPipeline())
    {
        pp->Set_PostProcessFeatures(PostProcessFeature::None);
    }

	return S_OK;
}

void ModelEditorApp::Priority_Update(float timeDelta)
{

}

void ModelEditorApp::Update(float timeDelta)
{
	GAME->Update_Engine(timeDelta);
	ModelImGui_Manager::GetInstance()->Update(timeDelta);
}

void ModelEditorApp::Late_Update(float timeDelta)
{
}

HRESULT ModelEditorApp::Render()
{
	const Vec4 clearColor = Vec4(0.15f, 0.15f, 0.15f, 0.f);
	if (FAILED(GAME->Clear_Buffers(clearColor)))
		return E_FAIL;

	if (FAILED(GAME->Draw()))
		return E_FAIL;

	ModelImGui_Manager::GetInstance()->Capture_BackBuffer_ToViewport();

	GAME->Bind_BackBuffer();
    GAME->Clear_Buffers(clearColor);
	ModelImGui_Manager::GetInstance()->Render();

	if (FAILED(GAME->Present()))
		return E_FAIL;

	return S_OK;
}

HRESULT ModelEditorApp::Ready_Fonts()
{
	return S_OK;
}

wstring ModelEditorApp::Get_ClientResourceRoot() const
{
	wchar_t modulePath[MAX_PATH] = {};
	if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
		return {};

	const std::filesystem::path runtimeRoot = std::filesystem::path(modulePath).parent_path();
	return std::filesystem::absolute(runtimeRoot / ResourcePath).lexically_normal().wstring();
}

wstring ModelEditorApp::Get_ClientResourcePath(const wchar_t* relativePath) const
{
	if (nullptr == relativePath || L'\0' == *relativePath)
		return Get_ClientResourceRoot();

	return (std::filesystem::path(Get_ClientResourceRoot()) / relativePath).lexically_normal().wstring();
}

ModelEditorApp::~ModelEditorApp()
{
}

Unique<ModelEditorApp> ModelEditorApp::Create()
{
	auto instance = Unique<ModelEditorApp>(new ModelEditorApp());

	if (FAILED(instance->Initialize()))
	{
        instance->Free();
		return nullptr;
	}

	return instance;
}

void ModelEditorApp::Free()
{
	ModelImGui_Manager::DestroyInstance();

	_context.Reset();
	_device.Reset();

    GameInstance::DestroyInstance();
}

NS_END
