#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_PlayerHpBar : public UI_RuntimePanelBase
{
public:
    UI_PlayerHpBar() = default;
    ~UI_PlayerHpBar() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Show_DamagedTarget(const BattleChar& target, float damage, float duration);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Set_Hp(float curHp, float maxHp);
    void    Apply_HpGauge(float ratio);                         
    void    Apply_DelayedHpGauge(float ratio);                  
    void    Update_DelayedHpGauge(float timeDelta, float targetRatio);

    bool    Try_GetDisplayTarget(BattleChar& outTarget) const;
    bool    Is_ValidPlayerTarget(const BattleChar& target) const;

private:
    Shared<UI_Image> _hpGauge{};   
    Shared<UI_Image> _hpGaugeBG{}; 

    BattleChar _displayTarget{};
    bool       _hasDisplayTarget = false;

    float      _displayElapsed = 0.f;
    float      _displayDuration = 0.f;

    float      _hpRatio = 1.f;        
    float      _delayedHpRatio = 1.f; 
    static constexpr float kDelayedHpLerpSpeed = 3.5f;

public:
    static Unique<UI_PlayerHpBar> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
