#include "pch.h"
#include "ImGui_Manager.h"
#include "GameInstance.h"

ImGui_Manager::~ImGui_Manager()
{
    Free();
}

HRESULT ImGui_Manager::Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

    io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/malgun.ttf",
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesKorean()
    );

    // === Font Awesome 추가 ===
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = 18.0f;

        const string iconFontPath = Get_ClientResourcePath(
            L"Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Solid-900.otf"
        );
        io.Fonts->AddFontFromFileTTF(
            iconFontPath.c_str(),
            18.0f,
            &icons_config,
            icons_ranges
        );
    }

    ImGuiStyleSetting();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(device.Get(), context.Get());

    return S_OK;
}

void ImGui_Manager::Update(float timeDelta)
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGui_Manager::Render()
{
    // Rendering
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // 멀티 뷰포트
    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGui_Manager::ImGuiStyleSetting()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // 1. 크기 및 패딩 (좀 더 널찍하고 클릭하기 편하게)
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 12.0f;

    // 2. 모서리 둥글기 (이게 언리얼 감성의 핵심입니다. 너무 둥글지도, 각지지도 않게)
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;

    // 3. 테두리 (구분감을 주어 깔끔하게)
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;

    // 4. 색상 팔레트 (Deep Dark & Subtle Accent)
    ImVec4* colors = style.Colors;

    // 배경은 완전 어두운 회색 (블랙에 가까움)
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

    // 테두리는 아주 얇고 은은하게
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // 프레임 (버튼, 텍스트박스 등 배경)
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    // 타이틀 바 (창 제목)
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);

    // 탭 바
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);

    // 헤더 (트리 노드, 콜랩싱 헤더 등)
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

    // 포인트 컬러 (언리얼처럼 살짝 채도 낮고 고급스러운 블루)
    const ImVec4 accentColor = ImVec4(0.15f, 0.45f, 0.85f, 1.00f);
    const ImVec4 accentColorHovered = ImVec4(0.25f, 0.55f, 0.95f, 1.00f);
    ImVec4 accentColorActive = ImVec4(0.10f, 0.35f, 0.75f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive] = accentColor; // 클릭 시 블루 포인트

    colors[ImGuiCol_CheckMark] = accentColorHovered;
    colors[ImGuiCol_SliderGrab] = accentColor;
    colors[ImGuiCol_SliderGrabActive] = accentColorHovered;

    // 도킹 프리뷰 (도킹할 때 나오는 반투명 색상)
    colors[ImGuiCol_DockingPreview] = ImVec4(0.15f, 0.45f, 0.85f, 0.40f);
}

string ImGui_Manager::Get_ClientResourcePath(const wchar_t* relativePath)
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

Unique<ImGui_Manager> ImGui_Manager::Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_unique<ImGui_Manager>();

    if (FAILED(instance->Initialize(hWnd, device, context)))
    {
        LOG_CRITICAL("Failed to Create : ImGui_Manager");
        MSG_BOX("Failed to Create : ImGui_Manager");
        return nullptr;
    }

    return instance;
}

void ImGui_Manager::Free()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}
