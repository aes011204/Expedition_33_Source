#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_MonsterHpBar : public UI_RuntimePanelBase
{
public:
    UI_MonsterHpBar() = default;
    ~UI_MonsterHpBar() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Set_TargetWorldPosition(const Vec3& worldPos);
    void    Set_Hp(float curHp, float maxHp);

    void    Show_TrackedTarget(const BattleChar& target);
    void    Clear_TrackedTarget();

    void    Show_TimedTarget(const BattleChar& target, float duration);
    void    Show_DamagedTarget(const BattleChar& target, float damage, float duration);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_HpGauge(float ratio);
    void    Apply_DelayedHpGauge(float ratio);

    void    Sync_DelayedHpGauge(float ratio);
    void    Update_DelayHpGauge(float timeDelta, float targetRatio);

    bool    Try_GetDisplayTarget(BattleChar& outTarget) const;
    bool    Is_ValidMonsterTarget(const BattleChar& target) const;

private:
    enum class DisplayMode
    {
        None,
        Tracked,
        Timed,
    };

private:
    Shared<UI_Image> _hpGauge{};
    Shared<UI_Image> _hpGaugeBG{};

    Vec3        _targetWorldPos = Vec3::Zero;
    bool        _hasTargetWorldPos = false;

    BattleChar  _displayTarget{};
    DisplayMode _displayMode = DisplayMode::None;

    float       _displayElapsed = 0.f;
    float       _displayDuration = 0.f;

    float       _hpRatio = 1.f;
    float       _delayedHpRatio = 1.f;

    static constexpr float kDelayedHpLerpSpeed = 3.5f;

public:
    static Unique<UI_MonsterHpBar> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
