#pragma once

#include "Client_Enum.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Text;
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_BattleVictory final : public UI_RuntimePanelBase
{
public:
    struct VictoryPlayerStatDesc
    {
        PlayerCharacterType playerType = PlayerCharacterType::None;
        uint32 level = 1;
        uint32 curExp = 0;
        uint32 maxExp = 0;

        uint32 prevLevel = 1;
        uint32 prevExp = 0;
        uint32 prevMaxExp = 0;

        // TODO : 전투 전리품에 아이템 + 돈
    };

    struct PlayerSlot
    {
        Shared<UI_Text>  curExpText{};
        Shared<UI_Text>  maxExpText{};
        Shared<UI_Text>  levelText{};
        Shared<UI_Image> expGauge{};

        uint32 startLevel = 1;
        uint32 targetLevel = 1;

        float startExp = 0.f;
        float displayExp = 0.f;
        float targetExp = 0.f;
        float startMaxExp = 1.f;
        float targetMaxExp = 1.f;
        float maxExp = 1.f;

        float animElapsed = 0.f;
        float animDuration = 0.8f;
        bool  isAnimating = false;
    };

public:
    UI_BattleVictory() = default;
    ~UI_BattleVictory() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;
    void    Close_Immediately();

    bool    Is_ClosingWithOutro() const { return _isClosingWithOutro; }

    void    Set_PlayerStatus(const vector<VictoryPlayerStatDesc>& stats);
    void    Set_RewardExp(uint32 rewardExp);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Play_TargetAnimation(const wchar_t* animationName);
    void    Stop_TargetAnimations();

    void    Apply_PlayerStat(const VictoryPlayerStatDesc& stat);
    static uint32 Get_SlotIndex(PlayerCharacterType playerType);
    static void   Set_UIntText(const Shared<UI_Text>& text, uint32 value);

    void Update_ExpGauge(float timeDelta);

private:
    array<PlayerSlot, 3> _playerSlots{};
    Shared<UI_Text>      _rewardExpText{};
    Shared<UI_Text>      _rewardExpBgText{};
    float                _introDuration = 0.f;
    float                _expAnimDelayRemaining = 0.f;
    bool                 _isClosingWithOutro = false;
    float                _closeElapsed = 0.f;
    float                _outroDuration = 0.f;

public:
    static Unique<UI_BattleVictory> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
