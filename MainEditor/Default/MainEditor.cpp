#include <Windows.h>
#include <filesystem>
#include <d3d11.h>
#include <tchar.h>
#include <wrl/client.h>

#include "EditorInstance.h"
#include "Editor_ImGuiInclude.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const wchar_t* g_windowClassName = L"Expedition158MainEditor";
static const wchar_t* g_windowTitle = L"Expedition_158 MainEditor";
static HWND g_hWnd = nullptr;
static Microsoft::WRL::ComPtr<Device> g_device = nullptr;
static Microsoft::WRL::ComPtr<Context> g_context = nullptr;
static Microsoft::WRL::ComPtr<IDXGISwapChain> g_swapChain = nullptr;
static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_renderTargetView = nullptr;

static ATOM Register_MainEditorClass(HINSTANCE hInstance);

static bool Create_MainEditorWindow(HINSTANCE hInstance, int nCmdShow);

static bool Create_DeviceD3D(HWND hWnd);

static void Cleanup_RenderTarget();

static void Cleanup_DeviceD3D();

static void Create_RenderTarget();

static LRESULT CALLBACK MainEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static bool Initialize_EditorRuntime();

static void Shutdown_EditorRuntime();

static void Render_EditorFrame();
/* 실행 위치와 상관없이 공용 리소스를 찾도록 시작 시 작업 디렉터리를 Client\Bin으로 맞춘다. */
static bool Set_RuntimeWorkingDirectory();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!Set_RuntimeWorkingDirectory())
        return FALSE;

    Register_MainEditorClass(hInstance);

    if (!Create_MainEditorWindow(hInstance, nCmdShow))
        return FALSE;

    if (!Create_DeviceD3D(g_hWnd))
    {
        Cleanup_DeviceD3D();
        return FALSE;
    }

    if (!Initialize_EditorRuntime())
    {
        Shutdown_EditorRuntime();
        Cleanup_DeviceD3D();
        return FALSE;
    }

    MSG msg{};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        Render_EditorFrame();
    }

    Shutdown_EditorRuntime();
    Cleanup_DeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClassW(g_windowClassName, hInstance);

    return static_cast<int>(msg.wParam);
}

static ATOM Register_MainEditorClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_CLASSDC;
    wcex.lpfnWndProc = MainEditor_WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = g_windowClassName;

    return RegisterClassExW(&wcex);
}

static bool Create_MainEditorWindow(HINSTANCE hInstance, int nCmdShow)
{
    g_hWnd = CreateWindowW(
        g_windowClassName,
        g_windowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1600,
        900,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (nullptr == g_hWnd)
        return false;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return true;
}

static bool Create_DeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = 0;
    swapChainDesc.BufferDesc.Height = 0;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        g_swapChain.GetAddressOf(),
        g_device.GetAddressOf(),
        &featureLevel,
        g_context.GetAddressOf());

#ifdef _DEBUG
    if (FAILED(hr) && 0 != (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG))
    {
        createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            g_swapChain.GetAddressOf(),
            g_device.GetAddressOf(),
            &featureLevel,
            g_context.GetAddressOf());
    }
#endif

    if (FAILED(hr))
        return false;

    Create_RenderTarget();
    return true;
}

static void Cleanup_RenderTarget()
{
    g_renderTargetView.Reset();
}

static void Cleanup_DeviceD3D()
{
    Cleanup_RenderTarget();
    g_swapChain.Reset();
    g_context.Reset();
    g_device.Reset();
}

static void Create_RenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer = nullptr;
    if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()))))
        return;

    g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTargetView.GetAddressOf());
}

static LRESULT CALLBACK MainEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_SIZE:
        if (nullptr != g_device && SIZE_MINIMIZED != wParam)
        {
            Cleanup_RenderTarget();
            g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            Create_RenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

static bool Initialize_EditorRuntime()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(g_hWnd))
        return false;

    if (!ImGui_ImplDX11_Init(g_device.Get(), g_context.Get()))
        return false;

    EDITOR_DESC editorDesc{};
    editorDesc.hWnd = g_hWnd;
    editorDesc.eWinMode = WinMode::WIN;
    editorDesc.iViewportWidth = 1600;
    editorDesc.iViewportHeight = 900;

    return SUCCEEDED(EditorInstance::GetInstance()->Initialize_Editor(editorDesc, g_device, g_context));
}

static void Shutdown_EditorRuntime()
{
    EditorInstance::DestroyInstance();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

static void Render_EditorFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    EditorInstance::GetInstance()->Update_Editor(1.f / 60.f);
    EditorInstance::GetInstance()->Render_Editor();

    ImGui::Render();

    const float clearColor[4] = { 0.08f, 0.085f, 0.095f, 1.f };
    g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), nullptr);
    g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_swapChain->Present(1, 0);
}

/* 실행 파일 출력 폴더와 리소스 폴더를 분리하더라도 기존 상대 경로가 그대로 동작하도록 맞춘다. */
static bool Set_RuntimeWorkingDirectory()
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return false;

    const std::filesystem::path runtimeRoot = std::filesystem::path(modulePath).parent_path();
    const std::filesystem::path clientBinPath = (runtimeRoot / L"..\\..\\Client\\Bin").lexically_normal();

    if (!std::filesystem::exists(clientBinPath))
        return false;

    return SetCurrentDirectoryW(clientBinPath.c_str()) != FALSE;
}




