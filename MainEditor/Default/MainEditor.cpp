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
static bool g_runtimeInitialized = false;

static ATOM Register_MainEditorClass(HINSTANCE hInstance);
static bool Create_MainEditorWindow(HINSTANCE hInstance, int nCmdShow);
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
    constexpr DWORD kEditorWindowStyle = WS_POPUP | WS_VISIBLE;

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);

    HMONITOR hMonitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfo(hMonitor, &monitorInfo))
        return false;

    const RECT& monitorRect = monitorInfo.rcMonitor;
    RECT rcWindow = { 0, 0, monitorRect.right - monitorRect.left, monitorRect.bottom - monitorRect.top };
    AdjustWindowRect(&rcWindow, kEditorWindowStyle, FALSE);

    g_hWnd = CreateWindowW(
        g_windowClassName,
        g_windowTitle,
        kEditorWindowStyle,
        monitorRect.left,
        monitorRect.top,
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
        FreeConsole();
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
