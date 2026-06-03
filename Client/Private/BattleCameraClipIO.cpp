#include "BattleCameraClipTypes.h"
#include "BattleCameraClipIO.h"
#include <algorithm>

BattleCameraClipIO::BattleCameraClipIO()
{
}

BattleCameraClipIO::~BattleCameraClipIO()
{
}

namespace
{
BattleCameraClipTimeMode Get_DefaultBattleCameraClipTimeMode(BattleCameraMode cameraMode)
{
    return cameraMode == BattleCameraMode::BATTLEINTRO ||
        cameraMode == BattleCameraMode::SELECTSKILL
        ? BattleCameraClipTimeMode::Independent
        : BattleCameraClipTimeMode::AnimationSync;
}

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
        keyJson["interpolation"] = static_cast<int>(keyframe.interpolation);
        keyframes.push_back(keyJson);
    }

    return keyframes;
}

void Sort_BattleCameraTrack(BattleCameraTrack& track)
{
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const BattleCameraKeyframe& lhs, const BattleCameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });
}

void Normalize_BattleCameraTrack(BattleCameraTrack& track, float durationSec)
{
    if (durationSec <= 0.f)
        return;

    for (BattleCameraKeyframe& keyframe : track.keyframes)
        keyframe.time = std::clamp(keyframe.time / durationSec, 0.f, 1.f);

    Sort_BattleCameraTrack(track);
}

void Normalize_AnimationSyncClip(BattleCameraClip& clip)
{
    const float legacyDurationSec = clip.duration;
    if (legacyDurationSec <= 1.f)
    {
        clip.duration = 1.f;
        return;
    }

    Normalize_BattleCameraTrack(clip.distanceOffset, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.fovOffset, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.sideOffset, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.pivotOffset, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.heightOffset, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.lookHeightOffset, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.yawOffset, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.shakeAmplitude, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.orbitYaw, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.orbitRadius, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.orbitHeight, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.orbitLookHeight, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.shakeFrequencyX, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.shakeFrequencyY, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.shakeDecayRateX, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.shakeDecayRate, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.shakeVerticalRatio, legacyDurationSec);
    Normalize_BattleCameraTrack(clip.shakeStrengthScale, legacyDurationSec);
    clip.duration = 1.f;
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
        const int interpolationValue = keyframe.value("interpolation", static_cast<int>(key.interpolation));
        switch (interpolationValue)
        {
        case static_cast<int>(InterpolationMode::Step):
            key.interpolation = InterpolationMode::Step;
            break;
        case static_cast<int>(InterpolationMode::EaseInOut):
            key.interpolation = InterpolationMode::EaseInOut;
            break;
        case static_cast<int>(InterpolationMode::EaseIn):
            key.interpolation = InterpolationMode::EaseIn;
            break;
        case static_cast<int>(InterpolationMode::EaseOut):
            key.interpolation = InterpolationMode::EaseOut;
            break;
        case static_cast<int>(InterpolationMode::Linear):
        default:
            key.interpolation = InterpolationMode::Linear;
            break;
        }
        outTrack.keyframes.push_back(key);
    }

    Sort_BattleCameraTrack(outTrack);
}
}

void BattleCameraClipIO::SaveBattleCameraClipToFile(BattleCameraMode cameraMode, const BattleCameraClip& clip, const string& skillId)
{
    const string filePath = Build_BattleCameraClipFilePath(cameraMode, skillId);

    json jsonData {};

    jsonData["duration"] = clip.timeMode == BattleCameraClipTimeMode::AnimationSync ? 1.f : clip.duration;
    jsonData["timeMode"] = string(magic_enum::enum_name(clip.timeMode));

    jsonData["distanceOffset"] = Serialize_BattleCameraTrack(clip.distanceOffset);
    jsonData["fovOffset"] = Serialize_BattleCameraTrack(clip.fovOffset);
    jsonData["sideOffset"] = Serialize_BattleCameraTrack(clip.sideOffset);
    jsonData["pivotOffset"] = Serialize_BattleCameraTrack(clip.pivotOffset);
    jsonData["heightOffset"] = Serialize_BattleCameraTrack(clip.heightOffset);
    jsonData["lookHeightOffset"] = Serialize_BattleCameraTrack(clip.lookHeightOffset);
    jsonData["yawOffset"] = Serialize_BattleCameraTrack(clip.yawOffset);
    jsonData["shakeAmplitude"] = Serialize_BattleCameraTrack(clip.shakeAmplitude);
    jsonData["orbitYaw"] = Serialize_BattleCameraTrack(clip.orbitYaw);
    jsonData["orbitRadius"] = Serialize_BattleCameraTrack(clip.orbitRadius);
    jsonData["orbitHeight"] = Serialize_BattleCameraTrack(clip.orbitHeight);
    jsonData["orbitLookHeight"] = Serialize_BattleCameraTrack(clip.orbitLookHeight);
    jsonData["shakeFrequencyX"] = Serialize_BattleCameraTrack(clip.shakeFrequencyX);
    jsonData["shakeFrequencyY"] = Serialize_BattleCameraTrack(clip.shakeFrequencyY);
    jsonData["shakeDecayRateX"] = Serialize_BattleCameraTrack(clip.shakeDecayRateX);
    jsonData["shakeDecayRate"] = Serialize_BattleCameraTrack(clip.shakeDecayRate);
    jsonData["shakeVerticalRatio"] = Serialize_BattleCameraTrack(clip.shakeVerticalRatio);
    jsonData["shakeStrengthScale"] = Serialize_BattleCameraTrack(clip.shakeStrengthScale);

    
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
    loadedClip.timeMode = Get_DefaultBattleCameraClipTimeMode(cameraMode);

    loadedClip.duration = jsonData.value("duration", clip.duration);
    if (const string timeModeName = jsonData.value("timeMode", string{}); !timeModeName.empty())
    {
        if (const auto parsedTimeMode = magic_enum::enum_cast<BattleCameraClipTimeMode>(timeModeName); parsedTimeMode.has_value())
            loadedClip.timeMode = parsedTimeMode.value();
    }

    Deserialize_BattleCameraTrack(jsonData, "distanceOffset", loadedClip.distanceOffset);
    Deserialize_BattleCameraTrack(jsonData, "fovOffset", loadedClip.fovOffset);
    Deserialize_BattleCameraTrack(jsonData, "sideOffset", loadedClip.sideOffset);
    Deserialize_BattleCameraTrack(jsonData, "pivotOffset", loadedClip.pivotOffset);
    Deserialize_BattleCameraTrack(jsonData, "heightOffset", loadedClip.heightOffset);
    Deserialize_BattleCameraTrack(jsonData, "lookHeightOffset", loadedClip.lookHeightOffset);
    Deserialize_BattleCameraTrack(jsonData, "yawOffset", loadedClip.yawOffset);
    Deserialize_BattleCameraTrack(jsonData, "shakeAmplitude", loadedClip.shakeAmplitude);
    Deserialize_BattleCameraTrack(jsonData, "orbitYaw", loadedClip.orbitYaw);
    Deserialize_BattleCameraTrack(jsonData, "orbitRadius", loadedClip.orbitRadius);
    Deserialize_BattleCameraTrack(jsonData, "orbitHeight", loadedClip.orbitHeight);
    Deserialize_BattleCameraTrack(jsonData, "orbitLookHeight", loadedClip.orbitLookHeight);
    Deserialize_BattleCameraTrack(jsonData, "shakeFrequencyX", loadedClip.shakeFrequencyX);
    Deserialize_BattleCameraTrack(jsonData, "shakeFrequencyY", loadedClip.shakeFrequencyY);
    Deserialize_BattleCameraTrack(jsonData, "shakeDecayRateX", loadedClip.shakeDecayRateX);
    Deserialize_BattleCameraTrack(jsonData, "shakeDecayRate", loadedClip.shakeDecayRate);
    Deserialize_BattleCameraTrack(jsonData, "shakeVerticalRatio", loadedClip.shakeVerticalRatio);
    Deserialize_BattleCameraTrack(jsonData, "shakeStrengthScale", loadedClip.shakeStrengthScale);

    if (loadedClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
        Normalize_AnimationSyncClip(loadedClip);

    clip = loadedClip;
    return true;
}
