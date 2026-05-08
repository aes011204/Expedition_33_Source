#include "AnimNotify_Parser.h"
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
            animationNode["notifies"].push_back({
                { "id", entry.id },
                { "time_sec", entry.timeSec },
                { "track_index", entry.trackIndex },
                { "name", entry.name }
            });
        }

        animationNode["notify_states"] = json::array();
        for (const auto& entry : animationData.notifyStates)
        {
            animationNode["notify_states"].push_back({
                { "id", entry.id },
                { "start_sec", entry.startSec },
                { "duration_sec", entry.durationSec },
                { "track_index", entry.trackIndex },
                { "name", entry.name }
            });
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
    outAsset.version = root.value("version", 1);
    outAsset.modelGuid = root.value("model_guid", string{});

    for (const auto& animationNode : root.value("animations", json::array()))
    {
        AnimNotifyData animationData{};
        animationData.animationName = animationNode.value("animation_name", string{});
        animationData.displayFps = max(1, animationNode.value("display_fps", 60));
        animationData.nextNotifyId = max(1, animationNode.value("next_notify_id", 1));
        animationData.nextStateId = max(1, animationNode.value("next_state_id", 1));

        for (const auto& trackNameNode : animationNode.value("notify_tracks", json::array()))
            animationData.notifyTracks.push_back({ trackNameNode.get<string>() });

        for (const auto& trackNameNode : animationNode.value("notify_state_tracks", json::array()))
            animationData.notifyStateTracks.push_back({ trackNameNode.get<string>() });

        for (const auto& entryNode : animationNode.value("notifies", json::array()))
        {
            AnimNotifyEventEntry entry{};
            entry.id = entryNode.value("id", 0);
            entry.timeSec = entryNode.value("time_sec", 0.f);
            entry.trackIndex = entryNode.value("track_index", 0);
            entry.name = entryNode.value("name", string{});
            animationData.notifies.push_back(::move(entry));
        }

        for (const auto& entryNode : animationNode.value("notify_states", json::array()))
        {
            AnimNotifyStateEntry entry{};
            entry.id = entryNode.value("id", 0);
            entry.startSec = entryNode.value("start_sec", 0.f);
            entry.durationSec = entryNode.value("duration_sec", 0.f);
            entry.trackIndex = entryNode.value("track_index", 0);
            entry.name = entryNode.value("name", string{});
            animationData.notifyStates.push_back(::move(entry));
        }

        Ensure_DefaultTracks(animationData);
        Sort_And_Clamp(animationData);
        outAsset.animations.push_back(::move(animationData));
    }

    return !outAsset.modelGuid.empty();
}

bool AnimNotify_Parser::Save_ToFile(const wstring& filePath, const AnimNotifyAsset& asset)
{
    try
    {
        fs::create_directories(fs::path(filePath).parent_path());

        ::ofstream file(filePath);
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
        if (!fs::exists(filePath))
            return false;

        ::ifstream file(filePath);
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

fs::path AnimNotify_Parser::Get_BaseFolderPath()
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    return fs::path(assetRoot) / L"Data" / L"json" / L"AnimNotifies";
}

fs::path AnimNotify_Parser::Get_ModelNotifyFilePath(const string& modelGuid)
{
    wstring resolvedPath = GAME->Resolve_AssetPath(modelGuid);
    string fileStem = modelGuid;

    if (!resolvedPath.empty())
    {
        fileStem = fs::path(resolvedPath).stem().string();
        return Get_BaseFolderPath() / (fileStem + "_" + modelGuid + ".animnotify.json");
    }

    return Get_BaseFolderPath() / (fileStem + ".animnotify.json");
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
        animationData.notifyTracks.push_back({ "Notifies" });

    if (animationData.notifyStateTracks.empty())
        animationData.notifyStateTracks.push_back({ "Notify States" });

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

        entry.trackIndex = 0;
        entry.timeSec = Clamp_TimeSec(entry.timeSec, animationLengthSec);
    }

    for (auto& entry : animationData.notifyStates)
    {
        if (entry.id <= 0)
            entry.id = animationData.nextStateId++;

        entry.trackIndex = 0;
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

int32 AnimNotify_Parser::Compute_NextStateId(const AnimNotifyData& anim)
{
    int32 maxId = 0;

    for (const auto& entry : anim.notifyStates)
        maxId = max(maxId, entry.id);

    return maxId + 1;
}
