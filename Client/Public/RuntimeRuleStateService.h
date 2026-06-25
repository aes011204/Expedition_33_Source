#pragma once

#include "RuntimeRuleStateIO.h"

NS_BEGIN(Client)

class RuntimeRuleStateService final
{
public:
    RuntimeRuleStateService() = delete;

    static RuntimeRuleStateIO::RuleState Build_GameStart();
    static RuntimeRuleStateIO::RuleState Build_CostumeUnlocks(bool unlockBaguette, bool unlockPotato);
    static RuntimeRuleStateIO::RuleState Build_BaguetteWeaponUnlocks();
    static RuntimeRuleStateIO::RuleState Build_TiresoSnowimPlenumWeaponUnlocks();
    static RuntimeRuleStateIO::RuleState Build_AllWeaponUnlocks();
    static RuntimeRuleStateIO::RuleState Build_FirstTutorialHeroUnlocks();
    static bool Apply_ToCurrentRuntime(const RuntimeRuleStateIO::RuleState& ruleState);
};

NS_END
