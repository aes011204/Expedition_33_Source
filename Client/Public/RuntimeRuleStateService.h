#pragma once

#include "RuntimeRuleStateIO.h"

NS_BEGIN(Client)

class RuntimeRuleStateService final
{
public:
    RuntimeRuleStateService() = delete;

    static RuntimeRuleStateIO::RuleState Build_GameStart();
    static RuntimeRuleStateIO::RuleState Build_CostumeUnlocks(bool unlockBaguette, bool unlockPotato);
    static bool Apply_ToCurrentRuntime(const RuntimeRuleStateIO::RuleState& ruleState);
};

NS_END
