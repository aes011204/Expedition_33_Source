#pragma once

#include "Client_Enum.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
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

        uint32 visualSlotIndex = UINT_MAX;
        string portraitGuid{};
        wstring displayName{};
    };

    struct VictorySummaryDesc
    {
        uint32 rewardExp = 0;
        uint32 rewardChroma = 0;
        float elapsedSec = 0.f;
        uint32 maxDamage = 0;
        uint32 totalDamageDealt = 0;
        uint32 totalDamageTaken = 0;
        uint32 parrySuccessCount = 0;
        uint32 dodgeSuccessCount = 0;
        vector<string> defeatedMonsterPortraitGuids{};
    };

    struct PlayerSlot
    {
        Shared<UI_Base>  root{};
        Shared<UI_Image> portrait{};
        Shared<UI_Text>  nameText{};
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

private:
    struct RewardNumberTween
    {
        Shared<UI_Text> text{};
        Shared<UI_Text> bgText{};
        uint32 startValue = 0;
        uint32 targetValue = 0;
        float elapsed = 0.f;
        float duration = 0.8f;
        bool isAnimating = false;
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
    void    Set_VictorySummary(const VictorySummaryDesc& summary);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Play_TargetAnimation(const wchar_t* animationName);
    void    Stop_TargetAnimations();

    void    Apply_PlayerStat(const VictoryPlayerStatDesc& stat);
    void    Apply_VictorySummary(const VictorySummaryDesc& summary);
    void    Apply_DefeatedMonsterPortraits(const vector<string>& portraitGuids);
    bool    Ensure_DefeatedMonsterPortraitSlot(uint32 slotIndex);
    static uint32 Get_SlotIndex(PlayerCharacterType playerType);
    static void   Set_UIntText(const Shared<UI_Text>& text, uint32 value);
    static void   Set_WStringText(const Shared<UI_Text>& text, const wstring& value);
    static wstring Format_ClearTime(float elapsedSec);

    void Update_ExpGauge(float timeDelta);
    void Start_RewardNumberTween(RewardNumberTween& tween, const Shared<UI_Text>& text, const Shared<UI_Text>& bgText, uint32 targetValue, float duration);
    void Update_RewardNumberTweens(float timeDelta);
    static void Update_RewardNumberTween(RewardNumberTween& tween, float timeDelta);

private:
    array<PlayerSlot, 3> _playerSlots{};
    Shared<UI_Text>      _rewardExpText{};
    Shared<UI_Text>      _rewardExpBgText{};
    Shared<UI_Text>      _rewardChromaText{};
    Shared<UI_Text>      _rewardChromaBgText{};
    Shared<UI_Text>      _maxDamageText{};
    Shared<UI_Text>      _totalDamageText{};
    Shared<UI_Text>      _takenDamageText{};
    Shared<UI_Text>      _clearTimeText{};
    Shared<UI_Text>      _parrySuccessText{};
    Shared<UI_Text>      _dodgeSuccessText{};
    Shared<UI_Base>      _defeatedMonsterPortraitPanel{};
    Shared<UI_Image>     _defeatedMonsterPortraitTemplate{};
    vector<Shared<UI_Image>> _defeatedMonsterPortraitSlots{};
    Vec2                 _defeatedMonsterPortraitBasePosition{};
    float                _introDuration = 0.f;
    float                _expAnimDelayRemaining = 0.f;
    bool                 _isClosingWithOutro = false;
    float                _closeElapsed = 0.f;
    float                _outroDuration = 0.f;
    RewardNumberTween    _rewardExpTween{};
    RewardNumberTween    _rewardChromaTween{};

public:
    static Unique<UI_BattleVictory> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
