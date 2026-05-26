#include "BattleCameraClipTypes.h"
#include "BattleCameraClipIO.h"

BattleCameraClipIO::BattleCameraClipIO()
{
}

BattleCameraClipIO::~BattleCameraClipIO()
{
}

namespace
{
string Build_BattleCameraClipFilePath(BattleCameraMode cameraMode, const string& skillId)
{
    const string modeName = string(magic_enum::enum_name(cameraMode));
    string fileName = "BattleCameraClip_" + modeName;

    if (!skillId.empty())
        fileName += "_" + skillId;

    return "../../../Client/Bin/Resources/Data/json/CameraPreset/" + fileName + ".json";
}

json Serialize_BattleCameraTrack(const BattleCameraTrack& track)
{
    json keyframes = json::array();
    for (const BattleCameraKeyframe& keyframe : track.keyframes)
    {
        json keyJson{};
        keyJson["time"] = keyframe.time;
        keyJson["value"] = keyframe.value;
        keyframes.push_back(keyJson);
    }

    return keyframes;
}

void Deserialize_BattleCameraTrack(const json& jsonData, const char* keyName, BattleCameraTrack& outTrack)
{
    if (!jsonData.contains(keyName))
        return;

    const json& keyframes = jsonData[keyName];
    if (!keyframes.is_array())
        return;

    for (const auto& keyframe : keyframes)
    {
        BattleCameraKeyframe key{};
        key.time = keyframe.value("time", key.time);
        key.value = keyframe.value("value", key.value);
        outTrack.keyframes.push_back(key);
    }
}
}

void BattleCameraClipIO::SaveBattleCameraClipToFile(BattleCameraMode cameraMode, const BattleCameraClip& clip, const string& skillId)
{
    const string filePath = Build_BattleCameraClipFilePath(cameraMode, skillId);

    json jsonData {};

    jsonData["duration"] = clip.duration;

    jsonData["distanceOffset"] = Serialize_BattleCameraTrack(clip.distanceOffset);
    jsonData["fovOffset"] = Serialize_BattleCameraTrack(clip.fovOffset);
    jsonData["sideOffset"] = Serialize_BattleCameraTrack(clip.sideOffset);
    jsonData["pivotOffset"] = Serialize_BattleCameraTrack(clip.pivotOffset);
    jsonData["yawOffset"] = Serialize_BattleCameraTrack(clip.yawOffset);
    jsonData["shakeAmplitude"] = Serialize_BattleCameraTrack(clip.shakeAmplitude);

    
    const fs::path savePath = filePath;
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open battle camera clip save file.");
        return;
    }

    file << jsonData.dump(4);
}


bool BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode cameraMode, BattleCameraClip& clip, const string& skillId)
{
    const string filePath = Build_BattleCameraClipFilePath(cameraMode, skillId);

    ifstream file(filePath);
    if(!file.is_open())
        return false;

    json jsonData {};

    file >> jsonData;

    BattleCameraClip loadedClip {};

    loadedClip.duration = jsonData.value("duration", clip.duration);

    Deserialize_BattleCameraTrack(jsonData, "distanceOffset", loadedClip.distanceOffset);
    Deserialize_BattleCameraTrack(jsonData, "fovOffset", loadedClip.fovOffset);
    Deserialize_BattleCameraTrack(jsonData, "sideOffset", loadedClip.sideOffset);
    Deserialize_BattleCameraTrack(jsonData, "pivotOffset", loadedClip.pivotOffset);
    Deserialize_BattleCameraTrack(jsonData, "yawOffset", loadedClip.yawOffset);
    Deserialize_BattleCameraTrack(jsonData, "shakeAmplitude", loadedClip.shakeAmplitude);

    clip = loadedClip;
    return true;
}
