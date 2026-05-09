#include "Cinematic_SequenceIO.h"

namespace
{
Vec3 ParseVec3FromJson(const json& vecJson, const Vec3& defaultValue = Vec3::Zero)
{
    if (!vecJson.is_object())
        return defaultValue;

    Vec3 value = defaultValue;
    value.x = vecJson.value("x", defaultValue.x);
    value.y = vecJson.value("y", defaultValue.y);
    value.z = vecJson.value("z", defaultValue.z);
    return value;
}
}

Cinematic_SequenceIO::Cinematic_SequenceIO()
{
}

void Cinematic_SequenceIO::SaveSequenceToFile(const CINEMATIC_SEQUENCE& sequence, const string& filePath)
{
    json jsonData{};

    jsonData["name"] = sequence.name;
    jsonData["startFrame"] = sequence.startFrame;
    jsonData["endFrame"] = sequence.endFrame;
    jsonData["fps"] = sequence.fps;
    jsonData["cinematicTracks"] = json::array();

    for (const auto& cinematicTrack : sequence.cinematicTracks)
    {
        json track{};
        track["name"] = cinematicTrack.name;
        track["enabled"] = cinematicTrack.enabled;
        track["keyframes"] = json::array();

        for (const auto& cinematicKeyframe : cinematicTrack.keyframes)
        {
            json keyframe{};
            keyframe["frame"] = cinematicKeyframe.frame;

            keyframe["position"] =
            {
                { "x", cinematicKeyframe.camPose.position.x },
                { "y", cinematicKeyframe.camPose.position.y },
                { "z", cinematicKeyframe.camPose.position.z }
            };

            keyframe["rotation"] =
            {
                { "x", cinematicKeyframe.camPose.rotation.x },
                { "y", cinematicKeyframe.camPose.rotation.y },
                { "z", cinematicKeyframe.camPose.rotation.z }
            };

            keyframe["fovY"] = cinematicKeyframe.camPose.fovY;
            keyframe["nearPlane"] = cinematicKeyframe.camPose.nearPlane;
            keyframe["farPlane"] = cinematicKeyframe.camPose.farPlane;
            keyframe["interpolation"] = static_cast<int>(cinematicKeyframe.interpolation);

            track["keyframes"].push_back(keyframe);
        }

        jsonData["cinematicTracks"].push_back(track);
    }

    const fs::path savePath = filePath;
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open cinematic sequence save file.");
        return;
    }

    file << jsonData.dump(4);
}

bool Cinematic_SequenceIO::LoadSequenceFromFile(const string& filePath, CINEMATIC_SEQUENCE& outSequence)
{
    ifstream file(filePath);
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    CINEMATIC_SEQUENCE loadedSequence{};
    loadedSequence.name = jsonData.value("name", loadedSequence.name);
    loadedSequence.fps = jsonData.value("fps", loadedSequence.fps);
    loadedSequence.startFrame = jsonData.value("startFrame", loadedSequence.startFrame);
    loadedSequence.endFrame = jsonData.value("endFrame", loadedSequence.endFrame);
    loadedSequence.cinematicTracks.clear();

    const json& tracksJson = jsonData["cinematicTracks"];
    if (tracksJson.is_array())
    {
        for (const auto& trackJson : tracksJson)
        {
            CINEMATIC_TRACK track{};
            track.name = trackJson.value("name", track.name);
            track.enabled = trackJson.value("enabled", track.enabled);
            track.keyframes.clear();

            const json& keyframesJson = trackJson["keyframes"];
            if (keyframesJson.is_array())
            {
                for (const auto& keyframeJson : keyframesJson)
                {
                    CINEMATIC_KEYFRAME keyframe{};
                    keyframe.frame = keyframeJson.value("frame", keyframe.frame);
                    keyframe.camPose.position = ParseVec3FromJson(keyframeJson["position"], keyframe.camPose.position);
                    keyframe.camPose.rotation = ParseVec3FromJson(keyframeJson["rotation"], keyframe.camPose.rotation);
                    keyframe.camPose.fovY = keyframeJson.value("fovY", keyframe.camPose.fovY);
                    keyframe.camPose.nearPlane = keyframeJson.value("nearPlane", keyframe.camPose.nearPlane);
                    keyframe.camPose.farPlane = keyframeJson.value("farPlane", keyframe.camPose.farPlane);

                    const int interpolationValue = keyframeJson.value("interpolation", static_cast<int>(keyframe.interpolation));
                    keyframe.interpolation = interpolationValue == static_cast<int>(InterpolationMode::Step)
                        ? InterpolationMode::Step
                        : InterpolationMode::Linear;

                    track.keyframes.push_back(keyframe);
                }
            }

            std::sort(track.keyframes.begin(), track.keyframes.end(),
                [](const CINEMATIC_KEYFRAME& lhs, const CINEMATIC_KEYFRAME& rhs)
                {
                    return lhs.frame < rhs.frame;
                });

            loadedSequence.cinematicTracks.push_back(track);
        }
    }

    outSequence = loadedSequence;
    return true;
}
