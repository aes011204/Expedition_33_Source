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
