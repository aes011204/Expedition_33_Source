#pragma once

#include "Battle_Enum.h"

NS_BEGIN(Client)

namespace GradientSkill
{
    inline constexpr uint32 NormalSkillSlotCount = 5;
    inline constexpr uint32 SlotIndex = 5;

    const char* Resolve_SkillKey(HeroId heroId);
    bool        Is_GradientSkill(HeroId heroId, const string& skillKey);

    inline constexpr uint32 MaxGaugeStacks = 3;
    inline constexpr uint32 GaugeGainPerTurn = 1;
    inline constexpr uint32 GaugeCostPerUse = 1;
}

NS_END
