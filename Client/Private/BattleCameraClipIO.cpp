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
}

void BattleCameraClipIO::SaveBattleCameraClipToFile(BattleCameraMode cameraMode, const BattleCameraClip& clip, const string& skillId)
{
    const string filePath = Build_BattleCameraClipFilePath(cameraMode, skillId);

    json jsonData {};

    jsonData["duration"] = clip.duration;

    json distanceKeys= json::array();

    for(auto&  keyframe : clip.distanceOffset.keyframes)
    {
        json KeyJson {};

        KeyJson["time"] = keyframe.time;
        KeyJson["value"] = keyframe.value;

        distanceKeys.push_back(KeyJson);
    }

    jsonData["distanceOffset"] = distanceKeys;

    json fovKeys = json::array();

    for(auto& keyframe :clip.fovOffset.keyframes)
    {
        json KeyJson {};

        KeyJson["time"] = keyframe.time;
        KeyJson["value"] = keyframe.value;

        fovKeys.push_back(KeyJson);
    }

    jsonData["fovOffset"] = fovKeys;

    json shakeAmpKeys {};

    for(auto& keyframe :clip.shakeAmplitude.keyframes)
    {
        json KeyJson {};

        KeyJson["time"] = keyframe.time;
        KeyJson["value"] = keyframe.value;

        shakeAmpKeys.push_back(KeyJson);
    }
    jsonData["shakeAmplitude"] = shakeAmpKeys;

    
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

    const json& keyframes = jsonData["distanceOffset"];

    if(keyframes.is_array())
    {
        for (const auto& keyframe : keyframes)
        {
            BattleCameraKeyframe key {};
            key.time = keyframe.value("time", key.time);
            key.value = keyframe.value("value",key.value);

            loadedClip.distanceOffset.keyframes.push_back(key);
        }
    }

    const json& fovOffsetKeyFrames = jsonData["fovOffset"];

    if(fovOffsetKeyFrames.is_array())
    {
        for (const auto& keyframe : fovOffsetKeyFrames)
        {
            BattleCameraKeyframe key {};
            key.time = keyframe.value("time", key.time);
            key.value = keyframe.value("value",key.value);

            loadedClip.fovOffset.keyframes.push_back(key);
        }
    
    }

    const json& shakeAmplitudeKeyFrames = jsonData["shakeAmplitude"];

    if(shakeAmplitudeKeyFrames.is_array())
    {
        for (const auto& keyframe : shakeAmplitudeKeyFrames)
        {
            BattleCameraKeyframe key {};
            key.time = keyframe.value("time", key.time);
            key.value = keyframe.value("value",key.value);

            loadedClip.shakeAmplitude.keyframes.push_back(key);
        }
    }

    clip = loadedClip;
    return true;
}
