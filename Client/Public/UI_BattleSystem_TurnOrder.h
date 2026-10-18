#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_BattleSystem_TurnOrder final : public UI_RuntimePanelBase
{
public:
    struct TurnOrderEntryDesc
    {
        string  portraitGuid{};
        bool    isEnemy = false;
        uint64  actorToken = 0;
    };

    struct TurnOrderSlot
    {
        Shared<UI_Base>  anchor{};
        Shared<UI_Base>  currentPlayerOverlay{};
        Shared<UI_Base>  currentMonsterOverlay{};
        Shared<UI_Base>  playerRoot{};
        Shared<UI_Base>  monsterRoot{};
        Shared<UI_Image> playerPortrait{};
        Shared<UI_Image> monsterPortrait{};
        Shared<UI_Image> currentPlayerPortrait{};
        Shared<UI_Image> currentMonsterPortrait{};
        string           lastPortraitGuid{};
        bool             lastIsEnemy = false;
        bool             hasLastEntry = false;
        uint64           lastActorToken = 0;
    };

public:
    UI_BattleSystem_TurnOrder() = default;
    ~UI_BattleSystem_TurnOrder() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Set_TurnOrderPreview(const vector<TurnOrderEntryDesc>& entries);
    void    Clear_TurnOrderPreview();
    void    Notify_TurnStarted();
    void    Set_AnimationPlaybackEnabled(bool enabled);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    HRESULT Load_UIFragment(const wchar_t* jsonPath, const wstring& prototypeTagPrefix, umap<wstring, Shared<UI_Base>>& outUIByName) const;
    void    Merge_UIMap(const umap<wstring, Shared<UI_Base>>& src);
    void    Place_OnSlot(const Shared<UI_Base>& ui, const Shared<UI_Base>& parent, bool keepOriginalSize) const;
    void    Open_UI(const Shared<UI_Base>& ui) const;
    void    Close_UI(const Shared<UI_Base>& ui) const;
    void    Play_TargetAnimation(const wchar_t* animationName);
    void    Stop_TargetAnimations();
    void    Play_SlotPortraitIntro(TurnOrderSlot& slot, bool isEnemy);
    void    Play_CurrentTurnPulse(bool isEnemy);

private:
    // 턴 슬롯은 최대 8개
    array<TurnOrderSlot, 8> _slots {};
    Shared<UI_Base> _currentTurnAnchor{};
    Shared<UI_Base> _preTurnCurrentFrameRoot{};
    bool _isAnimationPlaybackEnabled = false;
    bool _isFrameIntroPending = false;

public:
    static Unique<UI_BattleSystem_TurnOrder> Create(uint32 levelIndex);
    void Free() override;

};

NS_END
