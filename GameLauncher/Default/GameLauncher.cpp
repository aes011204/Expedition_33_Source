#include <Windows.h>
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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    Unique<GameLauncherApp::MainApp> mainApp = nullptr;

    Register_GameLauncherClass(hInstance);

    if (!Init_GameLauncherInstance(hInstance, nCmdShow))
        return FALSE;

    mainApp = GameLauncherApp::MainApp::Create();
    if (mainApp == nullptr)
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_Default"))))
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_60FPS"))))
        return FALSE;

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

        timeAcc += GAME->Compute_TimeDelta(TEXT("Timer_Default"));
        if (timeAcc >= 1.f / 60.f)
        {
            const float dt = GAME->Compute_TimeDelta(TEXT("Timer_60FPS"));

            mainApp->Update(dt);
            mainApp->Render();

            timeAcc = 0.f;
        }
    }

    mainApp->Free();
    mainApp.reset();

    if (nullptr != g_hWnd && IsWindow(g_hWnd))
    {
        DestroyWindow(g_hWnd);
        g_hWnd = nullptr;
    }

    FreeConsole();

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

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);

    HMONITOR hMonitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfo(hMonitor, &monitorInfo))
        return FALSE;

    const RECT& monitorRect = monitorInfo.rcMonitor;

    RECT rcWindow = { 0, 0, static_cast<LONG>(Client::g_iWinSizeX), static_cast<LONG>(Client::g_iWinSizeY) };
    AdjustWindowRect(&rcWindow, WS_OVERLAPPEDWINDOW, FALSE);

    const int windowWidth = rcWindow.right - rcWindow.left;
    const int windowHeight = rcWindow.bottom - rcWindow.top;

    const int posX = monitorRect.left + ((monitorRect.right - monitorRect.left) - windowWidth) / 2;
    const int posY = monitorRect.top + ((monitorRect.bottom - monitorRect.top) - windowHeight) / 2;

    HWND hWnd = CreateWindowW(
        g_windowClass,
        g_windowTitle,
        WS_OVERLAPPEDWINDOW,
        posX,          
        posY,          
        windowWidth,
        windowHeight,
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
        PostQuitMessage(0);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
