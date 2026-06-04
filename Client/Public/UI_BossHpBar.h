#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BossHpBar final : public UI_RuntimePanelBase
{
public:
    UI_BossHpBar() = default;
    ~UI_BossHpBar() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

    void    Show_Boss(const BattleChar& target);
    void    Clear_Boss();
    void    Show_DamagedBoss(const BattleChar& target, float damage);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    HRESULT Load_ShieldIcons(uint32 levelIndex);

    void    Sync_BossState(const BattleChar& target);
    void    Apply_HpGauge(float ratio);
    void    Apply_DelayedHpGauge(float ratio);
    void    Apply_ShieldGauge(const BattleChar& target);
    void    Apply_ShieldIcons(uint32 currentShield);
    void    Reset_ShieldIconDefaultVisual(uint32 iconIndex);
    void    Set_ShieldIconPosition(uint32 iconIndex, uint32 visibleOrder, uint32 visibleCount);
    Vec2    Resolve_ShieldIconScreenCenter() const;
    void    Play_BrokenShieldIcon(uint32 iconIndex);
    bool    Has_ActiveBrokenShieldAnimation() const;
    void    Update_ShieldIconAnimations(float timeDelta);
    void    Close_ShieldIcons();
    void    Update_DelayHpGauge(float timeDelta, float targetRatio);

    uint32  Resolve_MaxShield(const BattleChar& target) const;
    bool    Is_ValidBossTarget(const BattleChar& target) const;

private:
    Shared<UI_Image>            _hpGauge{};
    Shared<UI_Image>            _hpGaugeBG{};
    Shared<UI_Image>            _shieldGauge{};
    Shared<UI_Image>            _breakGauge{};
    Shared<UI_Text>             _bossNameText{};
    Shared<UI_Text>             _bossNameBgText{};
    array<Shared<UI_Base>, 3>    _shieldIcons{};
    array<float, 3>              _shieldIconCloseTimers{};

    BattleChar _displayTarget{};
    bool       _hasDisplayTarget = false;

    float      _hpRatio = 1.f;
    float      _delayedHpRatio = 1.f;
    uint32     _maxShield = 0;
    uint32     _lastShieldIconCount = 0;
    bool       _hasShieldIconState = false;

    static constexpr float kDelayedHpLerpSpeed = 3.5f;
    static constexpr float kBrokenShieldAnimationDurationSec = 1.f;
    static constexpr uint32 kMaxBossShieldIconCount = 3;

public:
    static Unique<UI_BossHpBar> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
