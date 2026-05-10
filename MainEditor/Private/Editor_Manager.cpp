#include "Editor_Manager.h"
#include "GameInstance.h"
#include "Console_View.h"
#include "EditorInstance.h"
#include "Scene_View.h"
#include "Game_View.h"
#include "Hierarchy_View.h"
#include "Content_Browser.h"
#include "Inspector.h"
#include "Camera_Tool.h"
#include "Profile_View.h"
#include "Material_View.h"

#include "Level_Tool.h"

#include "BT_View.h"

#include <filesystem>
#include <shellapi.h>

#include "Prefab_View.h"
#include "Animation_View.h"

namespace fs = std::filesystem;

Editor_Manager::Editor_Manager()
{

}

HRESULT Editor_Manager::Initialize()
{
    Add_Window(Scene_View::Create());
    Add_Window(Game_View::Create());
    Add_Window(Hierarchy_View::Create());
    Add_Window(Content_Browser::Create());
    Add_Window(Inspector::Create());
    Add_Window(Console_View::Create());
    Add_Window(Camera_Tool::Create());
    Add_Window(Profile_View::Create());

    Add_Window(BT_View::Create());

    Add_Window(Prefab_View::Create());
    Add_Window(Animation_View::Create());
    Add_Window(Level_Tool::Create());

    Add_Window(Material_View::Create());

    return S_OK;
}

void Editor_Manager::Update(float timeDelta)
{
    Handle_Shortcuts();

    for (auto& [name, window] : _windows)
    {
        if (window && window->Is_Open())
        {
            window->Update(timeDelta);
        }
    }

    // Update 이후에 Pre Render
    for (auto& [name, window] : _windows)
    {
        if (window && window->Is_Open())
            window->Pre_Render();
    }
}

void Editor_Manager::Render()
{
    Render_DockSpace();

    for (auto& Pair : _windows)
    {
        if (nullptr != Pair.second)
            Pair.second->Render();
    }
}

void Editor_Manager::Add_Window(const Shared<Editor_Window>& window)
{
    if (nullptr == window)
        return;

    _windows[window->Get_WindowName()] = window;
}

Shared<Editor_Window> Editor_Manager::Find_Window(const wstring& windowName) const
{
    const auto iter = _windows.find(windowName);
    if (iter == _windows.end())
        return nullptr;

    return iter->second;
}

void Editor_Manager::Render_DockSpace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (nullptr == viewport)
        return;

    Render_MainMenuBar();

    const float menuBarHeight = ImGui::GetFrameHeight();
    constexpr float toolbarHeight = 34.f;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - menuBarHeight));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

    bool isOpen = true;
    ImGui::Begin("MainEditorDockSpaceHost", &isOpen, windowFlags);
    ImGui::PopStyleVar(3);

    Render_RuntimeToolbar();

    ImGui::SetCursorPosY(toolbarHeight);
    const ImGuiID dockSpaceId = ImGui::GetID("MainEditorDockSpaceId");
    ImGui::DockSpace(dockSpaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

void Editor_Manager::Render_MainMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Exit"))
            Request_Exit();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        for (auto& pair : _windows)
        {
            if (nullptr == pair.second)
                continue;

            bool isWindowOpen = pair.second->Is_Open();
            const string windowName = String::ToString(pair.first);
            if (ImGui::MenuItem(windowName.c_str(), nullptr, &isWindowOpen))
                pair.second->Set_Open(isWindowOpen);
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Editor_Manager::Render_RuntimeToolbar()
{
    if (nullptr == EDITOR)
        return;

    const bool playing = EDITOR->IsPlaying();
    const bool paused = EDITOR->IsPaused();

    constexpr float toolbarHeight = 34.f;
    constexpr float toolbarControlWidth = 520.f;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
    ImGui::SetCursorPosX(max(12.f, (ImGui::GetWindowWidth() - toolbarControlWidth) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 4.f));

    if (playing)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.42f, 0.72f, 1.f));

    if (ImGui::Button("Play"))
    {
        if (paused)
            EDITOR->Resume();
        else
            EDITOR->Play();
    }

    if (playing)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    if (paused)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.46f, 0.10f, 1.f));

    if (ImGui::Button("Pause"))
        EDITOR->Pause();

    if (paused)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    if (!playing && !paused)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.32f, 0.36f, 0.40f, 1.f));

    if (ImGui::Button("Stop"))
        EDITOR->Stop();

    if (!playing && !paused)
        ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 10.f);

    float timeScale = EDITOR->Get_RuntimeTimeScale();
    ImGui::SetNextItemWidth(74.f);
    if (ImGui::DragFloat("##RuntimeTimeScale", &timeScale, 0.01f, 0.01f, 8.f, "%.2fx"))
        EDITOR->Set_RuntimeTimeScale(timeScale);

    ImGui::SameLine();
    if (ImGui::Button("1x"))
        EDITOR->Reset_RuntimeTimeScale();

    ImGui::SameLine();
    if (!paused)
        ImGui::BeginDisabled();

    if (ImGui::Button("Step"))
        EDITOR->Request_FrameStep();

    if (!paused)
        ImGui::EndDisabled();

    ImGui::SameLine();
    Render_ShortcutHelp();

    ImGui::SameLine();
    Render_LauncherButton();

    ImGui::SameLine();
    Render_ExitButton(toolbarHeight);
}

void Editor_Manager::Handle_Shortcuts()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) // 텍스트 입력 중일 때에는 무시
        return;

    // Ctrl + Z : Undo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        EDITOR->Undo();

    // Ctrl + Y : Redo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        EDITOR->Redo();

    // Ctrl + S : 저장
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        for (auto& [_, window] : _windows)
        {
            if (window && window->Is_Open() && window->Is_Focused() && window->CanSave())
            {
                window->Save();
                return;
            }
        }
    }

    // Ctrl + Space : Console, Context Browser 내리기
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        Toggle_WindowPair(L"Console", L"Content Browser");
        return;
    }

    // F5 : 게임 시작
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
    {
        if (EDITOR->IsPaused())
            EDITOR->Resume();
        else if (!EDITOR->IsPlaying())
            EDITOR->Play();

        return;
    }

    // F8 : 자유 카메라 전환
    if (ImGui::IsKeyPressed(ImGuiKey_F8, false))
    {
        GAME->SwitchTo_NextCamera();
        return;
    }

    // F9 : Pause + 자유카메라 전환
    if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
    {
        if (EDITOR->IsPlaying())
            EDITOR->Pause();
        else if (!EDITOR->IsPlaying())
            EDITOR->Play();

        return;
    }

    // Esc : 게임 종료
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        if (EDITOR->IsPlaying() || EDITOR->IsPaused())
            EDITOR->Stop();

        return;
    }

    // Shift +, - :배속 증가, 감소
    constexpr float timeScaleStep = 0.25f;
    if (io.KeyShift &&
        (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)))
    {
        const float nextTimeScale = EDITOR->Get_RuntimeTimeScale() + timeScaleStep;
        EDITOR->Set_RuntimeTimeScale(nextTimeScale);
        return;
    }

    if (io.KeyShift &&
        (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)))
    {
        const float nextTimeScale = EDITOR->Get_RuntimeTimeScale() - timeScaleStep;
        EDITOR->Set_RuntimeTimeScale(nextTimeScale);
        return;
    }
}

void Editor_Manager::Render_ShortcutHelp()
{
    if (ImGui::SmallButton("Shortcuts"))
    {
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();

        if (ImGui::BeginTable("ShortcutHelpTable", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 110.f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);

            auto DrawShortcutRow = [](const char* key, const char* description)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(key);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text(": %s", description);
            };

            DrawShortcutRow("F1", "충돌체 렌더링");
            DrawShortcutRow("F5", "Play / Resume");
            DrawShortcutRow("Esc", "Stop");
            DrawShortcutRow("F8", "카메라 전환");
            DrawShortcutRow("F9", "Pause + Free Camera");
            DrawShortcutRow("Ctrl + Wheel", "Free Camera Speed");
            DrawShortcutRow("Shift +", "배속 증가");
            DrawShortcutRow("Shift -", "배속 감소");
            DrawShortcutRow("Alt + Enter", "Fullscreen Toggle");
            DrawShortcutRow("Ctrl + Space", "Console + Content Browser");

            DrawShortcutRow("오브젝트 더블클릭", "오브젝트한테 이동");
            DrawShortcutRow("오브젝트 복사", "오브젝트 선택 후 Ctrl C + V");
            DrawShortcutRow("오브젝트 드래그 복사", "오브젝트 선택 후 Alt + 마우스 끌어서 이동");
            DrawShortcutRow("오브젝트 삭제", "오브젝트 선택 후 Delete");

            DrawShortcutRow("W E R", "Gizmo 전환");

            ImGui::EndTable();
        }

        ImGui::EndTooltip();
    }
}


void Editor_Manager::Toggle_Window(const wstring& windowName)
{
    Shared<Editor_Window> window = Find_Window(windowName);
    if (nullptr == window)
        return;

    window->Set_Open(!window->Is_Open());
}

void Editor_Manager::Toggle_WindowPair(const wstring& firstWindowName, const wstring& secondWindowName)
{
    Shared<Editor_Window> firstWindow = Find_Window(firstWindowName);
    Shared<Editor_Window> secondWindow = Find_Window(secondWindowName);

    if (nullptr == firstWindow || nullptr == secondWindow)
        return;

    const bool shouldOpen = !(firstWindow->Is_Open() || secondWindow->Is_Open());

    firstWindow->Set_Open(shouldOpen);
    secondWindow->Set_Open(shouldOpen);
}

void Editor_Manager::Render_ExitButton(float toolbarHeight)
{
    const float buttonWidth = 60.f;
    const float rightPadding = 12.f;

    float cursorX = ImGui::GetWindowWidth() - buttonWidth - rightPadding;
    if (cursorX > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(cursorX);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.20f, 0.20f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.24f, 0.24f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.14f, 0.14f, 1.f));

    if (ImGui::Button("Exit", ImVec2(buttonWidth, 0.f)))
        Request_Exit();

    ImGui::PopStyleColor(3);

    ImGui::PopStyleVar();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 lineStart = ImGui::GetCursorScreenPos();
    drawList->AddLine(
        ImVec2(lineStart.x, lineStart.y + 4.f),
        ImVec2(lineStart.x + ImGui::GetWindowWidth(), lineStart.y + 4.f),
        IM_COL32(55, 55, 55, 255));

    ImGui::SetCursorPosY(toolbarHeight);
}

void Editor_Manager::Request_Exit()
{
    PostQuitMessage(0);
}

void Editor_Manager::Render_LauncherButton()
{
    if (ImGui::SmallButton("Launch"))
    {
        if (!Launch_GameLauncher())
            MSG_BOX("Failed to launch GameLauncher.");
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("GameLauncher 실행");
        ImGui::EndTooltip();
    }
}

bool Editor_Manager::Launch_GameLauncher()
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return false;

    const fs::path editorExePath = fs::path(modulePath);
    const fs::path editorExeDir = editorExePath.parent_path();
    const fs::path configDirName = editorExeDir.filename();
    const fs::path repoRoot = editorExeDir.parent_path().parent_path().parent_path();

    const fs::path launcherExePath =
        (repoRoot / L"GameLauncher" / L"Bin" / configDirName / L"Expedition_158.exe").lexically_normal();

    if (!fs::exists(launcherExePath))
        return false;

    SHELLEXECUTEINFOW execInfo{};
    execInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    execInfo.lpVerb = L"open";
    execInfo.lpFile = launcherExePath.c_str();
    execInfo.lpDirectory = launcherExePath.parent_path().c_str();
    execInfo.nShow = SW_SHOWNORMAL;

    return TRUE == ShellExecuteExW(&execInfo);
}

Unique<Editor_Manager> Editor_Manager::Create()
{
    auto instance = make_unique<Editor_Manager>();

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}
