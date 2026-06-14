#include <Windows.h>
#include <crtdbg.h>
#include <cstdio>
#include <locale.h>
#include <tchar.h>

#include "Engine_Define.h"
#include "GameInstance.h"
#include "MapEditorApp.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

FILE* g_debugConsole = nullptr;
HWND g_hWnd = nullptr;
HINSTANCE g_hInst = nullptr;

static const wchar_t* g_windowClassName = L"Expedition158MapEditor";
static const wchar_t* g_windowTitle = L"Expedition 158 Map Editor";
static bool g_runtimeInitialized = false;

static ATOM Register_MapEditorClass(HINSTANCE hInstance);
static bool Create_MapEditorWindow(HINSTANCE hInstance, int nCmdShow);
static LRESULT CALLBACK MapEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Register_MapEditorClass(hInstance);

    if (!Create_MapEditorWindow(hInstance, nCmdShow))
        return FALSE;

    g_hInst = hInstance;

    Unique<MapEditor::MapEditorApp> app = MapEditor::MapEditorApp::Create();
    if (nullptr == app)
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_Default"))))
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_60FPS"))))
        return FALSE;

    g_runtimeInitialized = true;

    MSG   msg{};
    float timeAcc = 0.f;

    float fpsTitleAcc = 0.f;
    int fpsFrameCount = 0;

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
            app->Priority_Update(dt);
            app->Update(dt);
            app->Late_Update(dt);
            app->Render();
            timeAcc = 0.f;

            fpsTitleAcc += dt;
            ++fpsFrameCount;
            if (fpsTitleAcc >= 0.5f)
            {
                const float fps = fpsFrameCount / fpsTitleAcc;
                const float ms = fpsTitleAcc / fpsFrameCount * 1000.f;
                WCHAR title[128] = {};
                swprintf_s(title, L"Expedition 158 Map Editor | FPS: %.0f | %.1f ms", fps, ms);
                SetWindowTextW(g_hWnd, title);
                fpsTitleAcc = 0.f;
                fpsFrameCount = 0;
            }
        }
    }

    g_runtimeInitialized = false;
    app->Free();
    app.reset();

    if (nullptr != g_hWnd && IsWindow(g_hWnd))
    {
        DestroyWindow(g_hWnd);
        g_hWnd = nullptr;
    }

    FreeConsole();

    return static_cast<int>(msg.wParam);
}

static ATOM Register_MapEditorClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MapEditor_WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_windowClassName;
    return RegisterClassExW(&wcex);
}

static bool Create_MapEditorWindow(HINSTANCE hInstance, int nCmdShow)
{
    RECT rc = { 0, 0, static_cast<LONG>(g_iWinSizeX), static_cast<LONG>(g_iWinSizeY) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindowW(
        g_windowClassName, g_windowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (nullptr == g_hWnd)
        return false;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return true;
}

static LRESULT CALLBACK MapEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_DPICHANGED:
    {
        const RECT* pRect = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hWnd, nullptr,
            pRect->left, pRect->top,
            pRect->right - pRect->left,
            pRect->bottom - pRect->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
