#include "pch.h"
#include "ModelImGui_Manager.h"
#include "GameInstance.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <implot.h>

#include "LogWindow.h"
#include "ModelConverter.h"


NS_BEGIN(ModelEditor)

    ModelImGui_Manager::~ModelImGui_Manager()
{
	Free();
}

HRESULT ModelImGui_Manager::Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

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
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

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
			    ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
    }
    ImGui::End();

    _logWindow->Update(timeDelta);
    _modelConverter->Update(timeDelta);
}

void ModelImGui_Manager::Render()
{
    _logWindow->Render();
    _modelConverter->Render();

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
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

    _logWindow.reset();
    _modelConverter.reset();
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
	const std::filesystem::path resourceRoot =
		std::filesystem::absolute(runtimeRoot / L"../../../Client/Bin/Resources").lexically_normal();

	if (nullptr == relativePath || L'\0' == *relativePath)
		return resourceRoot.string();

	return (resourceRoot / relativePath).lexically_normal().string();
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
