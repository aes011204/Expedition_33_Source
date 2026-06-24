#include "RuntimeRuleStateService.h"

#include "ClientInstance.h"
#include "CostumeManager.h"
#include "Player_WorldBase.h"
#include "Unlock_Manager.h"

NS_BEGIN(Client)

namespace
{
    constexpr const char* kRuleMimeCostume = "kill_mime_unlock_baguette_costume";
    constexpr const char* kRulePotatoCostume = "kill_potatobag_unlock_potato_costume";
    constexpr const char* kRuleFirstTutorialUnlockParty = "kill_tw_lancelier_unlock_party";

    json Build_CostumeSaveJson(bool unlockBaguette, bool unlockPotato)
    {
        json root = json::object();
        root["current"] = json::array({
            json{{"hero_id", "Gustave"}, {"preset_id", "Default"}},
            json{{"hero_id", "Lune"}, {"preset_id", "Default"}},
            json{{"hero_id", "Maelle"}, {"preset_id", "Default"}},
        });

        root["unlocked"] = json::object();

        const auto BuildUnlockedPresets = [unlockBaguette, unlockPotato]()
        {
            json presets = json::array({ "Default" });

            if (unlockBaguette)
                presets.emplace_back("Baguette");

            if (unlockPotato)
                presets.emplace_back("Potato");

            return presets;
        };

        root["unlocked"]["Gustave"] = BuildUnlockedPresets();
        root["unlocked"]["Lune"] = BuildUnlockedPresets();
        root["unlocked"]["Maelle"] = BuildUnlockedPresets();

        return root;
    }

    json Build_UnlockManagerSaveJson(bool unlockBaguette, bool unlockPotato)
    {
        json root = json::object();
        root["completed_rule_ids"] = json::array();

        if (unlockBaguette)
            root["completed_rule_ids"].emplace_back(kRuleMimeCostume);

        if (unlockPotato)
            root["completed_rule_ids"].emplace_back(kRulePotatoCostume);

        return root;
    }

    json Build_HeroUnlockSaveJson(
        const vector<string>& completedRuleIds,
        const vector<HeroId>& unlockedHeroIds)
    {
        json root = json::object();
        root["completed_rule_ids"] = json::array();
        root["unlocked_hero_ids"] = json::array();

        for (const string& ruleId : completedRuleIds)
        {
            root["completed_rule_ids"].emplace_back(ruleId);
        }

        for (HeroId heroId : unlockedHeroIds)
        {
            root["unlocked_hero_ids"].emplace_back(static_cast<int>(heroId));
        }

        return root;
    }
}

RuntimeRuleStateIO::RuleState RuntimeRuleStateService::Build_GameStart()
{
    return Build_CostumeUnlocks(false, false);
}

RuntimeRuleStateIO::RuleState RuntimeRuleStateService::Build_CostumeUnlocks(bool unlockBaguette, bool unlockPotato)
{
    RuntimeRuleStateIO::RuleState ruleState{};
    ruleState.costumeJson = Build_CostumeSaveJson(unlockBaguette, unlockPotato);
    ruleState.unlockManagerJson = Build_UnlockManagerSaveJson(unlockBaguette, unlockPotato);
    return ruleState;
}

RuntimeRuleStateIO::RuleState RuntimeRuleStateService::Build_FirstTutorialHeroUnlocks()
{
    RuntimeRuleStateIO::RuleState ruleState{};
    ruleState.costumeJson = Build_CostumeSaveJson(false, false);
    ruleState.unlockManagerJson = Build_HeroUnlockSaveJson(
        { kRuleFirstTutorialUnlockParty },
        { HeroId::Gustave, HeroId::Lune, HeroId::Maelle });
    return ruleState;
}

bool RuntimeRuleStateService::Apply_ToCurrentRuntime(const RuntimeRuleStateIO::RuleState& ruleState)
{
    bool applied = false;

    if (CostumeManager* costumeManager = CLIENT->Get_CostumeManager())
    {
        costumeManager->Apply_CurrentCostumeSaveJson(ruleState.costumeJson);

        for (const HeroPartyEntry& entry : CLIENT->Get_HeroEntries())
        {
            if (entry.hero == nullptr)
                continue;

            if (const CostumePreset* preset = costumeManager->Get_CurrentCostume(entry.heroId))
                entry.hero->Apply_CostumePreset(*preset);
        }

        applied = true;
    }

    if (Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
    {
        unlockManager->Apply_SaveJson(ruleState.unlockManagerJson, false);
        applied = true;
    }

    return applied;
}

NS_END
