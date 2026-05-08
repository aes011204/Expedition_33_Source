#include "pch.h"
#include "UI_Editor_ImGuiManager.h"

NS_BEGIN(UIEditor)

UI_Editor_ImGuiManager::~UI_Editor_ImGuiManager()
{
    ShutDown();
}

HRESULT UI_Editor_ImGuiManager::Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    if (nullptr == hWnd || nullptr == device || nullptr == context)
        return E_FAIL;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImFont* editorFont = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/malgun.ttf",
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesKorean());
    if (nullptr != editorFont)
        io.FontDefault = editorFont;
    else
        io.Fonts->AddFontDefault();

    Apply_Style();

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(device.Get(), context.Get());

    _initialized = true;
    return S_OK;
}

void UI_Editor_ImGuiManager::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UI_Editor_ImGuiManager::Render()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UI_Editor_ImGuiManager::ShutDown()
{
    if (!_initialized)
        return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    _initialized = false;
}

void UI_Editor_ImGuiManager::Apply_Style()
{
    ImGui::StyleColorsDark();
}

Unique<UI_Editor_ImGuiManager> UI_Editor_ImGuiManager::Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_unique<UI_Editor_ImGuiManager>();

    if (FAILED(instance->Initialize(hWnd, device, context)))
    {
        MSG_BOX("Failed to Create : UI_Editor_ImGuiManager");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_Editor_ImGuiManager::Free()
{
    ShutDown();
    __super::Free();
}

NS_END
