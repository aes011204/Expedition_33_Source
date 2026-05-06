#include "pch.h"
#include "MEImGuiManager.h"

MEImGuiManager::~MEImGuiManager()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

HRESULT MEImGuiManager::Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    io.Fonts->AddFontFromFileTTF(
        "../../Bin/Resources/Fonts/Jua-Regular.ttf", 17.f,
        nullptr, io.Fonts->GetGlyphRangesKorean());

    ApplyStyle();

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(device.Get(), context.Get());

    return S_OK;
}

void MEImGuiManager::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void MEImGuiManager::Render()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void MEImGuiManager::ApplyStyle()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(10.f, 10.f);
    style.FramePadding = ImVec2(6.f, 4.f);
    style.ItemSpacing = ImVec2(6.f, 4.f);
    style.WindowRounding = 4.f;
    style.FrameRounding = 3.f;
    style.TabRounding = 3.f;
    style.WindowBorderSize = 1.f;
    style.FrameBorderSize = 1.f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.45f, 0.85f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.15f, 0.45f, 0.85f, 0.40f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
}

Unique<MEImGuiManager> MEImGuiManager::Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_unique<MEImGuiManager>();

    if (FAILED(instance->Initialize(hWnd, device, context)))
    {
        MSG_BOX("Failed to Create MEImGuiManager");
        return nullptr;
    }

    return instance;
}
