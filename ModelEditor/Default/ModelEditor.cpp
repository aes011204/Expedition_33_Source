#include "pch.h"

#ifdef new
#undef new
#endif

#include "framework.h"
#include "ModelEditor.h"
#include "ModelEditorApp.h"
#include "GameInstance.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

#ifdef _DEBUG
#define new DBG_NEW
#endif

#define MAX_LOADSTRING 100

HINSTANCE g_hInst;
HWND g_hWnd;

WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MODELEDITOR, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    g_hInst = hInstance;

    auto mainApp = ModelEditor::ModelEditorApp::Create();
    if (nullptr == mainApp)
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_Default"))))
        return FALSE;

    if (FAILED(GAME->Add_Timer(TEXT("Timer_60FPS"))))
        return FALSE;

    MSG msg;
    float timeAcc = 0.f;

    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (WM_QUIT == msg.message)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            timeAcc += GAME->Compute_TimeDelta(TEXT("Timer_Default"));

            if (timeAcc >= 1.f / 60.f)
            {
                float dt = GAME->Compute_TimeDelta(TEXT("Timer_60FPS"));

                mainApp->Priority_Update(dt);
                mainApp->Update(dt);
                mainApp->Late_Update(dt);
                mainApp->Render();

                timeAcc = 0.f;
            }
        }
    }

    mainApp->Free();
    mainApp.reset();

    GAME->Free();
    GAME->Release_Engine();

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MODELEDITOR));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = nullptr; 
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hInst = hInstance;

   g_hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 1600, 900, nullptr, nullptr, hInstance, nullptr);

   if (!g_hWnd)
   {
      return FALSE;
   }

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_MOUSEWHEEL:
        GAME->Set_MouseWheel((float)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
        break;
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
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
