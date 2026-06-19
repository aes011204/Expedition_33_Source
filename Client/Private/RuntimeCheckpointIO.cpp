#include "RuntimeCheckpointIO.h"
#include <fstream>

NS_BEGIN(Client)

namespace
{
    template <typename TEnum>
    string Enum_ToString(TEnum value)
    {
        return string(magic_enum::enum_name(value));
    }

    Vec3 Read_Vec3(const json& data, const Vec3& fallback)
    {
        if (!data.is_array() || data.size() < 3)
            return fallback;

        return Vec3(
            data.at(0).get<float>(),
            data.at(1).get<float>(),
            data.at(2).get<float>());
    }

    json Write_Vec3(const Vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }
}

bool RuntimeCheckpointIO::Save_Msgpack(const fs::path& path, const Checkpoint& checkpoint)
{
    if (path.empty() || !path.has_parent_path())
        return false;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec)
        return false;

    try
    {
        const json root = To_Json(checkpoint);
        const vector<uint8_t> bytes = json::to_msgpack(root);

        ofstream out(path, ios::binary | ios::trunc);
        if (!out.is_open())
            return false;

        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
        return out.good();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[RuntimeCheckpointIO] save failed: path={}, error={}",
            String::ToString(path.wstring()), e.what());
        return false;
    }
}

bool RuntimeCheckpointIO::Load_Msgpack(const fs::path& path, Checkpoint& outCheckpoint)
{
    if (path.empty() || !fs::exists(path))
        return false;

    try
    {
        ifstream in(path, ios::binary);
        if (!in.is_open())
            return false;

        const json root = json::from_msgpack(in);
        return From_Json(root, outCheckpoint);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[RuntimeCheckpointIO] load failed: path={}, error={}",
            String::ToString(path.wstring()), e.what());
        return false;
    }
}

json RuntimeCheckpointIO::To_Json(const Checkpoint& checkpoint)
{
    json root = json::object();
    root["version"] = checkpoint.version;
    root["level_type"] = Enum_ToString(checkpoint.levelType);
    root["active_slot"] = Enum_ToString(checkpoint.activeSlot);
    root["inventory"] = checkpoint.inventoryJson;
    root["costumes"] = checkpoint.costumeJson;

    root["heroes"] = json::array();
    for (const HeroState& hero : checkpoint.heroes)
    {
        json entry = json::object();
        entry["hero_id"] = Enum_ToString(hero.heroId);
        entry["slot"] = Enum_ToString(hero.slot);
        entry["position"] = Write_Vec3(hero.position);
        entry["rotation"] = Write_Vec3(hero.rotation);
        entry["stat"] = hero.statJson;
        root["heroes"].emplace_back(entry);
    }

    root["world_objects"] = json::array();
    for (const WorldObject& object : checkpoint.worldObjects)
    {
        json entry = json::object();
        entry["layer_tag"] = String::ToString(object.layerTag);
        entry["object"] = object.objectJson;
        root["world_objects"].emplace_back(entry);
    }

    return root;
}

bool RuntimeCheckpointIO::From_Json(const json& root, Checkpoint& outCheckpoint)
{
    if (!root.is_object())
        return false;

    Checkpoint checkpoint{};
    checkpoint.version = root.value("version", 0u);
    if (checkpoint.version != CurrentVersion)
        return false;

    const auto levelType = magic_enum::enum_cast<LevelType>(root.value("level_type", string{}));
    if (!levelType.has_value())
        return false;
    checkpoint.levelType = levelType.value();

    const auto activeSlot = magic_enum::enum_cast<HeroPartySlot>(root.value("active_slot", string{}));
    checkpoint.activeSlot = activeSlot.value_or(HeroPartySlot::Slot_0);

    checkpoint.inventoryJson = root.value("inventory", json::object());
    checkpoint.costumeJson = root.value("costumes", json::object());

    if (auto heroes = root.find("heroes"); heroes != root.end() && heroes->is_array())
    {
        for (const json& entry : *heroes)
        {
            if (!entry.is_object())
                continue;

            const auto heroId = magic_enum::enum_cast<HeroId>(entry.value("hero_id", string{}));
            if (!heroId.has_value())
                continue;

            HeroState hero{};
            hero.heroId = heroId.value();
            hero.slot = magic_enum::enum_cast<HeroPartySlot>(entry.value("slot", string{})).value_or(HeroPartySlot::Slot_0);
            hero.position = Read_Vec3(entry.value("position", json::array()), Vec3::Zero);
            hero.rotation = Read_Vec3(entry.value("rotation", json::array()), Vec3::Zero);
            hero.statJson = entry.value("stat", json::object());
            checkpoint.heroes.emplace_back(std::move(hero));
        }
    }

    if (auto objects = root.find("world_objects"); objects != root.end() && objects->is_array())
    {
        for (const json& entry : *objects)
        {
            if (!entry.is_object())
                continue;

            WorldObject object{};
            object.layerTag = String::ToWString(entry.value("layer_tag", string{}));
            object.objectJson = entry.value("object", json::object());
            if (object.layerTag.empty() || !object.objectJson.is_object())
                continue;

            checkpoint.worldObjects.emplace_back(std::move(object));
        }
    }

    outCheckpoint = std::move(checkpoint);
    return true;
}

NS_END
