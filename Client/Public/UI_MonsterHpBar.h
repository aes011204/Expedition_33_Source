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

    void    Set_FreeAimEmphasis(bool enabled);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_HpGauge(float ratio);
    void    Apply_DelayedHpGauge(float ratio);
    void    Apply_BreakGauge(const BattleChar& target);

    void    Sync_DelayedHpGauge(float ratio);
    void    Update_DelayHpGauge(float timeDelta, float targetRatio);

    bool    Try_GetHpBarWorldPosition(const BattleChar& target, Vec3& outWorldPos) const;
    bool    Try_GetDisplayTarget(BattleChar& outTarget) const;
    bool    Is_ValidMonsterTarget(const BattleChar& target) const;

private:
    void    Cache_RootBaseScale();
    void    Update_FreeAimScale(float timeDelta);

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
    Shared<UI_Image> _breakGauge{};

    Vec3        _targetWorldPos = Vec3::Zero;
    bool        _hasTargetWorldPos = false;

    BattleChar  _displayTarget{};
    DisplayMode _displayMode = DisplayMode::None;

    float       _displayElapsed = 0.f;
    float       _displayDuration = 0.f;

    float       _hpRatio = 1.f;
    float       _delayedHpRatio = 1.f;

    static constexpr float kDelayedHpLerpSpeed = 3.5f;

private:
    bool    _freeAimEmphasis = false;
    float   _freeAimScale = 1.f;
    Vec2    _rootBaseScale = Vec2(1.f, 1.f);
    bool    _hasRootBaseScale = false;

    static constexpr float kFreeAimMinScale = 1.f;
    static constexpr float kFreeAimMaxScale = 1.3f;
    static constexpr float kFreeAimScaleLerpSpeed = 8.f;
    static constexpr float kFreeAimScreenOffsetX = 35.f;
    static constexpr float kFreeAimScreenOffsetY = -130.f;
    static constexpr float kHpBarScaleX = 0.6f;

public:
    static Unique<UI_MonsterHpBar> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
