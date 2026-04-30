#include <Windows.h>
#include <filesystem>
#include <crtdbg.h>
#include <cstdio>
#include <locale.h>
#include <tchar.h>

#include "Client_Defines.h"
#include "GameInstance.h"
#include "MainEditorApp.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

FILE* g_debugConsole = nullptr;
HWND g_hWnd = nullptr;
HINSTANCE g_hInst = nullptr;

static const wchar_t* g_windowClassName = L"Expedition158MainEditor";
static const wchar_t* g_windowTitle = L"Expedition_158 MainEditor";
static bool g_runtimeInitialized = false;

static ATOM Register_MainEditorClass(HINSTANCE hInstance);
static bool Create_MainEditorWindow(HINSTANCE hInstance, int nCmdShow);
static LRESULT CALLBACK MainEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static bool Set_RuntimeWorkingDirectory();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!Set_RuntimeWorkingDirectory())
        return FALSE;

    Register_MainEditorClass(hInstance);

    if (!Create_MainEditorWindow(hInstance, nCmdShow))
        return FALSE;

    g_hInst = hInstance;

    Unique<MainEditor::MainEditorApp> mainApp = MainEditor::MainEditorApp::Create();
    if (nullptr == mainApp)
        return FALSE;

    g_runtimeInitialized = true;

    MSG msg{};
    float timeAcc = 0.f;

    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        Engine::GameInstance* gameInstance = Engine::GameInstance::GetInstance();
        Safe_AddRef(gameInstance);

        timeAcc += gameInstance->Compute_TimeDelta(TEXT("Timer_Default"));
        if (timeAcc >= 1.f / 60.f)
        {
            const float dt = gameInstance->Compute_TimeDelta(TEXT("Timer_60"));

            mainApp->Priority_Update(dt);
            mainApp->Update(dt);
            mainApp->Late_Update(dt);
            mainApp->Render();

            timeAcc = 0.f;
        }

        Safe_Release(gameInstance);
    }

    g_runtimeInitialized = false;
    mainApp->Free();
    mainApp.reset();

    return static_cast<int>(msg.wParam);
}

static ATOM Register_MainEditorClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainEditor_WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_windowClassName;

    return RegisterClassExW(&wcex);
}

static bool Create_MainEditorWindow(HINSTANCE hInstance, int nCmdShow)
{
    RECT rcWindow = { 0, 0, static_cast<LONG>(Client::g_iWinSizeX), static_cast<LONG>(Client::g_iWinSizeY) };
    AdjustWindowRect(&rcWindow, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindowW(
        g_windowClassName,
        g_windowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rcWindow.right - rcWindow.left,
        rcWindow.bottom - rcWindow.top,
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

static LRESULT CALLBACK MainEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_runtimeInitialized && ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_CREATE:
    {
        AllocConsole();
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        _tfreopen_s(&g_debugConsole, _T("CONOUT$"), _T("w"), stdout);
        _tfreopen_s(&g_debugConsole, _T("CONIN$"), _T("r"), stdin);
        _tfreopen_s(&g_debugConsole, _T("CONERR$"), _T("w"), stderr);
        _tsetlocale(LC_ALL, _T(""));
        break;
    }

    case WM_SIZE:
        if (g_runtimeInitialized && SIZE_MINIMIZED != wParam)
        {
            Engine::GameInstance* gameInstance = Engine::GameInstance::GetInstance();
            Safe_AddRef(gameInstance);

            const uint32 width = LOWORD(lParam);
            const uint32 height = HIWORD(lParam);
            if (width > 0 && height > 0)
                gameInstance->Resize_BackBuffer(width, height);

            Safe_Release(gameInstance);
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_CLOSE:
        FreeConsole();
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

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
