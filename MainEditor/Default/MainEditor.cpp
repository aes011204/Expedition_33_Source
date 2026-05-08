#include <Windows.h>
#include <crtdbg.h>
#include <cstdio>
#include <locale.h>
#include <tchar.h>

#include "Client_Defines.h"
#include "GameInstance.h"
#include "MainEditorApp.h"
#include "Profile_View.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

FILE* g_debugConsole = nullptr;
HWND g_hWnd = nullptr;
HINSTANCE g_hInst = nullptr;

static const wchar_t* g_windowClassName = L"Expedition158MainEditor";
static const wchar_t* g_windowTitle = L"Expedition_158 MainEditor";
static constexpr DWORD g_fullscreenWindowStyle = WS_POPUP | WS_VISIBLE;
static constexpr DWORD g_windowedWindowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
static bool g_runtimeInitialized = false;
static bool g_isEditorFullscreen = false;
static RECT g_windowedRect = { 0, 0, 1600, 900 };

static ATOM Register_MainEditorClass(HINSTANCE hInstance);
static bool Create_MainEditorWindow(HINSTANCE hInstance, int nCmdShow);
static RECT Make_CenteredWindowRect(const RECT& areaRect, LONG clientWidth, LONG clientHeight);
static void Toggle_MainEditorFullscreen(HWND hWnd);
static LRESULT CALLBACK MainEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    Register_MainEditorClass(hInstance);

    if (!Create_MainEditorWindow(hInstance, nCmdShow))
        return FALSE;

    g_hInst = hInstance;

    Unique<MainEditor::MainEditorApp> mainApp = MainEditor::MainEditorApp::Create();
    if (nullptr == mainApp)
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_Default"))))
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_60FPS"))))
        return FALSE;

    g_runtimeInitialized = true;

    MSG msg{};
    float timeAcc = 0.f;
    auto previousFrameTime = MainEditor::Profile_View::clock::now();

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

            MainEditor::Profile_View::Frame_Begin();

            {
                MainEditor::Profile_View::Scope frameScope("Frame");
                mainApp->Priority_Update(dt);
                mainApp->Update(dt);

                mainApp->Late_Update(dt);
                mainApp->Render();
            }

            const auto currentFrameTime = MainEditor::Profile_View::clock::now();
            const double frameMs = std::chrono::duration<double, std::milli>(currentFrameTime - previousFrameTime).count();
            previousFrameTime = currentFrameTime;

            MainEditor::Profile_View::Set_Frame_Time_External(frameMs);
            MainEditor::Profile_View::Set_RenderCallCount_External(GAME->Get_RenderCall());
            MainEditor::Profile_View::Frame_End();

            timeAcc = 0.f;
        }
    }

    g_runtimeInitialized = false;
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
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);

    HMONITOR hMonitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfo(hMonitor, &monitorInfo))
        return false;

    g_windowedRect = Make_CenteredWindowRect(monitorInfo.rcWork, 1600, 900);

    g_hWnd = CreateWindowW(
        g_windowClassName,
        g_windowTitle,
        g_windowedWindowStyle,
        g_windowedRect.left,
        g_windowedRect.top,
        g_windowedRect.right - g_windowedRect.left,
        g_windowedRect.bottom - g_windowedRect.top,
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

static RECT Make_CenteredWindowRect(const RECT& areaRect, LONG clientWidth, LONG clientHeight)
{
    RECT windowRect = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&windowRect, g_windowedWindowStyle, FALSE);

    const LONG windowWidth = windowRect.right - windowRect.left;
    const LONG windowHeight = windowRect.bottom - windowRect.top;
    const LONG areaWidth = areaRect.right - areaRect.left;
    const LONG areaHeight = areaRect.bottom - areaRect.top;

    const LONG left = areaRect.left + max(0L, (areaWidth - windowWidth) / 2);
    const LONG top = areaRect.top + max(0L, (areaHeight - windowHeight) / 2);

    return RECT{ left, top, left + windowWidth, top + windowHeight };
}

static void Toggle_MainEditorFullscreen(HWND hWnd)
{
    if (g_isEditorFullscreen)
    {
        SetWindowLongPtrW(hWnd, GWL_STYLE, g_windowedWindowStyle);
        SetWindowPos(
            hWnd,
            nullptr,
            g_windowedRect.left,
            g_windowedRect.top,
            g_windowedRect.right - g_windowedRect.left,
            g_windowedRect.bottom - g_windowedRect.top,
            SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );
        g_isEditorFullscreen = false;
        return;
    }

    GetWindowRect(hWnd, &g_windowedRect);

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfo(hMonitor, &monitorInfo))
        return;

    const RECT& monitorRect = monitorInfo.rcMonitor;
    SetWindowLongPtrW(hWnd, GWL_STYLE, g_fullscreenWindowStyle);
    SetWindowPos(
        hWnd,
        HWND_TOP,
        monitorRect.left,
        monitorRect.top,
        monitorRect.right - monitorRect.left,
        monitorRect.bottom - monitorRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW
    );
    g_isEditorFullscreen = true;
}

static LRESULT CALLBACK MainEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (WM_SYSKEYDOWN == message && VK_RETURN == wParam && 0 != (HIWORD(lParam) & KF_ALTDOWN))
    {
        Toggle_MainEditorFullscreen(hWnd);
        return 0;
    }

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
            const uint32 width = LOWORD(lParam);
            const uint32 height = HIWORD(lParam);

            if (width > 0 && height > 0)
                GAME->Resize_BackBuffer(width, height);
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_MOUSEWHEEL:
        {
            float wheel = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam));
            GAME->Set_MouseWheel(wheel);
            break;

        }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
