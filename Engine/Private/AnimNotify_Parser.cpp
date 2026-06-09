#include "pch.h"
#include "AnimNotify_Parser.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "GameInstance.h"

json AnimNotify_Parser::To_Json(const AnimNotifyAsset& asset)
{
    json root = json::object();

    root["version"] = asset.version;
    root["model_guid"] = asset.modelGuid;
    root["animations"] = json::array();

    for (const auto& animationData : asset.animations)
    {
        json animationNode = json::object();
        animationNode["animation_name"] = animationData.animationName;
        animationNode["display_fps"] = animationData.displayFps;
        animationNode["next_notify_id"] = animationData.nextNotifyId;
        animationNode["next_state_id"] = animationData.nextStateId;

        animationNode["notify_tracks"] = json::array();
        for (const auto& track : animationData.notifyTracks)
            animationNode["notify_tracks"].push_back(track.name);

        animationNode["notify_state_tracks"] = json::array();
        for (const auto& track : animationData.notifyStateTracks)
            animationNode["notify_state_tracks"].push_back(track.name);

        animationNode["notifies"] = json::array();
        for (const auto& entry : animationData.notifies)
        {
            json entryNode = json::object();
            entryNode["id"] = entry.id;
            entryNode["time_sec"] = entry.timeSec;
            entryNode["track_index"] = entry.trackIndex;
            entryNode["type_name"] = entry.notify ? entry.notify->Get_TypeName() : "";
            entryNode["payload"] = entry.notify ? entry.notify->To_Json() : json::object();

            animationNode["notifies"].push_back(::move(entryNode));
        }

        animationNode["notify_states"] = json::array();
        for (const auto& entry : animationData.notifyStates)
        {
            json entryNode = json::object();
            entryNode["id"] = entry.id;
            entryNode["start_sec"] = entry.startSec;
            entryNode["duration_sec"] = entry.durationSec;
            entryNode["track_index"] = entry.trackIndex;
            entryNode["type_name"] = entry.notifyState ? entry.notifyState->Get_TypeName() : "";
            entryNode["payload"] = entry.notifyState ? entry.notifyState->To_Json() : json::object();

            animationNode["notify_states"].push_back(::move(entryNode));
        }

        root["animations"].push_back(::move(animationNode));
    }

    return root;
}

bool AnimNotify_Parser::From_Json(const json& root, AnimNotifyAsset& outAsset)
{
    if (!root.is_object())
        return false;

    outAsset = {};
    outAsset.version = root.value("version", 2);
    outAsset.modelGuid = root.value("model_guid", string{});

    const json animationsNode =
        (root.contains("animations") && root["animations"].is_array())
        ? root["animations"]
        : json::array();

    for (const auto& animationNode : animationsNode)
    {
        if (!animationNode.is_object())
            continue;

        AnimNotifyData animationData{};
        animationData.animationName = animationNode.value("animation_name", string{});
        animationData.displayFps = max(1, animationNode.value("display_fps", 60));
        animationData.nextNotifyId = max(1, animationNode.value("next_notify_id", 1));
        animationData.nextStateId = max(1, animationNode.value("next_state_id", 1));

        const json notifyTracksNode =
            (animationNode.contains("notify_tracks") && animationNode["notify_tracks"].is_array())
            ? animationNode["notify_tracks"]
            : json::array();

        for (const auto& trackNameNode : notifyTracksNode)
        {
            if (trackNameNode.is_string())
                animationData.notifyTracks.push_back({ trackNameNode.get<string>() });
        }

        const json notifyStateTracksNode =
            (animationNode.contains("notify_state_tracks") && animationNode["notify_state_tracks"].is_array())
            ? animationNode["notify_state_tracks"]
            : json::array();

        for (const auto& trackNameNode : notifyStateTracksNode)
        {
            if (trackNameNode.is_string())
                animationData.notifyStateTracks.push_back({ trackNameNode.get<string>() });
        }

        Ensure_DefaultTracks(animationData);

        const json notifiesNode =
            (animationNode.contains("notifies") && animationNode["notifies"].is_array())
            ? animationNode["notifies"]
            : json::array();

        for (const auto& entryNode : notifiesNode)
        {
            if (!entryNode.is_object())
                continue;

            AnimNotifyEventEntry entry{};
            entry.id = entryNode.value("id", 0);
            entry.timeSec = entryNode.value("time_sec", 0.f);
            entry.trackIndex = entryNode.value("track_index", 0);

            if (animationData.notifyTracks.empty())
                entry.trackIndex = 0;
            else
                entry.trackIndex = ::clamp(entry.trackIndex, 0, static_cast<int32>(animationData.notifyTracks.size()) - 1);

            const string typeName = entryNode.value("type_name", entryNode.value("name", string{}));
            if (typeName.empty())
                continue;

            Shared<AnimNotify> instance = GAME->Create_AnimNotifyPrototype(typeName);
            if (!instance)
                continue;

            if (entryNode.contains("payload") && entryNode["payload"].is_object())
                instance->From_Json(entryNode["payload"]);

            entry.notify = instance;
            animationData.notifies.push_back(::move(entry));
        }

        const json notifyStatesNode =
            (animationNode.contains("notify_states") && animationNode["notify_states"].is_array())
            ? animationNode["notify_states"]
            : json::array();

        for (const auto& entryNode : notifyStatesNode)
        {
            if (!entryNode.is_object())
                continue;

            AnimNotifyStateEntry entry{};
            entry.id = entryNode.value("id", 0);
            entry.startSec = entryNode.value("start_sec", 0.f);
            entry.durationSec = entryNode.value("duration_sec", 0.f);
            entry.trackIndex = entryNode.value("track_index", 0);

            if (animationData.notifyStateTracks.empty())
                entry.trackIndex = 0;
            else
                entry.trackIndex = ::clamp(entry.trackIndex, 0, static_cast<int32>(animationData.notifyStateTracks.size()) - 1);

            const string typeName = entryNode.value("type_name", entryNode.value("name", string{}));
            if (typeName.empty())
                continue;

            Shared<AnimNotifyState> instance = GAME->Create_AnimNotifyStatePrototype(typeName);
            if (!instance)
                continue;

            if (entryNode.contains("payload") && entryNode["payload"].is_object())
                instance->From_Json(entryNode["payload"]);

            entry.notifyState = instance;
            animationData.notifyStates.push_back(::move(entry));
        }

        Sort_And_Clamp(animationData);
        outAsset.animations.push_back(::move(animationData));
    }

    return !outAsset.modelGuid.empty() || !outAsset.animations.empty();
}

bool AnimNotify_Parser::Save_ToFile(const wstring& filePath, const AnimNotifyAsset& asset)
{
    try
    {
        const fs::path path(filePath);
        fs::create_directories(path.parent_path());

        ::ofstream file(path);
        if (!file.is_open())
            return false;

        file << To_Json(asset).dump(4);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool AnimNotify_Parser::Load_FromFile(const wstring& filePath, AnimNotifyAsset& outAsset)
{
    try
    {
        const fs::path path(filePath);

        if (!fs::exists(path))
            return false;

        ::ifstream file(path);
        if (!file.is_open())
            return false;

        json root = json::parse(file, nullptr, false);
        if (root.is_discarded())
            return false;

        return From_Json(root, outAsset);
    }
    catch (...)
    {
        return false;
    }
}

bool AnimNotify_Parser::Load_ModelAsset(const string& modelGuid, AnimNotifyAsset& outAsset)
{
    outAsset = {};
    outAsset.modelGuid = modelGuid;

    bool loadedAnySplitFile = false;
    const fs::path folderPath = Get_ModelNotifyFolderPath(modelGuid);

    try
    {
        if (fs::exists(folderPath) && fs::is_directory(folderPath))
        {
            vector<fs::path> splitFiles;

            for (const fs::directory_entry& entry : fs::directory_iterator(folderPath))
            {
                if (!entry.is_regular_file())
                    continue;

                const fs::path path = entry.path();
                if (path.extension() == L".json" && path.filename().wstring().ends_with(L".notify.json"))
                    splitFiles.push_back(path);
            }

            ::sort(splitFiles.begin(), splitFiles.end());

            for (const fs::path& splitFile : splitFiles)
            {
                AnimNotifyAsset splitAsset{};
                if (!Load_FromFile(splitFile.wstring(), splitAsset))
                    continue;

                for (auto& animationData : splitAsset.animations)
                    Merge_AnimationIntoAsset(outAsset, ::move(animationData));

                loadedAnySplitFile = true;
            }
        }
    }
    catch (...)
    {
        return false;
    }

    if (loadedAnySplitFile)
    {
        if (outAsset.modelGuid.empty())
            outAsset.modelGuid = modelGuid;

        return true;
    }

    const fs::path filePath = Get_ModelNotifyFilePath(modelGuid);
    if (Load_FromFile(filePath.wstring(), outAsset))
        return true;

    const fs::path legacyFilePath = Get_LegacyModelNotifyFilePath(modelGuid);
    return Load_FromFile(legacyFilePath.wstring(), outAsset);
}

bool AnimNotify_Parser::Save_AnimationToSplitFile(const string& modelGuid, const AnimNotifyData& animationData, float animationLengthSec)
{
    if (modelGuid.empty() || animationData.animationName.empty())
        return false;

    AnimNotifyData animationCopy = animationData;
    Sort_And_Clamp(animationCopy, animationLengthSec);

    AnimNotifyAsset singleAnimationAsset{};
    singleAnimationAsset.version = 2;
    singleAnimationAsset.modelGuid = modelGuid;
    singleAnimationAsset.animations.push_back(::move(animationCopy));

    const fs::path filePath = Get_AnimationNotifyFilePath(modelGuid, singleAnimationAsset.animations.front().animationName);
    return Save_ToFile(filePath.wstring(), singleAnimationAsset);
}

fs::path AnimNotify_Parser::Get_BaseFolderPath()
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    return fs::path(assetRoot) / L"Data" / L"json" / L"AnimNotifies";
}

fs::path AnimNotify_Parser::Get_ModelNotifyFilePath(const string& modelGuid)
{
    return Get_BaseFolderPath() / (Get_ModelNotifyStem(modelGuid) + ".notify.json");
}

fs::path AnimNotify_Parser::Get_ModelNotifyFolderPath(const string& modelGuid)
{
    return Get_BaseFolderPath() / Get_ModelNotifyStem(modelGuid);
}

fs::path AnimNotify_Parser::Get_AnimationNotifyFilePath(const string& modelGuid, const string& animationName)
{
    return Get_ModelNotifyFolderPath(modelGuid) / (Sanitize_FileName(animationName) + ".notify.json");
}

fs::path AnimNotify_Parser::Get_LegacyModelNotifyFilePath(const string& modelGuid)
{
    const wstring resolvedPath = GAME->Resolve_AssetPath(modelGuid);
    string fileStem = modelGuid;

    if (!resolvedPath.empty())
        fileStem = fs::path(resolvedPath).stem().string();

    return Get_BaseFolderPath() / (fileStem + "_" + modelGuid + ".animnotify.json");
}

AnimNotifyData* AnimNotify_Parser::Find_Animation(AnimNotifyAsset& asset, const string& animationName)
{
    for (auto& animationData : asset.animations)
    {
        if (animationData.animationName == animationName)
            return &animationData;
    }

    return nullptr;
}

const AnimNotifyData* AnimNotify_Parser::Find_Animation(const AnimNotifyAsset& asset, const string& animationName)
{
    for (const auto& animationData : asset.animations)
    {
        if (animationData.animationName == animationName)
            return &animationData;
    }

    return nullptr;
}

AnimNotifyData& AnimNotify_Parser::Get_OrAddAnimation(AnimNotifyAsset& asset, const string& animationName)
{
    if (AnimNotifyData* found = Find_Animation(asset, animationName))
        return *found;

    AnimNotifyData animationData{};
    animationData.animationName = animationName;
    Ensure_DefaultTracks(animationData);

    asset.animations.push_back(::move(animationData));
    return asset.animations.back();
}

void AnimNotify_Parser::Ensure_DefaultTracks(AnimNotifyData& animationData)
{
    if (animationData.notifyTracks.empty())
        animationData.notifyTracks.push_back({ "Notify" });

    if (animationData.notifyStateTracks.empty())
        animationData.notifyStateTracks.push_back({ "Notify State" });

    animationData.displayFps = max(1, animationData.displayFps);
    animationData.nextNotifyId = max(1, animationData.nextNotifyId);
    animationData.nextStateId = max(1, animationData.nextStateId);
}

void AnimNotify_Parser::Sort_And_Clamp(AnimNotifyData& animationData, float animationLengthSec)
{
    Ensure_DefaultTracks(animationData);

    for (auto& entry : animationData.notifies)
    {
        if (entry.id <= 0)
            entry.id = animationData.nextNotifyId++;

        if (animationData.notifyTracks.empty())
            entry.trackIndex = 0;
        else
            entry.trackIndex = ::clamp(entry.trackIndex, 0, static_cast<int32>(animationData.notifyTracks.size()) - 1);

        entry.timeSec = Clamp_TimeSec(entry.timeSec, animationLengthSec);
    }

    for (auto& entry : animationData.notifyStates)
    {
        if (entry.id <= 0)
            entry.id = animationData.nextStateId++;

        if (animationData.notifyStateTracks.empty())
            entry.trackIndex = 0;
        else
            entry.trackIndex = ::clamp(entry.trackIndex, 0, static_cast<int32>(animationData.notifyStateTracks.size()) - 1);

        entry.startSec = Clamp_TimeSec(entry.startSec, animationLengthSec);
        entry.durationSec = max(0.f, entry.durationSec);

        if (animationLengthSec >= 0.f)
            entry.durationSec = min(entry.durationSec, max(0.f, animationLengthSec - entry.startSec));
    }

    ::sort(
        animationData.notifies.begin(),
        animationData.notifies.end(),
        [](const AnimNotifyEventEntry& lhs, const AnimNotifyEventEntry& rhs)
        {
            if (lhs.timeSec == rhs.timeSec)
                return lhs.id < rhs.id;

            return lhs.timeSec < rhs.timeSec;
        });

    ::sort(
        animationData.notifyStates.begin(),
        animationData.notifyStates.end(),
        [](const AnimNotifyStateEntry& lhs, const AnimNotifyStateEntry& rhs)
        {
            if (lhs.startSec == rhs.startSec)
                return lhs.id < rhs.id;

            return lhs.startSec < rhs.startSec;
        });

    animationData.nextNotifyId = Compute_NextNotifyId(animationData);
    animationData.nextStateId = Compute_NextStateId(animationData);
}

float AnimNotify_Parser::Clamp_TimeSec(float value, float animationLengthSec)
{
    if (animationLengthSec < 0.f)
        return max(0.f, value);

    return ::clamp(value, 0.f, animationLengthSec);
}

int32 AnimNotify_Parser::Compute_NextNotifyId(const AnimNotifyData& anim)
{
    int32 maxId = 0;

    for (const auto& entry : anim.notifies)
        maxId = max(maxId, entry.id);

    return maxId + 1;
}

string AnimNotify_Parser::Get_ModelNotifyStem(const string& modelGuid)
{
    const wstring resolvedPath = GAME->Resolve_AssetPath(modelGuid);

    if (!resolvedPath.empty())
        return fs::path(resolvedPath).stem().string();

    return modelGuid;
}

string AnimNotify_Parser::Sanitize_FileName(const string& value)
{
    string result = value;

    for (char& ch : result)
    {
        switch (ch)
        {
        case '<':
        case '>':
        case ':':
        case '"':
        case '/':
        case '\\':
        case '|':
        case '?':
        case '*':
            ch = '_';
            break;
        default:
            break;
        }
    }

    if (result.empty())
        result = "Animation";

    return result;
}

void AnimNotify_Parser::Merge_AnimationIntoAsset(AnimNotifyAsset& asset, AnimNotifyData&& animationData)
{
    if (animationData.animationName.empty())
        return;

    Sort_And_Clamp(animationData);

    if (AnimNotifyData* existing = Find_Animation(asset, animationData.animationName))
    {
        *existing = ::move(animationData);
        return;
    }

    asset.animations.push_back(::move(animationData));
}

int32 AnimNotify_Parser::Compute_NextStateId(const AnimNotifyData& anim)
{
    int32 maxId = 0;

    for (const auto& entry : anim.notifyStates)
        maxId = max(maxId, entry.id);

    return maxId + 1;
}
