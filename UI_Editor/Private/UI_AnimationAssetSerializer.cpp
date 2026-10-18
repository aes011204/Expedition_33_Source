#include "pch.h"
#include "UI_AnimationAssetSerializer.h"

namespace UIEditor
{
    namespace
    {
        constexpr int32 kAnimationAssetJsonVersion = 7;
        constexpr int32 kStateAnimationAssetJsonVersion = 1;

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

        ImVec4 Read_Color255(const json& item, const char* key, const ImVec4& defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_array() || iter->size() < 4)
                return defaultValue;

            return Clamp_AnimationKeyframeColor(ImVec4(
                iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
                iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y,
                iter->at(2).is_number() ? iter->at(2).get<float>() : defaultValue.z,
                iter->at(3).is_number() ? iter->at(3).get<float>() : defaultValue.w));
        }

        ImVec2 Read_Vec2(const json& item, const char* key, const ImVec2& defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_array() || iter->size() < 2)
                return defaultValue;

            return ImVec2(
                iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
                iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y);
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
                if (track.type == UI_AnimationKeyframeTrackType::Color)
                    key.color = Clamp_AnimationKeyframeColor(key.color);
                else if (Is_AnimationKeyframeTrackVec2Value(track.type))
                    key.valueVec2 = Clamp_AnimationKeyframeTrackVec2Value(track.type, key.valueVec2);
                else
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

        json Keyframe_ToJson(UI_AnimationKeyframeTrackType trackType, const UI_AnimationKeyframe& key)
        {
            json item = json::object();
            item["frame"] = key.frame;
            if (trackType == UI_AnimationKeyframeTrackType::Color)
            {
                const ImVec4 color = Clamp_AnimationKeyframeColor(key.color);
                item["color"] = { color.x, color.y, color.z, color.w };
            }
            else if (Is_AnimationKeyframeTrackVec2Value(trackType))
            {
                const ImVec2 value = Clamp_AnimationKeyframeTrackVec2Value(trackType, key.valueVec2);
                item["value"] = { value.x, value.y };
            }
            else
            {
                item["value"] = key.value;
            }
            item["easing"] = Get_AnimationEasingName(key.easing);
            return item;
        }

        json Track_ToJson(const UI_AnimationKeyframeTrack& track)
        {
            json item = json::object();
            const UI_AnimationKeyframeTrackType serializedType =
                track.type == UI_AnimationKeyframeTrackType::Alpha ? UI_AnimationKeyframeTrackType::Color : track.type;
            item["type"] = Get_AnimationKeyframeTrackTypeName(serializedType);
            item["enabled"] = track.enabled;
            item["easing"] = Get_AnimationEasingName(track.easing);
            item["keys"] = json::array();

            for (const UI_AnimationKeyframe& key : track.keys)
            {
                if (track.type == UI_AnimationKeyframeTrackType::Alpha)
                {
                    UI_AnimationKeyframe colorKey = key;
                    colorKey.color = Clamp_AnimationKeyframeColor(ImVec4(
                        key.color.x,
                        key.color.y,
                        key.color.z,
                        Clamp_AnimationKeyframeTrackValue(UI_AnimationKeyframeTrackType::Alpha, key.value) * 255.f));
                    item["keys"].push_back(Keyframe_ToJson(serializedType, colorKey));
                }
                else
                {
                    item["keys"].push_back(Keyframe_ToJson(serializedType, key));
                }
            }

            return item;
        }

        json Target_ToJson(const UI_AnimationTargetBinding& target)
        {
            json item = json::object();
            item["targetName"] = target.targetName;

            item["tracks"] = json::array();
            for (const UI_AnimationKeyframeTrack& track : target.tracks)
                item["tracks"].push_back(Track_ToJson(track));

            return item;
        }

        bool Try_ReadKeyframe(const json& item, UI_AnimationKeyframeTrackType trackType, UI_AnimationEasing defaultEasing, UI_AnimationKeyframe& outKeyframe)
        {
            if (!item.is_object())
                return false;

            UI_AnimationKeyframe key = {};
            key.frame = Read_Int(item, "frame", 0);
            if (trackType == UI_AnimationKeyframeTrackType::Color)
            {
                key.color = Read_Color255(item, "color", ImVec4(255.f, 255.f, 255.f, 255.f));
            }
            else if (Is_AnimationKeyframeTrackVec2Value(trackType))
            {
                const ImVec2 defaultValue = trackType == UI_AnimationKeyframeTrackType::PatternUvScale
                    ? ImVec2(1.f, 1.f)
                    : ImVec2(0.f, 0.f);
                key.valueVec2 = Clamp_AnimationKeyframeTrackVec2Value(trackType, Read_Vec2(item, "value", defaultValue));
            }
            else
            {
                key.value = Read_Float(item, "value", 0.f);
                if (trackType == UI_AnimationKeyframeTrackType::Alpha)
                    key.color = ImVec4(255.f, 255.f, 255.f, Clamp_AnimationKeyframeTrackValue(trackType, key.value) * 255.f);
            }
            key.easing = defaultEasing;

            UI_AnimationEasing easing = defaultEasing;
            if (Try_ParseAnimationEasing(Read_String(item, "easing", Get_AnimationEasingName(defaultEasing)), easing))
                key.easing = easing;

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

            UI_AnimationEasing easing = UI_AnimationEasing::Linear;
            if (Try_ParseAnimationEasing(Read_String(item, "easing", "Linear"), easing))
                track.easing = easing;

            const auto keyIter = item.find("keys");
            if (keyIter != item.end() && keyIter->is_array())
            {
                for (const json& keyItem : *keyIter)
                {
                    UI_AnimationKeyframe key = {};
                    if (Try_ReadKeyframe(keyItem, track.type, track.easing, key))
                        track.keys.push_back(key);
                }
            }

            outTrack = move(track);
            return true;
        }

        void Read_Tracks(const json& item, const UI_AnimationAsset& asset, vector<UI_AnimationKeyframeTrack>& outTracks)
        {
            const auto trackIter = item.find("tracks");
            if (trackIter == item.end() || !trackIter->is_array())
                return;

            for (const json& trackItem : *trackIter)
            {
                UI_AnimationKeyframeTrack track = {};
                if (!Try_ReadTrack(trackItem, track))
                    continue;

                Normalize_Track(track, asset);
                outTracks.push_back(move(track));
            }
        }

        bool Try_ReadTarget(const json& item, const UI_AnimationAsset& asset, UI_AnimationTargetBinding& outTarget)
        {
            if (!item.is_object())
                return false;

            UI_AnimationTargetBinding target = {};
            target.targetName = Read_String(item, "targetName", "");
            target.targetObjectId = 0;
            Read_Tracks(item, asset, target.tracks);

            if (target.targetName.empty() && target.tracks.empty())
                return false;

            outTarget = move(target);
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
        for (UI_AnimationTargetBinding& target : normalizedAsset.targets)
        {
            for (UI_AnimationKeyframeTrack& track : target.tracks)
                Normalize_Track(track, normalizedAsset);
        }
        for (UI_AnimationKeyframeTrack& track : normalizedAsset.tracks)
            Normalize_Track(track, normalizedAsset);

        json root = json::object();
        root["version"] = kAnimationAssetJsonVersion;
        root["animationType"] = Get_AnimationAssetTypeName(normalizedAsset.animationType);
        root["name"] = normalizedAsset.name;
        root["fps"] = normalizedAsset.fps;
        root["startFrame"] = normalizedAsset.startFrame;
        root["endFrame"] = normalizedAsset.endFrame;
        root["loop"] = normalizedAsset.loop;

        if (Is_PublicAnimation(normalizedAsset))
        {
            root["tracks"] = json::array();
            for (const UI_AnimationKeyframeTrack& track : normalizedAsset.tracks)
                root["tracks"].push_back(Track_ToJson(track));
            return root;
        }

        root["targets"] = json::array();
        for (const UI_AnimationTargetBinding& target : normalizedAsset.targets)
            root["targets"].push_back(Target_ToJson(target));

        return root;
    }

    bool UI_AnimationAssetSerializer::From_Json(UI_AnimationAsset& asset, const json& root)
    {
        if (!root.is_object())
            return false;

        UI_AnimationAsset loadedAsset = {};
        loadedAsset.name = Read_String(root, "name", "NewUIAnimation");
        UI_AnimationAssetType animationType = UI_AnimationAssetType::Target_Animation;
        if (Try_ParseAnimationAssetType(Read_String(root, "animationType", ""), animationType))
            loadedAsset.animationType = animationType;
        else if (Try_ParseAnimationAssetType(Read_String(root, "bindingMode", ""), animationType))
            loadedAsset.animationType = animationType;
        else
            loadedAsset.animationType = UI_AnimationAssetType::Target_Animation;
        loadedAsset.fps = Read_Int(root, "fps", 60);
        loadedAsset.startFrame = Read_Int(root, "startFrame", 0);
        loadedAsset.endFrame = Read_Int(root, "endFrame", 60);
        loadedAsset.loop = Read_Bool(root, "loop", false);
        Normalize_AssetRange(loadedAsset);

        if (Is_PublicAnimation(loadedAsset))
        {
            Read_Tracks(root, loadedAsset, loadedAsset.tracks);
            asset = move(loadedAsset);
            return true;
        }

        const auto targetIter = root.find("targets");
        if (targetIter != root.end() && targetIter->is_array())
        {
            for (const json& targetItem : *targetIter)
            {
                UI_AnimationTargetBinding target = {};
                if (!Try_ReadTarget(targetItem, loadedAsset, target))
                    continue;

                loadedAsset.targets.push_back(move(target));
            }
        }
        else
        {
            UI_AnimationTargetBinding legacyTarget = {};
            legacyTarget.targetName = Read_String(root, "targetName", "");
            legacyTarget.targetObjectId = 0;
            Read_Tracks(root, loadedAsset, legacyTarget.tracks);

            if (!legacyTarget.targetName.empty() || !legacyTarget.tracks.empty())
                loadedAsset.targets.push_back(move(legacyTarget));
        }

        asset = move(loadedAsset);
        return true;
    }

    bool UI_StateAnimationAssetSerializer::Save_ToJsonFile(const UI_StateAnimationAsset& asset, const wstring& filePath)
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
            LOG_WARN("UI state animation asset save failed: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    bool UI_StateAnimationAssetSerializer::Load_FromJsonFile(UI_StateAnimationAsset& asset, const wstring& filePath)
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
            LOG_WARN("UI state animation asset load failed: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    json UI_StateAnimationAssetSerializer::To_Json(const UI_StateAnimationAsset& asset)
    {
        json root = json::object();
        root["version"] = kStateAnimationAssetJsonVersion;
        root["animationType"] = "State_Animation";
        root["name"] = asset.name;
        root["events"] = json::object();

        for (int32 index = 0; index < static_cast<int32>(UI_StateAnimationEvent::Count); ++index)
        {
            const UI_StateAnimationEvent eventType = static_cast<UI_StateAnimationEvent>(index);
            const string& animationPath = asset.eventAnimationPaths[static_cast<size_t>(eventType)];
            if (!animationPath.empty())
                root["events"][Get_StateAnimationEventName(eventType)] = animationPath;
        }

        return root;
    }

    bool UI_StateAnimationAssetSerializer::From_Json(UI_StateAnimationAsset& asset, const json& root)
    {
        if (!root.is_object())
            return false;

        UI_StateAnimationAsset loadedAsset = {};
        loadedAsset.name = Read_String(root, "name", "NewUIStateAnimation");

        const auto eventsIter = root.find("events");
        if (eventsIter != root.end() && eventsIter->is_object())
        {
            for (auto iter = eventsIter->begin(); iter != eventsIter->end(); ++iter)
            {
                if (!iter.value().is_string())
                    continue;

                UI_StateAnimationEvent eventType = UI_StateAnimationEvent::HoverEnter;
                if (!Try_ParseStateAnimationEvent(iter.key(), eventType))
                    continue;

                loadedAsset.eventAnimationPaths[static_cast<size_t>(eventType)] = iter.value().get<string>();
            }
        }

        asset = move(loadedAsset);
        return true;
    }
}
