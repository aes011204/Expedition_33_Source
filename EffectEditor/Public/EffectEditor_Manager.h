#pragma once

#include "Base.h"
#include "EffectEditor_Enum.h"

NS_BEGIN(EffectEditor)

class Editor_Window;
class Emitter_View;

class EffectEditor_Manager final : public Base
{
public:
    EffectEditor_Manager() = default;
    ~EffectEditor_Manager() override;

public:
    HRESULT Initialize();
    void Update(float timeDelta);
    void Render();

    void Add_Window(const Shared<Editor_Window>& window);
    Shared<Editor_Window> Find_Window(const wstring& windowName) const;
    bool Is_SceneFocusModeActive() const { return _isSceneFocusModeActive; }
    uint32 Get_CurrentDockSpaceId() const { return _currentDockSpaceId; }

private:
    struct PreviewViewportSnapshot
    {
        PreviewViewportMode mode{ PreviewViewportMode::Auto };
        uint32 width{ 0 };
        uint32 height{ 0 };
    };

    map<wstring, Shared<Editor_Window>> _windows{};
    bool _defaultDockLayoutBuilt{ false };
    bool _isSceneFocusModeActive{ false };
    bool _sceneFocusDockLayoutBuilt{ false };
    uint32 _currentDockSpaceId{ 0 };
    map<wstring, bool> _sceneFocusWindowOpenSnapshot{};
    PreviewViewportSnapshot _sceneFocusPreviewViewportSnapshot{};
    string _sceneFocusDockSettingsSnapshot{};

private:
    // 등록된 editor window들의 초기화 hook을 한 번씩 실행한다.
    HRESULT Initialize_Windows();

    void Render_MainMenuBar();
    void Render_RuntimeToolbar();
    void Render_DockSpace();
    void Build_DefaultDockLayout(ImGuiID dockSpaceId, ImVec2 dockSpaceSize);
    void Build_SceneFocusDockLayout(ImGuiID dockSpaceId, ImVec2 dockSpaceSize);
    void Handle_Shortcuts();
    void Render_ShortcutHelp();

    // 현재 effect authoring document를 소유한 Emitter view를 찾는다.
    Shared<Emitter_View> Find_EffectAuthoringView() const;
    // Save 메뉴/단축키/툴바의 활성화 가능 여부를 한 곳에서 판단한다.
    bool CanSave_EffectDocument() const;
    // Save 메뉴/단축키/툴바가 공유하는 현재 effect document 저장 진입점이다.
    bool Save_EffectDocument();
    // Open 메뉴/단축키/툴바의 활성화 가능 여부를 한 곳에서 판단한다.
    bool CanOpen_EffectDocument() const;
    // Open 메뉴/단축키/툴바가 공유하는 현재 effect document 열기 진입점이다.
    bool Open_EffectDocument();
    // Add 툴바의 활성화 가능 여부를 한 곳에서 판단한다.
    bool CanAdd_EffectEmitters() const;
    // Add 툴바가 공유하는 외부 effect emitter 추가 진입점이다.
    bool Add_EffectEmitters();
    void Toggle_SceneFocusMode();
    void Enter_SceneFocusMode();
    void Exit_SceneFocusMode();

    void Toggle_Window(const wstring& windowName);
    void Toggle_WindowPair(const wstring& firstWindowName, const wstring& secondWindowName);

    void Render_ExitButton(float toolbarHeight);
    void Request_Exit();

public:
    static Unique<EffectEditor_Manager> Create();
    void Free() override;
};

NS_END
