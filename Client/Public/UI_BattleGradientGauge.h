#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"
#include "GradientSkill.h"

NS_BEGIN(Engine)
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BattleGradientGauge final : public UI_RuntimePanelBase
{
public:
    UI_BattleGradientGauge() = default;
    ~UI_BattleGradientGauge() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Update_ForActor(const BattleChar& actor);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Set_Stack(uint32 stack, uint32 progressTick);
    void    Set_Text(const Shared<UI_Text>& text, const wstring& value);

private:
    array<Shared<UI_Image>, GradientSkill::MaxGaugeStacks> _stackImages{};
    array<float, GradientSkill::MaxGaugeStacks> _visualFillRatios{};
    array<float, GradientSkill::MaxGaugeStacks> _targetFillRatios{};
    Shared<UI_Text> _currentCountText{};
    Shared<UI_Text> _currentCountBgText{};
    Shared<UI_Text> _maxCountText{};
    Shared<UI_Text> _maxCountBgText{};

public:
    static Unique<UI_BattleGradientGauge> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
