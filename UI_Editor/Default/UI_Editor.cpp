#include "pch.h"
#include "UI_EditorApp.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    constexpr wchar_t kWindowClassName[] = L"Expedition158UIEditor";
    constexpr wchar_t kWindowTitle[] = L"Expedition_158 UI Editor";

    HWND g_hWnd = nullptr;
    std::unique_ptr<UIEditor::UI_EditorApp> g_app = nullptr;

    ATOM Register_UIEditorClass(HINSTANCE hInstance);
    bool Create_UIEditorWindow(HINSTANCE hInstance, int nCmdShow);
    LRESULT CALLBACK UIEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    Register_UIEditorClass(hInstance);

    if (!Create_UIEditorWindow(hInstance, nCmdShow))
        return FALSE;

    g_app = std::make_unique<UIEditor::UI_EditorApp>();
    if (FAILED(g_app->Initialize(g_hWnd)))
        return FALSE;

    MSG msg {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        g_app->Render_Frame();
    }

    g_app->Shutdown();
    g_app.reset();

    return static_cast<int>(msg.wParam);
}

namespace
{
    ATOM Register_UIEditorClass(HINSTANCE hInstance)
    {
        WNDCLASSEXW wcex {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = UIEditor_WndProc;
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszClassName = kWindowClassName;

        return RegisterClassExW(&wcex);
    }

    bool Create_UIEditorWindow(HINSTANCE hInstance, int nCmdShow)
    {
        RECT rect { 0, 0, 1600, 900 };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        g_hWnd = CreateWindowW(
            kWindowClassName,
            kWindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

        if (g_hWnd == nullptr)
            return false;

        ShowWindow(g_hWnd, nCmdShow);
        UpdateWindow(g_hWnd);
        return true;
    }

    LRESULT CALLBACK UIEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
            return true;

        switch (message)
        {
        case WM_SIZE:
            if (g_app != nullptr && wParam != SIZE_MINIMIZED)
                g_app->Resize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}
