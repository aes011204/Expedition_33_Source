#include "GameDataTable.h"
#include "GameInstance.h"

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
    
    CHECK_FAILED(Load_SkillActionData(jsonRootPath), E_FAIL);

    CHECK_FAILED(Load_SkillTreeData(jsonRootPath), E_FAIL);

    CHECK_FAILED(Load_SkillTreeRequirementData(jsonRootPath), E_FAIL);

    _isLoaded = true;

    return S_OK;
}

void GameDataTable::Clear()
{
    _nextExpByLevel.clear();
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

        if(!file.is_open())
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

    if(!root.contains("DT_Skill") || !root["DT_Skill"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_Skill array not found: {}",String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for(const json& item : root["DT_Skill"])
    {
        const string skillId = item.value("skill id","");

        if(skillId == "")
            continue;

       _skillData[skillId].name = String::ToWString(item.value("name", ""));

       if( _skillData[skillId].name  == L"")
           continue;

        _skillData[skillId].apCost = item.value("ap cost", 0);
    }

    LOG_INFO("[GameDataTable] loaded skill table: count = {}", _skillData.size() > 0? _skillData.size() - 1 : 0);
    return S_OK;
}

HRESULT GameDataTable::Load_SkillActionData(const wstring& jsonRootPath)
{
    const fs::path tablePath = fs::path(jsonRootPath) / L"DT_SkillAction.json";

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
    }

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

    if(!root.contains("DT_SkillTree") || !root["DT_SkillTree"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_SkillTree array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for(const json& item : root["DT_SkillTree"])
    {
        const string skillId = item.value("skill id", "");

        if(skillId == "")
            continue;

       // _skillTreeData[skillId].requiredSkillId = item.value("required skill id", "");
       //
       // if(_skillTreeData[skillId].requiredSkillId == "")
       //     continue;
       //
        _skillTreeData[skillId].unlockCost = item.value("unlock cost", 0);
    }

    LOG_INFO("[GameDataTable] loaded skill tree table: count = {}", _skillTreeData.size() > 0? _skillTreeData.size() -1 : 0);
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

    if(!root.contains("DT_SkillTreeRequirement") || !root["DT_SkillTreeRequirement"].is_array())
    {
        LOG_ERROR("[GameDataTable] DT_SkillTreeRequirement array not found: {}", String::ToString(tablePath.wstring()));
        return E_FAIL;
    }

    for(const json& item : root["DT_SkillTreeRequirement"])
    {
        const string skillId = item.value("skill id", "");

        if(skillId == "")
            continue;

        const string requiredSkill = item.value("required skill id", "");
        _requirementSkills[skillId].push_back(requiredSkill);
    }

    LOG_INFO("[GameDataTable] loaded skill tree requirement table: count = {}", _requirementSkills.size() > 0? _requirementSkills.size() -1 : 0);
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

    if(iter == _skillTreeData.end())
        return nullptr;

    return &iter->second;
}

const umap<string, vector<string>>* GameDataTable::Get_SkillTreeRequirementData()
{
    return  &_requirementSkills;
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
