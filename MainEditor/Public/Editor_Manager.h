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

    enum class RuleStatePreset
    {
        GameStart,
        MimeCostumeOnly,
        PotatoCostumeOnly,
        AllCostumes,
        BaguetteWeapons,
        TiresoSnowimPlenumWeapons,
        AllWeapons,
        FirstTutorialHeroUnlocks,
        BishopStartUnlocks,
        FinalSkillSetupUnlocks,
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
    void Render_CheckpointButtons();
    void Render_RuleStateButtons();
    void Render_SaveCheckpointPopup();
    void Open_SaveCheckpointPopup();
    bool Save_CheckpointToPath(const fs::path& checkpointPath);
    bool Save_RuleStatePreset(RuleStatePreset preset);
    fs::path Resolve_DefaultCheckpointPath() const;
    fs::path Resolve_CheckpointPathFromName(const string& checkpointName) const;
    fs::path Resolve_DefaultRuleStatePath() const;
    fs::path Resolve_RuleStatePath(RuleStatePreset preset) const;
    bool Pick_CheckpointFile(fs::path& outPath) const;
    bool Pick_RuleStateFile(fs::path& outPath) const;
    bool Launch_GameLauncher(const fs::path* checkpointPath = nullptr);
    void Render_ReloadDataButton();
    void Notify_ReloadDataResult(const char* dataName, HRESULT hr, const char* failureMessage) const;
    void Render_SoundCheck();
    void Render_AutoParryCheck();

private:
    /* 창 이름을 키로 보관하는 에디터 뷰 목록. */
    map<wstring, Shared<Editor_Window>> _windows = {}; 
    map<wstring, bool> _focusModeWindowStates = {};
    bool _isFocusMode = false;
    LauncherConfig _launcherConfig = LauncherConfig::Debug;
    fs::path _selectedCheckpointPath = {};
    fs::path _selectedRuleStatePath = {};
    bool _openSaveCheckpointPopup = false;
    char _checkpointSaveNameBuffer[128] = "Demo";
    float _soundVolumePercent = 100.f;
    bool _soundVolumePercentInitialized = false;

public:
	static Unique<Editor_Manager> Create();
};

NS_END
