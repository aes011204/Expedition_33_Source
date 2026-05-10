#include "pch.h"
#include "UI_AnimationAssetSerializer.h"

namespace UIEditor
{
    namespace
    {
        constexpr int32 kCurrentAnimationAssetJsonVersion = 1;

        string Read_String(const json& item, const char* key, const string& defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_string())
                return defaultValue;

            return iter->get<string>();
        }

        int32 Read_Int(const json& item, const char* key, int32 defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_number_integer())
                return defaultValue;

            return iter->get<int32>();
        }

        float Read_Float(const json& item, const char* key, float defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_number())
                return defaultValue;

            return iter->get<float>();
        }

        bool Read_Bool(const json& item, const char* key, bool defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_boolean())
                return defaultValue;

            return iter->get<bool>();
        }

        void Normalize_AssetRange(UI_AnimationAsset& asset)
        {
            asset.fps = max(1, asset.fps);
            asset.endFrame = max(asset.startFrame + 1, asset.endFrame);
        }

        void Normalize_Track(UI_AnimationKeyframeTrack& track, const UI_AnimationAsset& asset)
        {
            for (UI_AnimationKeyframe& key : track.keys)
            {
                key.frame = min(max(key.frame, asset.startFrame), asset.endFrame);
                key.value = Clamp_AnimationKeyframeTrackValue(track.type, key.value);
            }

            stable_sort(
                track.keys.begin(),
                track.keys.end(),
                [](const UI_AnimationKeyframe& lhs, const UI_AnimationKeyframe& rhs)
                {
                    return lhs.frame < rhs.frame;
                });

            vector<UI_AnimationKeyframe> mergedKeys;
            mergedKeys.reserve(track.keys.size());
            for (const UI_AnimationKeyframe& key : track.keys)
            {
                if (!mergedKeys.empty() && mergedKeys.back().frame == key.frame)
                {
                    mergedKeys.back() = key;
                    continue;
                }

                mergedKeys.push_back(key);
            }

            track.keys = move(mergedKeys);
        }

        json Keyframe_ToJson(const UI_AnimationKeyframe& key)
        {
            json item = json::object();
            item["frame"] = key.frame;
            item["value"] = key.value;
            return item;
        }

        json Track_ToJson(const UI_AnimationKeyframeTrack& track)
        {
            json item = json::object();
            item["type"] = Get_AnimationKeyframeTrackTypeName(track.type);
            item["enabled"] = track.enabled;
            item["keys"] = json::array();

            for (const UI_AnimationKeyframe& key : track.keys)
                item["keys"].push_back(Keyframe_ToJson(key));

            return item;
        }

        bool Try_ReadKeyframe(const json& item, UI_AnimationKeyframe& outKeyframe)
        {
            if (!item.is_object())
                return false;

            UI_AnimationKeyframe key = {};
            key.frame = Read_Int(item, "frame", 0);
            key.value = Read_Float(item, "value", 0.f);

            outKeyframe = key;
            return true;
        }

        bool Try_ReadTrack(const json& item, UI_AnimationKeyframeTrack& outTrack)
        {
            if (!item.is_object())
                return false;

            UI_AnimationKeyframeTrackType trackType = UI_AnimationKeyframeTrackType::PositionX;
            if (!Try_ParseAnimationKeyframeTrackType(Read_String(item, "type", ""), trackType))
                return false;

            UI_AnimationKeyframeTrack track = {};
            track.type = trackType;
            track.enabled = Read_Bool(item, "enabled", true);

            const auto keyIter = item.find("keys");
            if (keyIter != item.end() && keyIter->is_array())
            {
                for (const json& keyItem : *keyIter)
                {
                    UI_AnimationKeyframe key = {};
                    if (Try_ReadKeyframe(keyItem, key))
                        track.keys.push_back(key);
                }
            }

            outTrack = move(track);
            return true;
        }
    }

    bool UI_AnimationAssetSerializer::Save_ToJsonFile(const UI_AnimationAsset& asset, const wstring& filePath)
    {
        try
        {
            ofstream file(filePath);
            if (!file.is_open())
                return false;

            file << To_Json(asset).dump(4);
            return true;
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI animation asset save failed: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    bool UI_AnimationAssetSerializer::Load_FromJsonFile(UI_AnimationAsset& asset, const wstring& filePath)
    {
        try
        {
            ifstream file(filePath);
            if (!file.is_open())
                return false;

            json root = json::parse(file, nullptr, false);
            if (root.is_discarded())
                return false;

            return From_Json(asset, root);
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI animation asset load failed: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    json UI_AnimationAssetSerializer::To_Json(const UI_AnimationAsset& asset)
    {
        UI_AnimationAsset normalizedAsset = asset;
        Normalize_AssetRange(normalizedAsset);
        for (UI_AnimationKeyframeTrack& track : normalizedAsset.tracks)
            Normalize_Track(track, normalizedAsset);

        json root = json::object();
        root["version"] = kCurrentAnimationAssetJsonVersion;
        root["name"] = normalizedAsset.name;
        root["targetName"] = normalizedAsset.targetName;
        root["fps"] = normalizedAsset.fps;
        root["startFrame"] = normalizedAsset.startFrame;
        root["endFrame"] = normalizedAsset.endFrame;
        root["loop"] = normalizedAsset.loop;
        root["tracks"] = json::array();

        for (const UI_AnimationKeyframeTrack& track : normalizedAsset.tracks)
            root["tracks"].push_back(Track_ToJson(track));

        return root;
    }

    bool UI_AnimationAssetSerializer::From_Json(UI_AnimationAsset& asset, const json& root)
    {
        if (!root.is_object())
            return false;

        UI_AnimationAsset loadedAsset = {};
        loadedAsset.name = Read_String(root, "name", "NewUIAnimation");
        loadedAsset.targetName = Read_String(root, "targetName", "");
        loadedAsset.targetObjectId = 0;
        loadedAsset.fps = Read_Int(root, "fps", 60);
        loadedAsset.startFrame = Read_Int(root, "startFrame", 0);
        loadedAsset.endFrame = Read_Int(root, "endFrame", 60);
        loadedAsset.loop = Read_Bool(root, "loop", false);
        Normalize_AssetRange(loadedAsset);

        const auto trackIter = root.find("tracks");
        if (trackIter != root.end() && trackIter->is_array())
        {
            for (const json& trackItem : *trackIter)
            {
                UI_AnimationKeyframeTrack track = {};
                if (!Try_ReadTrack(trackItem, track))
                    continue;

                Normalize_Track(track, loadedAsset);
                loadedAsset.tracks.push_back(move(track));
            }
        }

        asset = move(loadedAsset);
        return true;
    }
}
