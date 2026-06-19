#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

class RuntimeCheckpointIO final
{
public:
    static constexpr uint32 CurrentVersion = 2;

    struct WorldObject
    {
        wstring layerTag{};
        json objectJson{};
    };

    struct HeroState
    {
        HeroId heroId = HeroId::END;
        HeroPartySlot slot = HeroPartySlot::Slot_0;
        Vec3 position = Vec3::Zero;
        Vec3 rotation = Vec3::Zero;
        json statJson = json::object();
        json skillTreeJson = json::object();
    };

    struct Checkpoint
    {
        uint32 version = CurrentVersion;
        LevelType levelType = LevelType::MapTest;
        HeroPartySlot activeSlot = HeroPartySlot::Slot_0;
        json inventoryJson = json::object();
        json costumeJson = json::object();
        json unlockManagerJson = json::object();
        vector<HeroState> heroes{};
        vector<WorldObject> worldObjects{};
    };

public:
    RuntimeCheckpointIO() = delete;

    static bool Save_Msgpack(const fs::path& path, const Checkpoint& checkpoint);
    static bool Load_Msgpack(const fs::path& path, Checkpoint& outCheckpoint);

private:
    static json To_Json(const Checkpoint& checkpoint);
    static bool From_Json(const json& root, Checkpoint& outCheckpoint);
};

NS_END
