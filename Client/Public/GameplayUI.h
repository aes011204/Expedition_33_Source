#pragma once

#include "Base.h"
#include "Delegate.h"
#include "GameplayUI_Types.h"

NS_BEGIN(Client)

class GameplayUI_World;
class GameplayUI_Battle;
class GamePlayUI_SL;
class IRuntimeUI_EditorTarget;

// 실제로 렌더링되는 UI 가 아니라, 플레이 중 UI 들을 로드/토글/바인딩 등 처리할 컨트롤러라고 생각
class GameplayUI : public Base
{
public:
    enum class GameplayUIInputMode
    {
        World,  // 월드 UI 입력만 허용
        Battle, // 배틀 UI 입력만 허용
        SoulLike,
    };

public:
    GameplayUI() = default;
    ~GameplayUI() override;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

public:
    void                Handle_EditorStopRuntime();

    vector<RuntimeUIEntry> Get_WorldRuntimeUIEntries() const;
    vector<RuntimeUIEntry> Get_BattleRuntimeUIEntries() const;
    vector<RuntimeUIEntry> Get_SoulLikeRuntimeUIEntries() const;
    const vector<RuntimeUIEntry>& Get_CachedWorldRuntimeUIEntries() const;
    const vector<RuntimeUIEntry>& Get_CachedBattleRuntimeUIEntries() const;
    const vector<RuntimeUIEntry>& Get_CachedSoulLikeRuntimeUIEntries() const;
    void Refresh_RuntimeUIEntryCache() const;
    void Invalidate_RuntimeUIEntryCache();

    IRuntimeUI_EditorTarget* Find_RuntimeUIEditorTarget(const Shared<Engine::UI_Base>& rootUI) const; // Battle UI 루트에서 오프셋 소유자를 찾는다.

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;

    bool    Is_AnyMenuOpen() const;
    GameplayMenu Get_CurrentMenu() const; // 현재 최상위 메뉴 확인용
    bool    Open_MerchantShop();
    bool    Open_WeaponUpgrade();
    bool    Open_ArenaCharacterSelect();
    bool    Open_SavePoint();
    bool    Open_Journal(JournalOpenAnimation openAnimation = JournalOpenAnimation::JournalUI);

    void    Queue_GetChromaPresentation(uint32 chromaAmount);
    void    Queue_LevelAnnounce(LevelType levelType);
    bool    Open_EventDialogueUI(const EventDialogueUIRequest& request);
    void    Close_EventDialogueUI();
    void    Set_CinematicUIHidden(bool hidden);
    bool    Is_CinematicUIHidden() const { return _isCinematicUIHidden; }

    GameplayUI_World*           Get_WorldUI() const { return _worldUI.get(); }
    Shared<GameplayUI_Battle>   Get_BattleUI() const { return _battleUI; }
    GamePlayUI_SL* Get_SoulLikeUI() const { return _soulLikeUI.get(); }

private:
    // 이번 프레임에 어떤 UI 입력인지. World ? Battle ?
    GameplayUIInputMode Resolve_InputMode() const;
    void                Handle_InputModeChanged(GameplayUIInputMode previousMode, GameplayUIInputMode currentMode);
    void                Rebuild_RuntimeUIEntryCache() const;
    void                Close_RuntimeUIForCinematic();

private:
    struct RuntimeUIEntryCache
    {
        vector<RuntimeUIEntry> worldEntries{};
        vector<RuntimeUIEntry> battleEntries{};
        vector<RuntimeUIEntry> soulLikeEntries{};
        umap<Engine::UI_Base*, IRuntimeUI_EditorTarget*> editorTargetsByRoot{};
        uint64 frameSerial = static_cast<uint64>(-1);
        bool dirty = true;
    };

private:
    Unique<GameplayUI_World>    _worldUI;  // 월드 UI는 GameplayUI 하나만 소유
    Shared<GameplayUI_Battle>   _battleUI; // 배틀 UI는 delegate 직접 구독 때문에 Shared로 둔다.
    Unique<GamePlayUI_SL>       _soulLikeUI;

    GameplayUIInputMode         _lastInputMode = GameplayUIInputMode::World;
    FDelegateHandle             _editorStopHandle{};
    uint64                      _runtimeUIEntryFrameSerial = 0;
    bool                        _isCinematicUIHidden = false;
    mutable RuntimeUIEntryCache _runtimeUIEntryCache{};

public:
    static Shared<GameplayUI> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
