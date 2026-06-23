#include "Editor_Manager.h"
#include "GameInstance.h"
#include "Console_View.h"
#include "EditorInstance.h"
#include "Notification_Manager.h"
#include "Scene_View.h"
#include "Game_View.h"
#include "Hierarchy_View.h"
#include "Content_Browser.h"
#include "Inspector.h"
#include "Camera_Tool.h"
#include "Profile_View.h"
#include "Material_View.h"
#include "Cinematic_Tool.h"
#include "BattleCameraPreview_View.h"
//#include "BT_Tool.h"
#include "Level_Tool.h"
#include "BT_View.h"
#include <cctype>
#include <filesystem>
#include <shellapi.h>
#include <commdlg.h>
#include "Prefab_View.h"
#include "Animation_View.h"
#include "AnimState_View.h"
#include "ClientInstance.h"
#include "PostProcess_View.h"
#include "ScreenFx_View.h"
#include "SoulLike_Viewer.h"
#include "SoulLikeCamera_Viewer.h"

namespace fs = std::filesystem;

namespace
{
    string SanitizeCheckpointName(string name)
    {
        for (char& ch : name)
        {
            switch (ch)
            {
            case '\\':
            case '/':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                ch = '_';
                break;
            default:
                break;
            }
        }

        while (!name.empty() && isspace(static_cast<unsigned char>(name.front())))
            name.erase(name.begin());
        while (!name.empty() && isspace(static_cast<unsigned char>(name.back())))
            name.pop_back();

        if (name.empty())
            name = "Demo";

        return name;
    }

    bool PickCheckpointFileDialog(const fs::path& initialDir, fs::path& outPath)
    {
        wchar_t fileName[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hWnd;
        ofn.lpstrFilter = L"체크포인트 (*.checkpoint.bin)\0*.checkpoint.bin\0모든 파일 (*.*)\0*.*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
        ofn.lpstrDefExt = L"checkpoint.bin";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&ofn))
            return false;

        outPath = fs::path(fileName);
        return true;
    }

    bool PickRuleStateFileDialog(const fs::path& initialDir, fs::path& outPath)
    {
        wchar_t fileName[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hWnd;
        ofn.lpstrFilter = L"룰 상태 (*.rule.bin)\0*.rule.bin\0모든 파일 (*.*)\0*.*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
        ofn.lpstrDefExt = L"rule.bin";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&ofn))
            return false;

        outPath = fs::path(fileName);
        return true;
    }
}

Editor_Manager::Editor_Manager()
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 != GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
    {
        const fs::path editorExeDir = fs::path(modulePath).parent_path();
        const fs::path configDirName = editorExeDir.filename();

        if (configDirName == L"Release")
            _launcherConfig = LauncherConfig::Release;
    }
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
    Add_Window(Cinematic_Tool::Create());
    Add_Window(BattleCameraPreview_View::Create());
    Add_Window(BT_View::Create());
    Add_Window(Prefab_View::Create());
    Add_Window(Animation_View::Create());
    Add_Window(Level_Tool::Create());

    Add_Window(Material_View::Create());
    Add_Window(AnimState_View::Create());
    Add_Window(PostProcess_View::Create());
    Add_Window(ScreenFx_View::Create());
    Add_Window(SoulLike_Viewer::Create());
    Add_Window(SoulLikeCamera_Viewer::Create());
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

    for (auto& [name, window] : _windows)
    {
        if (window && window->Is_Open())
            window->Render();
    }

    Render_SaveCheckpointPopup();
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
        if (ImGui::BeginMenu("데모 체크포인트"))
        {
            Render_CheckpointButtons();
            ImGui::EndMenu();
        }

        ImGui::Separator();

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
    constexpr float toolbarControlWidth = 760.f;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
    ImGui::SetCursorPosX(max(12.f, (ImGui::GetWindowWidth() - toolbarControlWidth) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 4.f));

    Render_SoundCheck();
    ImGui::SameLine(0.f, 10.f);

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
    
    // Ctrl + Space : Scene/Game/Profile/Camera Tool 집중 모드
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        Toggle_FocusMode();
        return;
    }

    // F5 : 게임 시작
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
    {
        if (EDITOR->IsPaused())
            EDITOR->Resume();
        else if (!EDITOR->IsPlaying())
            EDITOR->Play();

        EDITOR->Force_EnablePlayerInputForPlay();
        return;
    }

    // F8 : 자유 카메라 전환
    if (ImGui::IsKeyPressed(ImGuiKey_F8, false))
    {
        if (GAME->Is_ActiveCameraType(ObjectType::FreeCamera))
            EDITOR->Restore_PreviousCameraOrTarget();
        else
            EDITOR->Activate_FreeCamera();

        //GAME->SwitchTo_NextCamera();

        return;
    }

    // F9 : Pause + 자유카메라 전환
    if (!io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F9, false))
    {
        if (EDITOR->IsPlaying())
            EDITOR->Pause();
        else if (EDITOR->IsPaused())
            EDITOR->Resume();
        else
            EDITOR->Play();

        return;
    }

    // F10 : 마우스 락 수동 토글
    if (ImGui::IsKeyPressed(ImGuiKey_F10, false))
    {
        EDITOR->Toggle_MouseLockOverride();
        return;
    }

    // Esc : 게임 종료
    if (ImGui::IsKeyPressed(ImGuiKey_F4, false))
    {
        if (GAME->Is_GameplayPaused())
        {
            return;
        }
    
        if (EDITOR->IsPlaying() || EDITOR->IsPaused())
            EDITOR->Stop();
    
        return;
    }

    // Ctrl + - : 배속 1x 복귀
    if (io.KeyCtrl &&
        (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)))
    {
        EDITOR->Reset_RuntimeTimeScale();
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
    if (ImGui::SmallButton("단축키 모음"))
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
            DrawShortcutRow("F4", "Stop");
            DrawShortcutRow("F8", "카메라 전환");
            DrawShortcutRow("F9", "Pause + Free Camera");
            DrawShortcutRow("F10", "마우스 언락 / 자동 복귀");
            DrawShortcutRow("Ctrl + Wheel", "Free Camera Speed");
            DrawShortcutRow("Shift +", "배속 증가");
            DrawShortcutRow("Shift -", "배속 감소");
            DrawShortcutRow("Ctrl -", "배속 1x 복귀");
            DrawShortcutRow("Alt + Enter", "Fullscreen Toggle");
            DrawShortcutRow("Ctrl + Space", "Focus Scene/Game/Profile/Camera Tool");

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

void Editor_Manager::Toggle_FocusMode()
{
    if (_isFocusMode)
    {
        for (auto& [name, window] : _windows)
        {
            if (nullptr == window)
                continue;

            const auto iter = _focusModeWindowStates.find(name);
            if (iter != _focusModeWindowStates.end())
                window->Set_Open(iter->second);
        }

        _focusModeWindowStates.clear();
        _isFocusMode = false;
        return;
    }

    _focusModeWindowStates.clear();

    for (auto& [name, window] : _windows)
    {
        if (nullptr == window)
            continue;

        _focusModeWindowStates[name] = window->Is_Open();

        if (name == L"Scene" || name == L"Game" || name == L"Profile" || name == L"Console" || name == L"Camera Tool" || name == L"SoulLike_Viewer")
            continue;

        window->Set_Open(false);
    }

    _isFocusMode = true;
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
    const char* previewLabel =
        (_launcherConfig == LauncherConfig::Debug) ? "Debug" : "Release";

    ImGui::SetNextItemWidth(78.f);
    if (ImGui::BeginCombo("##LauncherConfig", previewLabel))
    {
        const bool isDebugSelected = (_launcherConfig == LauncherConfig::Debug);
        if (ImGui::Selectable("Debug", isDebugSelected))
            _launcherConfig = LauncherConfig::Debug;
        if (isDebugSelected)
            ImGui::SetItemDefaultFocus();

        const bool isReleaseSelected = (_launcherConfig == LauncherConfig::Release);
        if (ImGui::Selectable("Release", isReleaseSelected))
            _launcherConfig = LauncherConfig::Release;
        if (isReleaseSelected)
            ImGui::SetItemDefaultFocus();

        ImGui::EndCombo();
    }

    ImGui::SameLine(0.f, 6.f);
    if (ImGui::SmallButton("Launch"))
    {
        if (!Launch_GameLauncher())
            MSG_BOX("Failed to launch GameLauncher.");
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("GameLauncher 실행 (%s)", previewLabel);
        ImGui::EndTooltip();
    }

    ImGui::SameLine(0.f, 16.f);
    Render_ReloadDataButton();
}

void Editor_Manager::Render_CheckpointButtons()
{
    Notification_Manager* notification = EDITOR->Get_Notification();

    if (ImGui::BeginMenu("체크포인트"))
    {
        if (ImGui::MenuItem("체크포인트 저장"))
        {
            Open_SaveCheckpointPopup();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("현재 위치, 월드 오브젝트, 인벤토리, 룰 상태를 함께 저장");

        if (ImGui::MenuItem("체크포인트 열기..."))
        {
            fs::path path{};
            if (Pick_CheckpointFile(path))
            {
                _selectedCheckpointPath = path;
                if (EDITOR->Load_DemoCheckpointInEditor(_selectedCheckpointPath))
                {
                    if (notification)
                    {
                        notification->Add_Notification_With_Type(
                            NotifyType::Success,
                            "체크포인트 로드 완료: {}",
                            String::ToString(_selectedCheckpointPath.wstring()));
                    }
                }
                else
                {
                    if (notification)
                        notification->Add_Notification_With_Type(NotifyType::Error, "체크포인트 로드 실패.");
                    MSG_BOX("체크포인트 로드에 실패했습니다.");
                }
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("체크포인트 파일 선택 후 현재 레벨에 적용");

        if (ImGui::MenuItem("체크포인트로 실행"))
        {
            if (_selectedCheckpointPath.empty())
                _selectedCheckpointPath = Resolve_DefaultCheckpointPath();

            if (!fs::exists(_selectedCheckpointPath) || !Launch_GameLauncher(&_selectedCheckpointPath))
            {
                if (notification)
                    notification->Add_Notification_With_Type(NotifyType::Error, "체크포인트 실행 실패.");
                MSG_BOX("체크포인트로 GameLauncher 실행에 실패했습니다.");
            }
            else if (notification)
            {
                notification->Add_Notification_With_Type(
                    NotifyType::Success,
                    "체크포인트 실행 요청: {}",
                    String::ToString(_selectedCheckpointPath.wstring()));
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("선택된 체크포인트로 GameLauncher 실행");

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("룰 상태"))
    {
        Render_RuleStateButtons();
        ImGui::Separator();

        if (ImGui::MenuItem("룰 상태 열기..."))
        {
            fs::path path{};
            if (Pick_RuleStateFile(path))
            {
                _selectedRuleStatePath = path;
                if (EDITOR->Load_RuleStateInEditor(_selectedRuleStatePath))
                {
                    if (notification)
                    {
                        notification->Add_Notification_With_Type(
                            NotifyType::Success,
                            "룰 상태 로드 완료: {}",
                            String::ToString(_selectedRuleStatePath.wstring()));
                    }
                }
                else
                {
                    if (notification)
                        notification->Add_Notification_With_Type(NotifyType::Error, "룰 상태 로드 실패.");
                    MSG_BOX("룰 상태 로드에 실패했습니다.");
                }
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("위치와 월드는 유지하고 의상/룰 완료 상태만 적용");

        ImGui::EndMenu();
    }
}

void Editor_Manager::Render_RuleStateButtons()
{
    if (ImGui::MenuItem("게임 시작 상태 저장"))
        Save_RuleStatePreset(RuleStatePreset::GameStart);

    if (ImGui::MenuItem("마임 옷 해금 상태 저장"))
        Save_RuleStatePreset(RuleStatePreset::MimeCostumeOnly);

    if (ImGui::MenuItem("포테이토 옷 해금 상태 저장"))
        Save_RuleStatePreset(RuleStatePreset::PotatoCostumeOnly);

    if (ImGui::MenuItem("의상 해금 전체 상태 저장"))
        Save_RuleStatePreset(RuleStatePreset::AllCostumes);

    if (ImGui::MenuItem("튜토리얼 후 플레이어 해금 상태 저장"))
        Save_RuleStatePreset(RuleStatePreset::FirstTutorialHeroUnlocks);
}

void Editor_Manager::Render_SaveCheckpointPopup()
{
    if (_openSaveCheckpointPopup)
    {
        _openSaveCheckpointPopup = false;
        ImGui::OpenPopup("데모 체크포인트 저장");
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport != nullptr)
    {
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
            ImGuiCond_Appearing,
            ImVec2(0.5f, 0.5f));
    }
    ImGui::SetNextWindowSize(ImVec2(420.f, 0.f), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("데모 체크포인트 저장", nullptr, ImGuiWindowFlags_NoResize))
        return;

    ImGui::TextUnformatted("체크포인트 이름");
    ImGui::SetNextItemWidth(320.f);
    const bool enterPressed = ImGui::InputText(
        "##CheckpointName",
        _checkpointSaveNameBuffer,
        IM_ARRAYSIZE(_checkpointSaveNameBuffer),
        ImGuiInputTextFlags_EnterReturnsTrue);

    const fs::path previewPath = Resolve_CheckpointPathFromName(_checkpointSaveNameBuffer);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextDisabled("%s", String::ToString(previewPath.wstring()).c_str());
    ImGui::PopTextWrapPos();

    ImGui::Separator();

    const bool savePressed = ImGui::Button("저장", ImVec2(90.f, 0.f));
    ImGui::SameLine();
    if (ImGui::Button("취소", ImVec2(90.f, 0.f)))
        ImGui::CloseCurrentPopup();

    if (savePressed || enterPressed)
    {
        if (Save_CheckpointToPath(previewPath))
            ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Editor_Manager::Open_SaveCheckpointPopup()
{
    string defaultName = "Demo";
    if (!_selectedCheckpointPath.empty())
    {
        defaultName = String::ToString(_selectedCheckpointPath.stem().wstring());
        if (defaultName.ends_with(".checkpoint"))
            defaultName = defaultName.substr(0, defaultName.size() - string(".checkpoint").size());
    }

    strncpy_s(_checkpointSaveNameBuffer, defaultName.c_str(), _TRUNCATE);
    _openSaveCheckpointPopup = true;
}

bool Editor_Manager::Save_CheckpointToPath(const fs::path& checkpointPath)
{
    _selectedCheckpointPath = checkpointPath;

    Notification_Manager* notification = EDITOR->Get_Notification();
    if (EDITOR->Capture_DemoCheckpoint(_selectedCheckpointPath))
    {
        if (notification)
        {
            notification->Add_Notification_With_Type(
                NotifyType::Success,
                "체크포인트 저장 완료: {}",
                String::ToString(_selectedCheckpointPath.wstring()));
        }
        return true;
    }

    if (notification)
        notification->Add_Notification_With_Type(NotifyType::Error, "체크포인트 저장 실패.");
    MSG_BOX("체크포인트 저장에 실패했습니다.");
    return false;
}

bool Editor_Manager::Save_RuleStatePreset(RuleStatePreset preset)
{
    const bool unlockBaguette =
        preset == RuleStatePreset::MimeCostumeOnly ||
        preset == RuleStatePreset::AllCostumes;

    const bool unlockPotato =
        preset == RuleStatePreset::PotatoCostumeOnly ||
        preset == RuleStatePreset::AllCostumes;

    const fs::path ruleStatePath = Resolve_RuleStatePath(preset);
    _selectedRuleStatePath = ruleStatePath;

    Notification_Manager* notification = EDITOR->Get_Notification();
    bool saved = false;
    if (preset == RuleStatePreset::GameStart)
        saved = EDITOR->Save_GameStartRuleState(ruleStatePath);
    else if (preset == RuleStatePreset::FirstTutorialHeroUnlocks)
        saved = EDITOR->Save_FirstTutorialHeroUnlockRuleState(ruleStatePath);
    else
        saved = EDITOR->Save_CostumeUnlockRuleState(ruleStatePath, unlockBaguette, unlockPotato);

    if (saved)
    {
        if (notification)
        {
            notification->Add_Notification_With_Type(
                NotifyType::Success,
                "룰 상태 저장 완료: {}",
                String::ToString(ruleStatePath.wstring()));
        }

        return true;
    }

    if (notification)
        notification->Add_Notification_With_Type(NotifyType::Error, "룰 상태 저장 실패.");

    MSG_BOX("룰 상태 저장에 실패했습니다.");
    return false;
}

fs::path Editor_Manager::Resolve_DefaultCheckpointPath() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    const fs::path editorExeDir = fs::path(modulePath).parent_path();
    const fs::path repoRoot = editorExeDir.parent_path().parent_path().parent_path();
    return (repoRoot / L"Client" / L"Bin" / L"Resources" / L"Debug" / L"Checkpoints" / L"Demo.checkpoint.bin").lexically_normal();
}

fs::path Editor_Manager::Resolve_CheckpointPathFromName(const string& checkpointName) const
{
    string fileName = SanitizeCheckpointName(checkpointName);
    if (!fileName.ends_with(".checkpoint.bin"))
        fileName += ".checkpoint.bin";

    return Resolve_DefaultCheckpointPath().parent_path() / String::ToWString(fileName);
}

fs::path Editor_Manager::Resolve_DefaultRuleStatePath() const
{
    return Resolve_DefaultCheckpointPath()
        .parent_path()
        .parent_path()
        / L"RuleStates"
        / L"Rule_GameStart.rule.bin";
}

fs::path Editor_Manager::Resolve_RuleStatePath(RuleStatePreset preset) const
{
    const fs::path ruleStateDir = Resolve_DefaultRuleStatePath().parent_path();

    switch (preset)
    {
    case RuleStatePreset::GameStart:
        return ruleStateDir / L"Rule_GameStart.rule.bin";

    case RuleStatePreset::MimeCostumeOnly:
        return ruleStateDir / L"Rule_MimeCostume.rule.bin";

    case RuleStatePreset::PotatoCostumeOnly:
        return ruleStateDir / L"Rule_PotatoCostume.rule.bin";

    case RuleStatePreset::AllCostumes:
        return ruleStateDir / L"Rule_AllCostumes.rule.bin";

    case RuleStatePreset::FirstTutorialHeroUnlocks:
        return ruleStateDir / L"Rule_FirstTutorialHeroUnlocks.rule.bin";

    default:
        return ruleStateDir / L"Rule_Debug.rule.bin";
    }
}

bool Editor_Manager::Pick_CheckpointFile(fs::path& outPath) const
{
    fs::path initialDir = Resolve_DefaultCheckpointPath().parent_path();
    if (!_selectedCheckpointPath.empty() && _selectedCheckpointPath.has_parent_path())
        initialDir = _selectedCheckpointPath.parent_path();

    return PickCheckpointFileDialog(initialDir, outPath);
}

bool Editor_Manager::Pick_RuleStateFile(fs::path& outPath) const
{
    fs::path initialDir = Resolve_DefaultRuleStatePath().parent_path();
    if (!_selectedRuleStatePath.empty() && _selectedRuleStatePath.has_parent_path())
        initialDir = _selectedRuleStatePath.parent_path();

    return PickRuleStateFileDialog(initialDir, outPath);
}

void Editor_Manager::Render_ReloadDataButton()
{
    if (ImGui::SmallButton("Reload Data"))
        ImGui::OpenPopup("ReloadDataPopup");
    const bool reloadDataHovered = ImGui::IsItemHovered();

    if (ImGui::BeginPopup("ReloadDataPopup"))
    {
        if (ImGui::MenuItem("GameDataTable"))
        {
            if (FAILED(CLIENT->Reload_GameDataTables()))
                MSG_BOX("Failed to reload game data tables.");
        }

        if (ImGui::MenuItem("Effect Data"))
        {
            if (FAILED(CLIENT->Reload_EffectJsons()))
                MSG_BOX("Failed to reload effect jsons.");
        }

        ImGui::EndPopup();
    }

    if (reloadDataHovered)
    {
        ImGui::BeginTooltip();
        ImGui::Text("재로드할 데이터 종류 선택");
        ImGui::EndTooltip();
    }
}

void Editor_Manager::Render_SoundCheck()
{
    bool muted = GAME->Is_AllSoundMuted();
    if (ImGui::Checkbox("사운드 차단", &muted))
        GAME->Set_AllSoundMuted(muted);

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("체크 시 모든 사운드 재생을 차단합니다.");
        ImGui::EndTooltip();
    }
}

bool Editor_Manager::Launch_GameLauncher(const fs::path* checkpointPath)
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return false;

    const fs::path editorExePath = fs::path(modulePath);
    const fs::path editorExeDir = editorExePath.parent_path();
    const fs::path repoRoot = editorExeDir.parent_path().parent_path().parent_path();
    const fs::path configDirName =
        (_launcherConfig == LauncherConfig::Debug) ? L"Debug" : L"Release";

    const fs::path launcherExePath =
        (repoRoot / L"GameLauncher" / L"Bin" / configDirName / L"Expedition_158.exe").lexically_normal();
    const fs::path launcherExeDir = launcherExePath.parent_path();

    if (!fs::exists(launcherExePath))
        return false;

    wstring parameters{};
    if (checkpointPath != nullptr && !checkpointPath->empty())
        parameters = L"--checkpoint \"" + fs::absolute(*checkpointPath).lexically_normal().wstring() + L"\"";

    SHELLEXECUTEINFOW execInfo{};
    execInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    execInfo.lpVerb = L"open";
    execInfo.lpFile = launcherExePath.c_str();
    execInfo.lpDirectory = launcherExeDir.c_str();
    execInfo.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
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
