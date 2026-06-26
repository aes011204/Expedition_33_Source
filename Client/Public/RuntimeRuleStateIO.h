#pragma once

#include "Client_Defines.h"
#include "RuntimeCheckpointIO.h"

NS_BEGIN(Client)

class RuntimeRuleStateIO final
{
public:
    static constexpr uint32 CurrentVersion = 1;

    struct RuleState
    {
        uint32 version = CurrentVersion;
        json costumeJson = json::object();
        json unlockManagerJson = json::object();
        json inventoryJson = json::object();
        vector<RuntimeCheckpointIO::HeroState> heroes{};
    };

public:
    RuntimeRuleStateIO() = delete;

    static bool Save_Msgpack(const fs::path& path, const RuleState& ruleState);
    static bool Load_Msgpack(const fs::path& path, RuleState& outRuleState);

private:
    static json To_Json(const RuleState& ruleState);
    static bool From_Json(const json& root, RuleState& outRuleState);
};

NS_END
