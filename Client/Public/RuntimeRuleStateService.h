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

    static RuntimeRuleStateIO::RuleState Build_GameplayStartUnlocks();
    static RuntimeRuleStateIO::RuleState Build_GestralVillageStartUnlocks();
    static RuntimeRuleStateIO::RuleState Build_BishopStartUnlocks();
    static RuntimeRuleStateIO::RuleState Build_PotatoBagStartUnlocks();

    static RuntimeRuleStateIO::RuleState Build_SimonStartUnlocks();

    static RuntimeRuleStateIO::RuleState Build_FinalSkillSetupUnlocks();
    static bool Apply_ToCurrentRuntime(const RuntimeRuleStateIO::RuleState& ruleState, bool queuePresentation = true);
};

NS_END
