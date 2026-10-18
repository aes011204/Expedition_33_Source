#pragma once

#include "UI_RuntimePanelBase.h"
#include "UI_Animation.h"

NS_BEGIN(Client)

class UI_AttackTooltip final : public UI_RuntimePanelBase
{
public:
    UI_AttackTooltip() = default;
    ~UI_AttackTooltip() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;
    void    Close_Immediately(); // 전투 종료/F4/Free 처럼 잔상 없이 바로 끌 때

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Load_TooltipAnimations();

    void    Finish_Close();

private:
    UI_KeyframeAnimationAsset _tooltipIntroAnimation{};
    UI_KeyframeAnimationAsset _tooltipOutroAnimation{};

    bool  _hasTooltipIntroAnimation = false;
    bool  _hasTooltipOutroAnimation = false;

    bool  _isClosingWithOutro = false;
    float _closeElapsed = 0.f;
    float _outroDuration = 0.f;

public:
    static Unique<UI_AttackTooltip> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
