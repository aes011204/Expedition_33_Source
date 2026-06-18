#pragma once

#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Editor_Manager final
{
public:
    enum class LauncherConfig
    {
        Debug,
        Release
    };

public:
    Editor_Manager();
	~Editor_Manager() = default;

public:
	HRESULT Initialize();
	void    Update(float timeDelta);
	void    Render();

    // View 추가
	void Add_Window(const Shared<Editor_Window>& window);
	Shared<Editor_Window> Find_Window(const wstring& windowName) const;

private:
    void Render_MainMenuBar();
    void Render_RuntimeToolbar();
	void Render_DockSpace();
    void Handle_Shortcuts();    // 단축키
    void Render_ShortcutHelp(); // 단축키 정리

    void Toggle_Window(const wstring& windowName);
    void Toggle_WindowPair(const wstring& firstWindowName, const wstring& secondWindowName);
    void Toggle_FocusMode();

    void Render_ExitButton(float toolbarHeight);
    void Request_Exit();

    // GameLauncher 실행
    void Render_LauncherButton();
    bool Launch_GameLauncher();
    void Render_ReloadDataButton();
    void Render_SoundCheck();

private:
    /* 창 이름을 키로 보관하는 에디터 뷰 목록. */
    map<wstring, Shared<Editor_Window>> _windows = {}; 
    map<wstring, bool> _focusModeWindowStates = {};
    bool _isFocusMode = false;
    LauncherConfig _launcherConfig = LauncherConfig::Debug;

public:
	static Unique<Editor_Manager> Create();
};

NS_END
