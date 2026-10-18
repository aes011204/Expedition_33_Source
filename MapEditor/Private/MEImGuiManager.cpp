#include "pch.h"
#include "MEImGuiManager.h"
#include "RenderTarget.h"
#include "GameInstance.h"

MEImGuiManager::~MEImGuiManager()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

HRESULT MEImGuiManager::Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    _device = device;
    _context = context;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    io.Fonts->AddFontFromFileTTF(
        "../../../Client/Bin/Resources/Fonts/Jua-Regular.ttf", 17.f,
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

void MEImGuiManager::Capture_BackBuffer_ToViewport()
{
    Shared<Engine::RenderTarget> renderTarget =
        Prepare_SharedViewportRenderTarget(
            GAME->Get_BackBufferWidth(),
            GAME->Get_BackBufferHeight());

    if (!renderTarget || !_context)
        return;

    ComPtr<IDXGISwapChain> swapChain = GAME->Get_SwapChain();
    if (!swapChain)
        return;

    ComPtr<ID3D11Texture2D> backBufferTexture;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(backBufferTexture.GetAddressOf()))))
        return;

    ComPtr<ID3D11Texture2D> viewportTexture = renderTarget->Get_Texture();
    if (!viewportTexture)
        return;

    _context->CopyResource(viewportTexture.Get(), backBufferTexture.Get());
}

ID3D11ShaderResourceView* MEImGuiManager::Get_SharedViewportSRV() const
{
    if (!_sharedViewportRenderTarget)
        return nullptr;

    return _sharedViewportRenderTarget->Get_SRV();
}

Shared<Engine::RenderTarget> MEImGuiManager::Prepare_SharedViewportRenderTarget(uint32 width, uint32 height)
{
    if (width == 0) width = 1;
    if (height == 0) height = 1;

    if (!_sharedViewportRenderTarget)
    {
        _sharedViewportRenderTarget = RenderTarget::Create(
            L"RenderTarget",
            _device, _context, width, height,
            DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f));
    }
    else if (FAILED(_sharedViewportRenderTarget->Resize(
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f))))
    {
        return nullptr;
    }

    return _sharedViewportRenderTarget;
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
