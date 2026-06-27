#include "RuntimeRuleStateService.h"

#include "Battle_Enum.h"
#include "ClientInstance.h"
#include "CostumeManager.h"
#include "PlayerInventory.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "SkillTreeCom.h"
#include "Unlock_Manager.h"
#include <cctype>

NS_BEGIN(Client)

namespace
{
    constexpr const char* kRuleMimeCostume = "kill_mime_unlock_baguette_costume";
    constexpr const char* kRulePotatoCostume = "kill_potatobag_unlock_potato_costume";
    constexpr const char* kRuleFirstTutorialUnlockParty = "kill_tw_lancelier_unlock_party";

    struct WeaponUnlockDesc
    {
        const char* characterId = "";
        const char* weaponId = "";
    };

    struct FinalSkillSetupDesc
    {
        HeroId heroId = HeroId::END;
        vector<const char*> skillKeys{};
    };

    string ToLowerAscii(string value)
    {
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    }

    bool Is_DefaultAttackSkill(const string& skillKey)
    {
        return skillKey == "Gustave_Default_Attack" ||
            skillKey == "Lune_Default_Attack" ||
            skillKey == "Maelle_Default_Attack";
    }

    bool Is_HeroSkill(HeroId heroId, const Skill_Data& skill)
    {
        if (skill.characterId.empty())
            return false;

        const string heroName = ToLowerAscii(string(magic_enum::enum_name(heroId)));
        const string skillCharacterId = ToLowerAscii(skill.characterId);
        return heroName == skillCharacterId;
    }

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

    json Build_AllWeaponInventoryJson()
    {
        json root = json::object();
        root["owned_weapons"] = json::array();

        const string weaponCharacterIds[] = { "Gustave", "Lune", "Maelle" };
        for (const string& characterId : weaponCharacterIds)
        {
            const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(characterId);
            if (weapons == nullptr)
                continue;

            for (const GameDataTable::WeaponDesc& weapon : *weapons)
            {
                json entry = json::object();
                entry["character_id"] = weapon.characterId;
                entry["weapon_id"] = weapon.weaponId;
                entry["enhance_level"] = weapon.level;
                entry["enhance_exp"] = 0;
                root["owned_weapons"].emplace_back(std::move(entry));
            }
        }

        return root;
    }

    void Add_WeaponInventoryEntry(json& ownedWeapons, const char* characterId, const char* weaponId)
    {
        if (characterId == nullptr || weaponId == nullptr)
            return;

        const GameDataTable::WeaponDesc* weapon = CLIENT->Get_WeaponDesc(characterId, weaponId);
        if (weapon == nullptr)
            return;

        json entry = json::object();
        entry["character_id"] = weapon->characterId;
        entry["weapon_id"] = weapon->weaponId;
        entry["enhance_level"] = weapon->level;
        entry["enhance_exp"] = 0;
        ownedWeapons.emplace_back(std::move(entry));
    }

    json Build_SelectedWeaponInventoryJson(const vector<WeaponUnlockDesc>& weapons)
    {
        json root = json::object();
        root["owned_weapons"] = json::array();

        for (const WeaponUnlockDesc& weapon : weapons)
        {
            Add_WeaponInventoryEntry(root["owned_weapons"], weapon.characterId, weapon.weaponId);
        }

        return root;
    }

    json Build_AllWeaponAndPictosInventoryJson()
    {
        json root = Build_AllWeaponInventoryJson();
        root["unlocked_pictos"] = CLIENT->Get_AllPictosIds();
        return root;
    }

    json Build_AllHeroSkillTreeSaveJson(HeroId heroId)
    {
        json root = json::object();
        root["unlocked_skill_ids"] = json::array();

        for (const string& skillId : CLIENT->Get_AllSkillIds())
        {
            if (Is_DefaultAttackSkill(skillId))
                continue;

            const Skill_Data* skill = CLIENT->Get_Skill(skillId);
            if (skill == nullptr || !Is_HeroSkill(heroId, *skill))
                continue;

            root["unlocked_skill_ids"].emplace_back(skillId);
        }

        return root;
    }

    RuntimeCheckpointIO::HeroState Build_FinalHeroState(const FinalSkillSetupDesc& setup)
    {
        RuntimeCheckpointIO::HeroState hero{};
        hero.heroId = setup.heroId;

        vector<string> skillKeys{};
        skillKeys.reserve(setup.skillKeys.size());
        for (const char* skillKey : setup.skillKeys)
        {
            if (skillKey != nullptr && skillKey[0] != '\0')
                skillKeys.emplace_back(skillKey);
        }

        hero.statJson = json::object();
        hero.statJson["_skillKeys"] = skillKeys;
        hero.skillTreeJson = Build_AllHeroSkillTreeSaveJson(setup.heroId);

        return hero;
    }

    vector<RuntimeCheckpointIO::HeroState> Build_FinalHeroStates()
    {
        const FinalSkillSetupDesc setups[] =
        {
            {
                HeroId::Gustave,
                { "LumiereAssault", "UnleashedCharge", "MarkingShot", "Shatter", "PerfectBreak", "AngelsEye" }
            },
            {
                HeroId::Lune,
                { "Immolation", "IceGust", "Thunderfall", "TerraQuake", "CripplingTsunami", "SkyBreak" }
            },
            {
                HeroId::Maelle,
                { "Spark", "Percee", "FleureFury", "SwordBallet", "Stendhal", "Pyrolyse" }
            },
        };

        vector<RuntimeCheckpointIO::HeroState> heroes{};
        heroes.reserve(sizeof(setups) / sizeof(setups[0]));
        for (const FinalSkillSetupDesc& setup : setups)
        {
            heroes.emplace_back(Build_FinalHeroState(setup));
        }

        return heroes;
    }

    bool Apply_WeaponInventoryJson(const json& data)
    {
        if (!data.is_object())
            return false;

        const auto weapons = data.find("owned_weapons");
        if (weapons == data.end() || !weapons->is_array())
            return false;

        Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
        if (inventory == nullptr)
            return false;

        Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager();

        vector<PlayerInventory::WeaponInventoryEntry> entries{};
        for (const json& weapon : *weapons)
        {
            if (!weapon.is_object())
                continue;

            PlayerInventory::WeaponInventoryEntry entry{};
            entry.characterId = weapon.value("character_id", string{});
            entry.weaponId = weapon.value("weapon_id", string{});
            entry.enhanceLevel = weapon.value("enhance_level", 1u);
            entry.enhanceExp = weapon.value("enhance_exp", 0u);

            if (entry.characterId.empty() || entry.weaponId.empty())
                continue;

            const bool wasUnlocked = inventory->Is_WeaponUnlocked(entry.characterId, entry.weaponId);
            entries.emplace_back(std::move(entry));

            if (!wasUnlocked && unlockManager != nullptr)
                unlockManager->Queue_GetWeaponPresentation(entries.back().characterId, entries.back().weaponId);
        }

        inventory->Ensure_DefaultUnlockedWeapons(entries);
        return true;
    }

    bool Apply_PictosInventoryJson(const json& data)
    {
        if (!data.is_object())
            return false;

        const auto pictosIds = data.find("unlocked_pictos");
        if (pictosIds == data.end() || !pictosIds->is_array())
            return false;

        Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
        if (inventory == nullptr)
            return false;

        vector<string> ids{};
        for (const json& pictosId : *pictosIds)
        {
            if (pictosId.is_string())
                ids.emplace_back(pictosId.get<string>());
        }

        inventory->Ensure_DefaultUnlockedPictos(ids);
        return true;
    }

    bool Apply_HeroRuleStates(const vector<RuntimeCheckpointIO::HeroState>& heroes)
    {
        bool applied = false;

        for (const RuntimeCheckpointIO::HeroState& heroState : heroes)
        {
            if (heroState.heroId == HeroId::END)
                continue;

            Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroState.heroId);
            if (hero == nullptr)
                continue;

            if (Shared<PlayerStatCom> stat = hero->Get_Component<PlayerStatCom>())
            {
                stat->Apply_SaveJson(heroState.statJson);
                applied = true;
            }

            if (Shared<SkillTreeCom> skillTree = hero->Get_Component<SkillTreeCom>())
            {
                skillTree->Apply_SaveJson(heroState.skillTreeJson);
                applied = true;
            }
        }

        return applied;
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

RuntimeRuleStateIO::RuleState RuntimeRuleStateService::Build_BaguetteWeaponUnlocks()
{
    RuntimeRuleStateIO::RuleState ruleState{};
    ruleState.inventoryJson = Build_SelectedWeaponInventoryJson({
        { "Gustave", "Baguette" },
        { "Lune", "Baguette" },
        { "Maelle", "Baguette" },
    });
    return ruleState;
}

RuntimeRuleStateIO::RuleState RuntimeRuleStateService::Build_TiresoSnowimPlenumWeaponUnlocks()
{
    RuntimeRuleStateIO::RuleState ruleState{};
    ruleState.inventoryJson = Build_SelectedWeaponInventoryJson({
        { "Gustave", "Tireso" },
        { "Lune", "Snowim" },
        { "Maelle", "Plenum" },
    });
    return ruleState;
}

RuntimeRuleStateIO::RuleState RuntimeRuleStateService::Build_AllWeaponUnlocks()
{
    RuntimeRuleStateIO::RuleState ruleState{};
    ruleState.inventoryJson = Build_AllWeaponInventoryJson();
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

RuntimeRuleStateIO::RuleState RuntimeRuleStateService::Build_FinalSkillSetupUnlocks()
{
    RuntimeRuleStateIO::RuleState ruleState{};
    ruleState.unlockManagerJson = Build_HeroUnlockSaveJson(
        { kRuleFirstTutorialUnlockParty },
        { HeroId::Gustave, HeroId::Lune, HeroId::Maelle });
    ruleState.inventoryJson = Build_AllWeaponAndPictosInventoryJson();
    ruleState.heroes = Build_FinalHeroStates();
    return ruleState;
}

bool RuntimeRuleStateService::Apply_ToCurrentRuntime(const RuntimeRuleStateIO::RuleState& ruleState)
{
    bool applied = false;

    if (ruleState.costumeJson.is_object() &&
        (ruleState.costumeJson.contains("current") || ruleState.costumeJson.contains("unlocked")))
    {
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
    }

    if (ruleState.unlockManagerJson.is_object() &&
        (ruleState.unlockManagerJson.contains("completed_rule_ids") || ruleState.unlockManagerJson.contains("unlocked_hero_ids")))
    {
        if (Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
        {
            unlockManager->Apply_SaveJson(ruleState.unlockManagerJson, false);
            applied = true;
        }
    }

    if (Apply_WeaponInventoryJson(ruleState.inventoryJson))
        applied = true;

    if (Apply_PictosInventoryJson(ruleState.inventoryJson))
        applied = true;

    if (Apply_HeroRuleStates(ruleState.heroes))
        applied = true;

    return applied;
}

NS_END
