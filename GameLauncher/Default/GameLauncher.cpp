#include <Windows.h>
#include <filesystem>
#include <crtdbg.h>
#include <cstdio>
#include <locale.h>
#include <tchar.h>

#include "Client_Defines.h"
#include "GameInstance.h"
#include "..\Public\MainApp.h"

#define MAX_LOADSTRING 100

FILE* g_debugConsole = nullptr;
HWND g_hWnd = nullptr;
HINSTANCE g_hInst = nullptr;

static WCHAR g_windowClass[MAX_LOADSTRING] = L"Expedition158GameLauncher";
static WCHAR g_windowTitle[MAX_LOADSTRING] = L"Expedition_158";

static ATOM Register_GameLauncherClass(HINSTANCE hInstance);
static BOOL Init_GameLauncherInstance(HINSTANCE hInstance, int nCmdShow);
static LRESULT CALLBACK GameLauncher_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

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

    Unique<GameLauncherApp::MainApp> mainApp = nullptr;

    Register_GameLauncherClass(hInstance);

    if (!Init_GameLauncherInstance(hInstance, nCmdShow))
        return FALSE;

    mainApp = GameLauncherApp::MainApp::Create();
    if (mainApp == nullptr)
        return FALSE;

    Engine::GameInstance* gameInstance = Engine::GameInstance::GetInstance();
    Safe_AddRef(gameInstance);

    if (FAILED(gameInstance->Add_Timer(TEXT("Timer_Default"))))
        return FALSE;

    if (FAILED(gameInstance->Add_Timer(TEXT("Timer_60"))))
        return FALSE;

    MSG msg{};
    float timeAcc = {};

    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        timeAcc += gameInstance->Compute_TimeDelta(TEXT("Timer_Default"));
        if (timeAcc >= 1.f / 60.f)
        {
            mainApp->Update(gameInstance->Compute_TimeDelta(TEXT("Timer_60")));
            mainApp->Render();
            timeAcc = 0.f;
        }
    }

    Safe_Release(gameInstance);
    mainApp->Free();
    mainApp.reset();

    return static_cast<int>(msg.wParam);
}

static ATOM Register_GameLauncherClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = GameLauncher_WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_windowClass;
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    return RegisterClassExW(&wcex);
}

static BOOL Init_GameLauncherInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_hInst = hInstance;

    RECT rcWindow = { 0, 0, static_cast<LONG>(Client::g_iWinSizeX), static_cast<LONG>(Client::g_iWinSizeY) };
    AdjustWindowRect(&rcWindow, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(
        g_windowClass,
        g_windowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        0,
        rcWindow.right - rcWindow.left,
        rcWindow.bottom - rcWindow.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    g_hWnd = hWnd;
    return TRUE;
}

static LRESULT CALLBACK GameLauncher_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
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
    case WM_CLOSE:
        FreeConsole();
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
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
