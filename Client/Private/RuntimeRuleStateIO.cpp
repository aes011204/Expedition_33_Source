#include "RuntimeRuleStateIO.h"
#include <fstream>

NS_BEGIN(Client)

bool RuntimeRuleStateIO::Save_Msgpack(const fs::path& path, const RuleState& ruleState)
{
    if (path.empty() || !path.has_parent_path())
        return false;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec)
        return false;

    try
    {
        const json root = To_Json(ruleState);
        const vector<uint8_t> bytes = json::to_msgpack(root);

        ofstream out(path, ios::binary | ios::trunc);
        if (!out.is_open())
            return false;

        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
        return out.good();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[RuntimeRuleStateIO] save failed: path={}, error={}",
            String::ToString(path.wstring()), e.what());
        return false;
    }
}

bool RuntimeRuleStateIO::Load_Msgpack(const fs::path& path, RuleState& outRuleState)
{
    if (path.empty() || !fs::exists(path))
        return false;

    try
    {
        ifstream in(path, ios::binary);
        if (!in.is_open())
            return false;

        const json root = json::from_msgpack(in);
        return From_Json(root, outRuleState);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[RuntimeRuleStateIO] load failed: path={}, error={}",
            String::ToString(path.wstring()), e.what());
        return false;
    }
}

json RuntimeRuleStateIO::To_Json(const RuleState& ruleState)
{
    json root = json::object();
    root["version"] = ruleState.version;
    root["costumes"] = ruleState.costumeJson;
    root["unlock_manager"] = ruleState.unlockManagerJson;
    return root;
}

bool RuntimeRuleStateIO::From_Json(const json& root, RuleState& outRuleState)
{
    if (!root.is_object())
        return false;

    RuleState ruleState{};
    ruleState.version = root.value("version", 0u);
    if (ruleState.version != CurrentVersion)
        return false;

    ruleState.costumeJson = root.value("costumes", json::object());
    ruleState.unlockManagerJson = root.value("unlock_manager", json::object());

    outRuleState = std::move(ruleState);
    return true;
}

NS_END
