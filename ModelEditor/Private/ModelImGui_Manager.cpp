#include "pch.h"
#include "ModelImGui_Manager.h"
#include "GameInstance.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <implot.h>

#include "LogWindow.h"
#include "ModelConverter.h"
#include "ModelInspector.h"
#include "ModelSceneWindow.h"
#include "ModelTestPanel.h"
#include "GltfMorphBuilder.h"
#include "RenderTarget.h"


NS_BEGIN(ModelEditor)

IMPLEMENT_SINGLETON(ModelImGui_Manager)

ModelImGui_Manager::~ModelImGui_Manager()
{
}

HRESULT ModelImGui_Manager::Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	_device = device;
	_context = context;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); 
    (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGuiStyleSetting();

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());

    if (nullptr == (_logWindow = LogWindow::Create()))
        MSG_BOX("[Model Editor] : Failed to Create LogWindow!");

    if (nullptr == (_modelConverter = ModelConverter::Create(hWnd)))
        LOG_ERROR("[Model Editor] : Failed to Create ModelConverter!");

    if (nullptr == (_modelInspector = ModelInspector::Create(hWnd)))
        LOG_ERROR("[Model Editor] : Failed to Create ModelInspector!");

    if (nullptr == (_modelSceneWindow = ModelSceneWindow::Create()))
        LOG_ERROR("[Model Editor] : Failed to Create ModelSceneWindow!");

    if (nullptr == (_modelTestPanel = ModelTestPanel::Create()))
        LOG_ERROR("[Model Editor] : Failed to Create ModelTestPanel!");

    if (nullptr == (_gltfMorphBuilder = GltfMorphBuilder::Create()))
        LOG_ERROR("[Model Editor] : Failed to Create GltfMorphBuilder!");

	return S_OK;
}

void ModelImGui_Manager::Update(float timeDelta)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("Main Panel", nullptr, window_flags);
    {
        ImGuiID dockspaceID = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Window"))
			{
				if (ImGui::MenuItem("Log Console")) { _logWindow->ToggleBlock(); }
                if (ImGui::MenuItem("Model Converter")) { _modelConverter->ToggleBlock(); }
                if (ImGui::MenuItem("Model Inspector")) { _modelInspector->ToggleBlock(); }
                if (ImGui::MenuItem("GLB Morph Builder")) { _gltfMorphBuilder->ToggleBlock(); }
                if (ImGui::MenuItem("Model Test Panel")) { _modelTestPanel->ToggleBlock(); }
			    ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
    }
    ImGui::End();

    _logWindow->Update(timeDelta);
    _modelConverter->Update(timeDelta);
    _modelInspector->Update(timeDelta);
    _gltfMorphBuilder->Update(timeDelta);
    _modelTestPanel->Update(timeDelta);
}

void ModelImGui_Manager::Render()
{
    _logWindow->Render();
    _modelConverter->Render();
    _modelInspector->Render();
    _gltfMorphBuilder->Render();
    _modelTestPanel->Render();
    if (_modelSceneWindow) _modelSceneWindow->Render();

    ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void ModelImGui_Manager::Free()
{
    _logWindow.reset();
    _modelConverter.reset();
    _modelInspector.reset();
    _modelSceneWindow.reset();
    _modelTestPanel.reset();
    _gltfMorphBuilder.reset();
    _sharedViewportRenderTarget.reset();

	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

    _context.Reset();
    _device.Reset();
}

void ModelImGui_Manager::ImGuiStyleSetting()
{
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.TabRounding = 3.0f;
}

string ModelImGui_Manager::Get_ClientResourcePath(const wchar_t* relativePath)
{
	wchar_t modulePath[MAX_PATH] = {};
	if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
		return {};

	const std::filesystem::path runtimeRoot = std::filesystem::path(modulePath).parent_path();
	const std::filesystem::path resourceRoot = std::filesystem::absolute(runtimeRoot / L"../../../Client/Bin/Resources").lexically_normal();

	if (nullptr == relativePath || L'\0' == *relativePath)
		return resourceRoot.string();

	return (resourceRoot / relativePath).lexically_normal().string();
}

void ModelImGui_Manager::Request_ViewportSize(uint32 width, uint32 height)
{
    _requestedViewportWidth = max(1u, width);
    _requestedViewportHeight = max(1u, height);
}

Shared<::Engine::RenderTarget> ModelImGui_Manager::Prepare_SharedViewportRenderTarget(uint32 width, uint32 height)
{
    if (0 == width) width = 1;
    if (0 == height) height = 1;

    if (nullptr == _sharedViewportRenderTarget)
    {
        _sharedViewportRenderTarget = RenderTarget::Create(
            L"RenderTarget",
            _device,
            _context,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4(0.f, 0.f, 0.f, 0.f));
    }
    else if (FAILED(_sharedViewportRenderTarget->Resize(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4(0.f, 0.f, 0.f, 0.f))))


    {
        return nullptr;
    }

    return _sharedViewportRenderTarget;
}

ID3D11ShaderResourceView* ModelImGui_Manager::Get_SharedViewportSRV() const
{
    if (nullptr == _sharedViewportRenderTarget)
        return nullptr;

    return _sharedViewportRenderTarget->Get_SRV();
}

void ModelImGui_Manager::Capture_BackBuffer_ToViewport()
{
    Shared<::Engine::RenderTarget> renderTarget =
            Prepare_SharedViewportRenderTarget(
                GAME->Get_BackBufferWidth(),
                GAME->Get_BackBufferHeight());

    if (nullptr == renderTarget || nullptr == _context)
        return;

    ComPtr<IDXGISwapChain> swapChain = GAME->Get_SwapChain();
    if (nullptr == swapChain)
        return;

    ComPtr<ID3D11Texture2D> backBufferTexture = nullptr;
    if (FAILED(swapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(backBufferTexture.GetAddressOf()))))
    {
        return;
    }

    ComPtr<ID3D11Texture2D> viewportTexture = renderTarget->Get_Texture();
    if (nullptr == viewportTexture)
        return;

    _context->CopyResource(viewportTexture.Get(), backBufferTexture.Get());
}

Unique<ModelImGui_Manager> ModelImGui_Manager::Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = Unique<ModelImGui_Manager>(new ModelImGui_Manager());

	if (FAILED(instance->Initialize(hWnd, device, context)))
	{
		return nullptr;
	}

	return instance;
}

NS_END
