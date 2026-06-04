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

    void    Sync_BossState(const BattleChar& target);
    void    Apply_HpGauge(float ratio);
    void    Apply_DelayedHpGauge(float ratio);
    void    Apply_ShieldGauge(const BattleChar& target);
    void    Update_DelayHpGauge(float timeDelta, float targetRatio);

    bool    Is_ValidBossTarget(const BattleChar& target) const;

private:
    Shared<UI_Image> _hpGauge{};
    Shared<UI_Image> _hpGaugeBG{};
    Shared<UI_Image> _shieldGauge{};
    Shared<UI_Image> _breakGauge{};
    Shared<UI_Text>  _bossNameText{};
    Shared<UI_Text>  _bossNameBgText{};

    BattleChar _displayTarget{};
    bool       _hasDisplayTarget = false;

    float      _hpRatio = 1.f;
    float      _delayedHpRatio = 1.f;

    static constexpr float kDelayedHpLerpSpeed = 3.5f;

public:
    static Unique<UI_BossHpBar> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
