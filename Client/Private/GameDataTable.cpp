#include "GameDataTable.h"
#include "GameInstance.h"

#include <cctype>

namespace
{
    bool StartsWith(const string& value, const string& prefix)
    {
        return value.size() >= prefix.size() &&
            equal(prefix.begin(), prefix.end(), value.begin());
    }

    bool ReadBoolFlexible(const json& item, const char* key, bool defaultValue)
    {
        if (!item.contains(key) || item[key].is_null())
            return defaultValue;

        const json& value = item[key];

        if (value.is_boolean())
            return value.get<bool>();

        if (value.is_number_integer())
            return value.get<int>() != 0;

        if (value.is_string())
        {
            string text = value.get<string>();

            for (char& c : text)
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

            if (text == "true" || text == "1")
                return true;

            if (text == "false" || text == "0")
                return false;
        }

        LOG_WARN("[GameDataTable] invalid bool field: key={}, valueType={}", key, value.type_name());
        return defaultValue;
    }

    wstring Resolve_WeaponPortraitTexturePath(const string& portraitName)
    {
        if (portraitName.empty())
            return L"";

        string normalizedPath = portraitName;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        if (fs::path(String::ToWString(normalizedPath)).extension().empty())
            normalizedPath += ".png";


        const fs::path inputPath = fs::path(String::ToWString(normalizedPath));
        if (inputPath.is_absolute())
            return inputPath.lexically_normal().wstring();

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return fs::absolute(inputPath).lexically_normal().wstring();

        const string loweredPath = String::ToLowerCopy(normalizedPath);
        string resourceRelativePath = normalizedPath;

        constexpr const char* resourcePrefix = "../bin/resources/";
        constexpr const char* binResourcePrefix = "bin/resources/";
        constexpr const char* resourcesPrefix = "resources/";

        if (StartsWith(loweredPath, resourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcePrefix));
        else if (StartsWith(loweredPath, binResourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(binResourcePrefix));
        else if (StartsWith(loweredPath, resourcesPrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcesPrefix));

        return (fs::path(assetRoot) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    string Make_WeaponUpgradeKey(const string& characterId, const string& weaponId)
    {
        return characterId + ":" + weaponId;
    }
}

HRESULT GameDataTable::Initialize()
{
    return S_OK;
}

HRESULT GameDataTable::Load_All()
{
    Clear();

    const wstring jsonRootPath = Get_DefaultJsonRootPath();
    if (jsonRootPath.empty())
    {
        LOG_ERROR("[GameDataTable] json root path is empty");
        return E_FAIL;
    }

    CHECK_FAILED(Load_ExpLevel(jsonRootPath), E_FAIL);

    CHECK_FAILED(Load_SkillData(jsonRootPath), E_FAIL);
    CHECK_FAILED(Load_SkillPhaseData(jsonRootPath), E_FAIL);

    // CHECK_FAILED(Load_SkillActionData(jsonRootPath), E_FAIL);

    CHECK_FAILED(Load_SkillTreeData(jsonRootPath), E_FAIL);

    CHECK_FAILED(Load_SkillTreeRequirementData(jsonRootPath), E_FAIL);

    CHECK_FAILED(Load_Weapon(jsonRootPath), E_FAIL);
    CHECK_FAILED(Load_WeaponUpgrade(jsonRootPath), E_FAIL);
    CHECK_FAILED(Load_ItemData(jsonRootPath), E_FAIL);
    CHECK_FAILED(Load_PictosData(jsonRootPath), E_FAIL);

    _isLoaded = true;

    return S_OK;
}

void GameDataTable::Clear()
{
    _nextExpByLevel.clear();
    _weaponDataByCharacter.clear();

    _skillTreeData.clear();
    _requirementSkills.clear();
    _weaponUpgradeDataByKey.clear();

    _skillDefs.clear();
    _skillIndexById.clear();

    _itemDefs.clear();
    _itemIndexById.clear();

    _pictosDefs.clear();
    _pictosIndexById.clear();

    _isLoaded = false;
}

uint32 GameDataTable::Get_NextExp(uint32 level) const
{
    if (!_isLoaded)
        return Get_FallbackNextExp(level);

    if (level == 0)
        return 0;

    if (level >= _nextExpByLevel.size())
        return 0;

    return _nextExpByLevel[level];
}

HRESULT GameDataTable::Load_ExpLevel(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_ExpLevel.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open exp table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse exp table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_ExpLevel") || !root["DT_ExpLevel"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_ExpLevel array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    _nextExpByLevel.assign(100, 0);

    for (const json& item : root["DT_ExpLevel"])
    {
        const uint32 level = item.value("Level", 0u);
        const uint32 nextExp = item.value("NextExp", 0u);

        if (level == 0)
            continue;

        if (level >= _nextExpByLevel.size())
            _nextExpByLevel.resize(static_cast<size_t>(level) + 1, 0);

        _nextExpByLevel[level] = nextExp;
    }

    LOG_INFO("[GameDataTable] loaded exp table: count={}", _nextExpByLevel.size() > 0 ? _nextExpByLevel.size() - 1 : 0);
    return S_OK;
}

HRESULT GameDataTable::Load_SkillData(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_Skill.json";

    json root = {};

    try
    {
        ifstream file(tablePath);

        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open skill table : {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;

    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse skill table : {} ({})", String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_Skill") || !root["DT_Skill"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_Skill array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for (const json& item : root["DT_Skill"])
    {
        const string skillId = item.value("skillKey", "");

        if (skillId.empty())
            continue;

        Skill_Data skill{};

        skill.characterId = item.value("character id", "");
        skill.skillKey = item.value("skillKey", "");
        skill.scriptClassName = item.value("scriptClassName", "");
        skill.apCost = item.value("ap cost", 0);
        skill.battleAnnounceActionText =
            String::ToWString(item.value("battleAnnounceActionText", ""));
        //skill.AttackCount = item.value("AttackCount", 0);
       /* skill.targetingType =
            magic_enum::enum_cast<ECharacterTeamSelector>(item.value("targetingType", ""))
            .value_or(ECharacterTeamSelector::Invalid);*/
        skill.element =
            magic_enum::enum_cast<ElementalType>(item.value("element", ""))
            .value_or(ElementalType::None);
        skill.buff =
            magic_enum::enum_cast<Buffs>(item.value("buff", ""))
            .value_or(Buffs::None);
        skill.ailmentChance = item.value("BuffChance", 0.f);
        // skill.IdleAnimationKey = item.value("IdleAnimationKey", "");

        skill.buffDurationTurn = item.value("BuffTurn", 0);

        skill.targetingType = ParseTargetTypes(item.value("targetingType", ""));
        skill.targetStyle =
            magic_enum::enum_cast<AITargetStyle>(item.value("targetStyle", "Single"))
            .value_or(AITargetStyle::Single);
        skill.openingBias = item.value("openingBias", 0.f);
        skill.midBias = item.value("midBias", 0.f);
        skill.criticalBias = item.value("criticalBias", 0.f);
        skill.powerMultiplier = item.value("powerMultiplier", 1.f);
        skill.forceOnPhaseEnter = ReadBoolFlexible(item, "forceOnPhaseEnter", false);
        skill.forceOnPhaseCritical = ReadBoolFlexible(item, "forceOnPhaseCritical", false);
        skill.minTurn = item.value("minTurn", 0);
        skill.maxUseCount = item.value("maxUseCount", -1);
        skill.coolDownTurn = item.value("coolDownTurn", 0);
        skill.coolDownTurn = item.value("coolDownTurn", 0);
		skill.slCooldownSec = item.value("slCooldownSec", 0.f);

        const size_t index = _skillDefs.size();
        _skillDefs.push_back(skill);
        _skillIndexById[skillId] = index;
    }

    LOG_INFO("[GameDataTable] loaded skill table: count = {}", _skillDefs.size() > 0 ? _skillDefs.size() - 1 : 0);
    return S_OK;
}

HRESULT GameDataTable::Load_ItemData(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_Item.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open item table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse item table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_Item") || !root["DT_Item"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_Item array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for (const json& item : root["DT_Item"])
    {
        const string itemKey = item.value("itemKey", "");
        if (itemKey.empty())
            continue;

        Item_Data itemData{};
        itemData.itemKey = itemKey;
        itemData.displayName = String::ToWString(item.value("displayName", ""));
        itemData.itemType =
            magic_enum::enum_cast<BattleItemType>(item.value("itemType", "Invalid"))
            .value_or(BattleItemType::Invalid);
        itemData.count = item.value("count", 0);
        itemData.defaultCount = itemData.count;
        itemData.healHp = item.value("healHp", 0);
        itemData.restoreAp = item.value("restoreAp", 0);
        itemData.reviveHp = item.value("reviveHp", 0);

        const size_t index = _itemDefs.size();
        _itemDefs.push_back(itemData);
        _itemIndexById[itemKey] = index;
    }

    LOG_INFO("[GameDataTable] loaded item table: count={}", _itemDefs.size());
    return S_OK;
}

HRESULT GameDataTable::Load_PictosData(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_Picktos.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open pictos table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse pictos table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_Picktos") || !root["DT_Picktos"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_Picktos array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    _pictosDefs.clear();
    _pictosIndexById.clear();

    for (const json& item : root["DT_Picktos"])
    {
        PictosDesc desc{};
        desc.id = item.value("id", "");
        if (desc.id.empty())
            continue;

        desc.name = String::ToWString(item.value("name", ""));
        desc.hp = item.value("hp", 0);
        desc.speed = item.value("speed", 0);
        desc.critical = item.value("critical", 0);
        desc.buttonUi = item.value("button ui", "");
        desc.tooltipUi = item.value("tooltip ui", "");
        desc.getItemUi = item.value("get item ui", "");

        const size_t index = _pictosDefs.size();
        _pictosDefs.push_back(desc);
        _pictosIndexById[desc.id] = index;
    }

    LOG_INFO("[GameDataTable] loaded pictos table: count={}", _pictosDefs.size());
    return S_OK;
}

const Skill_Data* GameDataTable::Get_Skill(const string& skillId)
{
    auto it = _skillIndexById.find(skillId);
    if (it == _skillIndexById.end())
        return nullptr;

    const size_t index = it->second;
    if (index >= _skillDefs.size())
        return nullptr;

    return &_skillDefs[index];
}

vector<TargetType> GameDataTable::ParseTargetTypes(const string& data)
{
    vector<TargetType> result;

    stringstream ss(data);
    string token;

    while (getline(ss, token, '|'))
    {
        if (token.empty())
            continue;

        auto opt = magic_enum::enum_cast<TargetType>(token);

        if (opt.has_value())
            result.push_back(opt.value());
        else
        {
            result.push_back(TargetType::Invalid);
            LOG_ERROR("-----------------fail to load TargetTypes----------------");
        }

    }

    return result;
}
const Item_Data* GameDataTable::Get_Item(const string& itemId)
{
    auto it = _itemIndexById.find(itemId);
    if (it == _itemIndexById.end())
        return nullptr;

    const size_t index = it->second;
    if (index >= _itemDefs.size())
        return nullptr;

    return &_itemDefs[index];
}

const Item_Data* GameDataTable::Get_Item(BattleItemType itemType)
{
    for (const auto& item : _itemDefs)
    {
        if (item.itemType == itemType)
            return &item;
    }

    return nullptr;
}

bool GameDataTable::Consume_Item(BattleItemType itemType)
{
    for (Item_Data& item : _itemDefs)
    {
        if (item.itemType != itemType)
            continue;

        if (item.count <= 0)
            return false;

        --item.count;
        return true;
    }

    return false;
}

void GameDataTable::Reset_BattleItemCounts()
{
    for (Item_Data& item : _itemDefs)
    {
        switch (item.itemType)
        {
        case BattleItemType::HealingTint:
        case BattleItemType::EnergyTint:
        case BattleItemType::ReviveTint:
            item.count = item.defaultCount; // DT_Item의 기본 수량으로 새 전투 시작 상태를 복원한다.
            break;
        default:
            break;
        }
    }
}

HRESULT GameDataTable::Load_SkillPhaseData(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_SkillAction.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open skill phase table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse skill phase table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_SkillAction") || !root["DT_SkillAction"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_SkillAction array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for (const json& item : root["DT_SkillAction"])
    {
        const string skillKey = item.value("skillKey", "");
        if (skillKey.empty())
            continue;

        BattleActionPhase phase{};
        phase.phaseIndex = item.value("phaseIndex", 0);
        phase.animationKey = item.value("animationKey", "");
        const string legacyCameraProfileId = item.value("cameraProfileId", "");
        phase.cameraPresetId = item.value("cameraPresetId", legacyCameraProfileId);
        phase.cameraClipId = item.value("cameraClipId", legacyCameraProfileId);

        phase.playRate = item.value("playRate", 1.f);
        if (phase.playRate <= 0.f)
            phase.playRate = 1.f;

        phase.movementPolicy =
            magic_enum::enum_cast<BattleMovementPolicy>(item.value("movementPolicy", "None"))
            .value_or(BattleMovementPolicy::None);

        phase.targetAnchor =
            magic_enum::enum_cast<BattleTargetAnchor>(item.value("targetAnchor", "None"))
            .value_or(BattleTargetAnchor::None);

        phase.useDistance = ReadBoolFlexible(item, "useDistance", false);
        phase.faceTarget = ReadBoolFlexible(item, "faceTarget", true);
        phase.offsetDistance = item.value("offsetDistance", 0.f);

        auto it = _skillIndexById.find(skillKey);
        if (it == _skillIndexById.end())
        {
            LOG_WARN("[GameDataTable] phase references unknown skillKey: {}", skillKey);
            continue;
        }

        const size_t skillIndex = it->second;
        if (skillIndex >= _skillDefs.size())
            continue;

        _skillDefs[skillIndex].AnimPhases.push_back(phase);
    }

    for (Skill_Data& skill : _skillDefs)
    {
        sort(skill.AnimPhases.begin(), skill.AnimPhases.end(),
            [](const BattleActionPhase& a, const BattleActionPhase& b)
            {
                return a.phaseIndex < b.phaseIndex;
            });
    }

    LOG_INFO("[GameDataTable] loaded skill phase table");
    return S_OK;
}

HRESULT GameDataTable::Load_SkillActionData(const wstring& jsonRootPath)
{
    /* const fs::path tablePath = fs::path(jsonRootPath) / L"DT_SkillAction.json";

     json root = {};

     try
     {
         ifstream file(tablePath);

         if(!file.is_open())
         {
             LOG_ERROR("[GameDataTable] failed to open skill action table: {}", String::ToString(tablePath.wstring()));
             return E_FAIL;
         }

         file >>root;
     }
     catch (const std::exception& e)
     {
         LOG_ERROR("[GameDataTable] failed to parse skill action table : {} ({})", String::ToString(tablePath.wstring()), e.what());
         return E_FAIL;
     }

     if (!root.contains("DT_SkillAction") || !root["DT_SkillAction"].is_array())
     {
         LOG_ERROR("[GameDataTable] DT_SkillAction array not found: {}", String::ToString(tablePath.wstring()));
         return E_FAIL;
     }

  for (const json& item : root["DT_SkillAction"])
     {
         const string skillId = item.value("skill id", "");
         const string actionType = item.value("action type", "");

         auto iter = _skillData.find(skillId);
         if (iter == _skillData.end())
         {
             LOG_WARN("Unknown SkillId in DT_SkillAction : {}", skillId);
             continue;
         }

         SkillAction action{};

         if (actionType == "DamageAction")
         {
             DamageAction payload{};
             payload.hitCount = item.value("hit count", 1);
             payload.bonusChargeOnCritical = item.value("bonus charge on critical", 0);

             const string damageType = item.value("damage type", "PHYSICAL");
             if (damageType == "lightning")
                 payload.damageType = DamageType::LIGHTNING;
             else
                 payload.damageType = DamageType::PHYSICAL;

             const string damageLevel = item.value("damage level", "NORMAL");
             if (damageLevel == "low") payload.damageLevel = DamageLevel::LOW;
             else if (damageLevel == "high") payload.damageLevel = DamageLevel::HIGH;
             else if (damageLevel == "extreme") payload.damageLevel = DamageLevel::EXTREME;
             else payload.damageLevel = DamageLevel::NORMAL;

             action.skillActionPayLoad = payload;
         }
         else if (actionType == "HealAction")
         {
             HealAction payload{};
             payload.healPercent = item.value("heal percent", 0);
             action.skillActionPayLoad = payload;
         }
         else if (actionType == "GainChargeAction")
         {
             GainChargeAction payload{};
             payload.min = item.value("charge gain min", 0);
             payload.max = item.value("charge gain max", 0);
             action.skillActionPayLoad = payload;
         }
         else if (actionType == "ShatterAction")
         {
             ShatterAction payload{};
             payload.fullChargeOnShatter = item.value("full charge on shatter", 0) != 0;
             action.skillActionPayLoad = payload;
         }
         else if (actionType == "ApplyMarkAction")
         {
             action.skillActionPayLoad = ApplyMarkAction{};
         }
         else if (actionType == "ClearStatusAction")
         {
             action.skillActionPayLoad = ClearStatusAction{};
         }
         else if (actionType == "BuffAction")
         {
             BuffAction payload {};

             payload.durationTurn = item.value("duration turn", 0);
             action.skillActionPayLoad = payload;
         }
         else
         {
             LOG_WARN("Unknown action type in DT_SkillAction : {}", actionType);
             continue;
         }

         iter->second.skillActions.push_back(move(action));
     }*/
    return S_OK;
}

HRESULT GameDataTable::Load_SkillTreeData(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_SkillTree.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open skill tree table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse skill tree table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_SkillTree") || !root["DT_SkillTree"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_SkillTree array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }


    for (const json& item : root["DT_SkillTree"])
    {
        const string skillId = item.value("skill id", "");

        if (skillId == "")
            continue;

        // _skillTreeData[skillId].requiredSkillId = item.value("required skill id", "");
        //
        // if(_skillTreeData[skillId].requiredSkillId == "")
        //     continue;
        //

        SkillTreeDesc desc{};
        desc.unlockCost = item.value("unlock cost", 0u);
        desc.useCost = item.value("use cost", 0u);
        desc.portraitTextureName = item.value("portrait", "");
        desc.descUiName = item.value("desc ui", "");
        desc.skillName = String::ToWString(item.value("skill name", ""));

        _skillTreeData[skillId] = move(desc);
    }

    LOG_INFO("[GameDataTable] loaded skill tree table: count = {}", _skillTreeData.size() > 0 ? _skillTreeData.size() - 1 : 0);
    return S_OK;
}

HRESULT GameDataTable::Load_SkillTreeRequirementData(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_SkillTreeRequirement.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open skill tree requirement table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse  skill tree requirement table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_SkillTreeRequirement") || !root["DT_SkillTreeRequirement"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_SkillTreeRequirement array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for (const json& item : root["DT_SkillTreeRequirement"])
    {
        const string skillId = item.value("skill id", "");

        if (skillId == "")
            continue;

        const string requiredSkill = item.value("required skill id", "");

        if (requiredSkill == "")
            continue;

        _requirementSkills[skillId].push_back(requiredSkill);
    }

    LOG_INFO("[GameDataTable] loaded skill tree requirement table: count = {}", _requirementSkills.size() > 0 ? _requirementSkills.size() - 1 : 0);
    return S_OK;

}

HRESULT GameDataTable::Load_Weapon(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_Weapon.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open weapon table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse weapon table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_Weapon") || !root["DT_Weapon"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_Weapon array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for (const json& item : root["DT_Weapon"])
    {
        const string characterId = item.value("Character ID", "");
        const string weaponId = item.value("Weapon ID", "");

        if (characterId.empty() || weaponId.empty())
            continue;

        WeaponDesc desc{};
        desc.characterId = characterId;
        desc.weaponId = weaponId;
        desc.weaponName = String::ToWString(item.value("Weapon Name", ""));
        desc.portraitTexturePath = Resolve_WeaponPortraitTexturePath(
            item.value("Portrait", item.value("Portriat", "")));

        desc.weaponPrefabName = item.value("Weapon Prefab", "");
        desc.subWeaponPrefabName = item.value("Sub Weapon Prefab", "");

        desc.level = item.value("Level", 1u);
        desc.power = item.value("Power", 0u);

        _weaponDataByCharacter[characterId].push_back(desc);
    }

    size_t weaponCount = 0;
    for (const auto& [characterId, weapons] : _weaponDataByCharacter)
        weaponCount += weapons.size();

    LOG_INFO("[GameDataTable] loaded weapon table: count = {}", weaponCount);
    return S_OK;
}

HRESULT GameDataTable::Load_WeaponUpgrade(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_WeaponUpgrade.json";

    json root = {};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_ERROR("[GameDataTable] failed to open weapon upgrade table: {}", String::ToString(tablePath.wstring()));
            return E_FAIL;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[GameDataTable] failed to parse weapon upgrade table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return E_FAIL;
    }

    if (!root.contains("DT_WeaponUpgrade") || !root["DT_WeaponUpgrade"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_WeaponUpgrade array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    _weaponUpgradeDataByKey.clear();

    for (const json& item : root["DT_WeaponUpgrade"])
    {
        WeaponUpgradeDesc desc{};
        desc.characterId = item.value("characterId", item.value("Character ID", ""));
        desc.weaponId = item.value("weaponId", item.value("Weapon ID", ""));

        if (desc.characterId.empty() || desc.weaponId.empty())
            continue;

        desc.level = max(1u, item.value("level", item.value("Level", 1u)));
        desc.power = item.value("power", item.value("Power", 0u));
        desc.requiredChromaCatalyst = item.value(
            "requiredChromaCatalyst",
            item.value("Required Chroma Catalyst", 0u));
        desc.vitalityGrade = item.value("vitalityGrade", item.value("Vitality Grade", string{}));
        desc.mightGrade = item.value("mightGrade", item.value("Might Grade", string{}));
        desc.agilityGrade = item.value("agilityGrade", item.value("Agility Grade", string{}));
        desc.defenseGrade = item.value("defenseGrade", item.value("Defense Grade", string{}));
        desc.luckGrade = item.value("luckGrade", item.value("Luck Grade", string{}));

        _weaponUpgradeDataByKey[Make_WeaponUpgradeKey(desc.characterId, desc.weaponId)].push_back(desc);
    }

    size_t rowCount = 0;
    for (auto& [key, rows] : _weaponUpgradeDataByKey)
    {
        UNREFERENCED_PARAMETER(key);

        sort(rows.begin(), rows.end(),
            [](const WeaponUpgradeDesc& lhs, const WeaponUpgradeDesc& rhs)
            {
                return lhs.level < rhs.level;
            });

        rowCount += rows.size();
    }

    LOG_INFO("[GameDataTable] loaded weapon upgrade table: count = {}", rowCount);
    return S_OK;
}

wstring GameDataTable::Get_DefaultJsonRootPath() const
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
        return {};

    return (fs::path(assetRoot) / L"Data" / L"json").wstring();
}

uint32 GameDataTable::Get_FallbackNextExp(uint32 level) const
{
    if (level == 0 || level >= 99)
        return 0;

    return max(1u, level) * 100u;
}

GameDataTable::SkillTreeDesc* GameDataTable::Get_SkillTreeDesc(const string& skillName)
{
    auto iter = _skillTreeData.find(skillName);

    if (iter == _skillTreeData.end())
        return nullptr;

    return &iter->second;
}

const umap<string, vector<string>>* GameDataTable::Get_SkillTreeRequirementData()
{
    return  &_requirementSkills;
}

const GameDataTable::WeaponDesc* GameDataTable::Get_WeaponDesc(const string& characterId, const string& weaponId) const
{
    const auto characterIter = _weaponDataByCharacter.find(characterId);
    if (characterIter == _weaponDataByCharacter.end())
        return nullptr;

    for (const WeaponDesc& weapon : characterIter->second)
    {
        if (weapon.weaponId == weaponId)
            return &weapon;
    }

    return nullptr;
}

const vector<GameDataTable::WeaponDesc>* GameDataTable::Get_WeaponDescs(const string& characterId) const
{
    const auto characterIter = _weaponDataByCharacter.find(characterId);
    if (characterIter == _weaponDataByCharacter.end())
        return nullptr;

    return &characterIter->second;
}

const GameDataTable::WeaponUpgradeDesc* GameDataTable::Get_WeaponUpgradeDesc(
    const string& characterId,
    const string& weaponId,
    uint32 level) const
{
    const auto rowsIter = _weaponUpgradeDataByKey.find(Make_WeaponUpgradeKey(characterId, weaponId));
    if (rowsIter == _weaponUpgradeDataByKey.end())
        return nullptr;

    for (const WeaponUpgradeDesc& row : rowsIter->second)
    {
        if (row.level == level)
            return &row;
    }

    return nullptr;
}

const GameDataTable::WeaponUpgradeDesc* GameDataTable::Get_NextWeaponUpgradeDesc(
    const string& characterId,
    const string& weaponId,
    uint32 currentLevel) const
{
    const auto rowsIter = _weaponUpgradeDataByKey.find(Make_WeaponUpgradeKey(characterId, weaponId));
    if (rowsIter == _weaponUpgradeDataByKey.end())
        return nullptr;

    for (const WeaponUpgradeDesc& row : rowsIter->second)
    {
        if (row.level > currentLevel)
            return &row;
    }

    return nullptr;
}

const vector<GameDataTable::WeaponUpgradeDesc>* GameDataTable::Get_WeaponUpgradeDescs(
    const string& characterId,
    const string& weaponId) const
{
    const auto rowsIter = _weaponUpgradeDataByKey.find(Make_WeaponUpgradeKey(characterId, weaponId));
    if (rowsIter == _weaponUpgradeDataByKey.end())
        return nullptr;

    return &rowsIter->second;
}

vector<string> GameDataTable::Get_AllSkillIds() const
{
    vector<string> skillIds;

    skillIds.reserve(_skillDefs.size());

    for (const Skill_Data& skill : _skillDefs)
    {
        if (!skill.skillKey.empty())
            skillIds.push_back(skill.skillKey);
    }

    sort(skillIds.begin(), skillIds.end());

    return skillIds;
}

vector<string> GameDataTable::Split_String(const string& value, char delimiter)
{
    vector<string> result;
    string token;
    stringstream ss(value);

    while (getline(ss, token, delimiter))
    {
        const size_t first = token.find_first_not_of(" \t");
        if (first == string::npos)
            continue;

        const size_t last = token.find_last_not_of(" \t");
        result.push_back(token.substr(first, last - first + 1));
    }

    return result;
}

const GameDataTable::PictosDesc* GameDataTable::Get_PictosDesc(const string& pictosId) const
{
    const auto it = _pictosIndexById.find(pictosId);
    if (it == _pictosIndexById.end())
        return nullptr;

    const size_t index = it->second;
    if (index >= _pictosDefs.size())
        return nullptr;

    return &_pictosDefs[index];
}

vector<string> GameDataTable::Get_AllPictosIds() const
{
    vector<string> pictosIds{};
    pictosIds.reserve(_pictosDefs.size());

    for (const PictosDesc& desc : _pictosDefs)
        pictosIds.push_back(desc.id);

    return pictosIds;
}

Unique<GameDataTable> GameDataTable::Create()
{
    auto instance = make_unique<GameDataTable>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : GameDataTable");
        return nullptr;
    }

    return instance;
}

void GameDataTable::Free()
{
    Clear();

    __super::Free();
}
