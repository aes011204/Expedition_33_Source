#include "Cinematic_SequenceIO.h"
#include "Helper_String.h"

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

json Serialize_CinematicModifierTrack(const CameraTrack& track)
{
    json trackJson = json::array();

    for (const auto& keyframe : track.keyframes)
    {
        json keyframeJson{};
        keyframeJson["time"] = keyframe.time;
        keyframeJson["value"] = keyframe.value;
        keyframeJson["interpolation"] = static_cast<int>(keyframe.interpolation);
        trackJson.push_back(keyframeJson);
    }

    return trackJson;
}

void Deserialize_CinematicModifierTrack(const json& trackJson, CameraTrack& outTrack)
{
    outTrack.keyframes.clear();

    if (!trackJson.is_array())
        return;

    for (const auto& keyframeJson : trackJson)
    {
        CameraKeyframe keyframe{};
        keyframe.time = keyframeJson.value("time", keyframe.time);
        keyframe.value = keyframeJson.value("value", keyframe.value);
        keyframe.interpolation = static_cast<InterpolationMode>(
            keyframeJson.value("interpolation", static_cast<int>(keyframe.interpolation)));
        outTrack.keyframes.push_back(keyframe);
    }

    std::sort(outTrack.keyframes.begin(), outTrack.keyframes.end(),
        [](const CameraKeyframe& lhs, const CameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });
}

const char* ToString(CinematicCleanupPolicy policy)
{
    switch (policy)
    {
    case CinematicCleanupPolicy::DespawnOnStop:
        return "despawn_on_stop";

    case CinematicCleanupPolicy::Keep:
        return "keep";

    default:
        return "despawn_on_stop";
    }
}

CinematicCleanupPolicy ParseCleanupPolicy(const json& value)
{
    const string text = value.is_string() ? value.get<string>() : "despawn_on_stop";
    if (text == "keep")
        return CinematicCleanupPolicy::Keep;

    return CinematicCleanupPolicy::DespawnOnStop;
}

void SerializeSpawnData(json& targetJson, const CinematicSpawnEventData& payload)
{
    targetJson["prefabName"] = payload.prefabName;
    targetJson["layerTag"] = String::ToString(payload.layerTag);
    targetJson["position"] =
    {
        { "x", payload.position.x },
        { "y", payload.position.y },
        { "z", payload.position.z }
    };
    targetJson["rotation"] =
    {
        { "x", payload.rotation.x },
        { "y", payload.rotation.y },
        { "z", payload.rotation.z }
    };
    targetJson["scale"] =
    {
        { "x", payload.scale.x },
        { "y", payload.scale.y },
        { "z", payload.scale.z }
    };
    targetJson["cleanupPolicy"] = ToString(payload.cleanupPolicy);
}

void DeserializeSpawnData(const json& sourceJson, CinematicSpawnEventData& outPayload)
{
    outPayload.prefabName = sourceJson.value("prefabName", outPayload.prefabName);
    outPayload.layerTag = String::ToWString(sourceJson.value("layerTag", String::ToString(outPayload.layerTag)));
    outPayload.position = ParseVec3FromJson(sourceJson["position"], outPayload.position);
    outPayload.rotation = ParseVec3FromJson(sourceJson["rotation"], outPayload.rotation);
    outPayload.scale = ParseVec3FromJson(sourceJson["scale"], outPayload.scale);
    outPayload.cleanupPolicy = ParseCleanupPolicy(sourceJson["cleanupPolicy"]);
}

json SerializeActorTransformKeyframe(const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& keyframe)
{
    return
    {
        { "frame", keyframe.frame },
        { "position", {
            { "x", keyframe.position.x },
            { "y", keyframe.position.y },
            { "z", keyframe.position.z } } },
        { "rotation", {
            { "x", keyframe.rotation.x },
            { "y", keyframe.rotation.y },
            { "z", keyframe.rotation.z } } },
        { "interpolation", static_cast<int>(keyframe.interpolation) }
    };
}

void DeserializeActorTransformKeyframe(const json& keyframeJson, CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& outKeyframe)
{
    outKeyframe.frame = keyframeJson.value("frame", outKeyframe.frame);
    outKeyframe.position = ParseVec3FromJson(keyframeJson["position"], outKeyframe.position);
    outKeyframe.rotation = ParseVec3FromJson(keyframeJson["rotation"], outKeyframe.rotation);
    outKeyframe.interpolation = static_cast<InterpolationMode>(
        keyframeJson.value("interpolation", static_cast<int>(outKeyframe.interpolation)));
}

json SerializeActorTransformTrack(const CINEMATIC_ACTOR_TRANSFORM_TRACK& track)
{
    json trackJson{};
    trackJson["targetId"] = track.targetId;
    trackJson["name"] = track.name;
    trackJson["enabled"] = track.enabled;
    trackJson["keyframes"] = json::array();

    for (const auto& keyframe : track.keyframes)
        trackJson["keyframes"].push_back(SerializeActorTransformKeyframe(keyframe));

    return trackJson;
}

void DeserializeActorTransformTrack(const json& trackJson, CINEMATIC_ACTOR_TRANSFORM_TRACK& outTrack)
{
    outTrack.targetId = trackJson.value("targetId", outTrack.targetId);
    outTrack.name = trackJson.value("name", outTrack.name);
    outTrack.enabled = trackJson.value("enabled", outTrack.enabled);
    outTrack.keyframes.clear();

    const json& keyframesJson = trackJson["keyframes"];
    if (!keyframesJson.is_array())
        return;

    for (const auto& keyframeJson : keyframesJson)
    {
        CINEMATIC_ACTOR_TRANSFORM_KEYFRAME keyframe{};
        DeserializeActorTransformKeyframe(keyframeJson, keyframe);
        outTrack.keyframes.push_back(keyframe);
    }

    std::sort(outTrack.keyframes.begin(), outTrack.keyframes.end(),
        [](const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& lhs, const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& rhs)
        {
            return lhs.frame < rhs.frame;
        });
}

json SerializeEvent(const CINEMATIC_EVENT& event)
{
    json eventJson{};
    eventJson["frame"] = event.frame;
    eventJson["targetId"] = event.targetId;

    if (get_if<CinematicSpawnEventData>(&event.payload) != nullptr)
    {
        eventJson["type"] = "spawn";
        SerializeSpawnData(eventJson, get<CinematicSpawnEventData>(event.payload));
    }
    else if (const auto* playAnim = get_if<CinematicPlayAnimEventData>(&event.payload))
    {
        eventJson["type"] = "play_anim";
        eventJson["animName"] = playAnim->animName;
        eventJson["blendDuration"] = playAnim->blendDuration;
        eventJson["loop"] = playAnim->loop;
        eventJson["startFrame"] = playAnim->startFrame;
    }
    else if (get_if<CinematicDespawnEventData>(&event.payload) != nullptr)
    {
        eventJson["type"] = "despawn";
    }

    return eventJson;
}

void DeserializeEvent(const json& eventJson, CINEMATIC_EVENT& outEvent)
{
    outEvent.frame = eventJson.value("frame", outEvent.frame);
    outEvent.targetId = eventJson.value("targetId", outEvent.targetId);

    const string type = eventJson.value("type", "spawn");
    if (type == "play_anim")
    {
        CinematicPlayAnimEventData payload{};
        payload.animName = eventJson.value("animName", payload.animName);
        payload.blendDuration = eventJson.value("blendDuration", payload.blendDuration);
        payload.loop = eventJson.value("loop", payload.loop);
        payload.startFrame = eventJson.value("startFrame", payload.startFrame);
        outEvent.payload = payload;
    }
    else if (type == "despawn")
    {
        outEvent.payload = CinematicDespawnEventData{};
    }
    else
    {
        CinematicSpawnEventData payload{};
        DeserializeSpawnData(eventJson, payload);
        outEvent.payload = payload;
    }
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
    jsonData["eventTrack"] = json::array();
    jsonData["actorTransformTracks"] = json::array();
    jsonData["modifierClip"] =
    {
        { "shakeAmplitude", Serialize_CinematicModifierTrack(sequence.modifierClip.shakeAmplitude) },
        { "shakeFrequencyX", Serialize_CinematicModifierTrack(sequence.modifierClip.shakeFrequencyX) },
        { "shakeFrequencyY", Serialize_CinematicModifierTrack(sequence.modifierClip.shakeFrequencyY) },
        { "shakeDecayRateX", Serialize_CinematicModifierTrack(sequence.modifierClip.shakeDecayRateX) },
        { "shakeDecayRate", Serialize_CinematicModifierTrack(sequence.modifierClip.shakeDecayRate) },
        { "shakeVerticalRatio", Serialize_CinematicModifierTrack(sequence.modifierClip.shakeVerticalRatio) },
        { "shakeStrengthScale", Serialize_CinematicModifierTrack(sequence.modifierClip.shakeStrengthScale) }
    };

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

    for (const auto& event : sequence.eventTrack)
        jsonData["eventTrack"].push_back(SerializeEvent(event));

    for (const auto& actorTransformTrack : sequence.actorTransformTracks)
        jsonData["actorTransformTracks"].push_back(SerializeActorTransformTrack(actorTransformTrack));

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
    loadedSequence.eventTrack.clear();
    loadedSequence.actorTransformTracks.clear();

    const json& modifierJson = jsonData["modifierClip"];
    if (modifierJson.is_object())
    {
        Deserialize_CinematicModifierTrack(modifierJson["shakeAmplitude"], loadedSequence.modifierClip.shakeAmplitude);
        Deserialize_CinematicModifierTrack(modifierJson["shakeFrequencyX"], loadedSequence.modifierClip.shakeFrequencyX);
        Deserialize_CinematicModifierTrack(modifierJson["shakeFrequencyY"], loadedSequence.modifierClip.shakeFrequencyY);
        Deserialize_CinematicModifierTrack(modifierJson["shakeDecayRateX"], loadedSequence.modifierClip.shakeDecayRateX);
        Deserialize_CinematicModifierTrack(modifierJson["shakeDecayRate"], loadedSequence.modifierClip.shakeDecayRate);
        Deserialize_CinematicModifierTrack(modifierJson["shakeVerticalRatio"], loadedSequence.modifierClip.shakeVerticalRatio);
        Deserialize_CinematicModifierTrack(modifierJson["shakeStrengthScale"], loadedSequence.modifierClip.shakeStrengthScale);
    }

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
                    keyframe.interpolation = static_cast<InterpolationMode>(interpolationValue);

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

    struct LegacySpawnEntry
    {
        string id;
        CinematicSpawnEventData payload;
    };

    vector<LegacySpawnEntry> legacySpawnEntries{};
    const json& bindingsJson = jsonData["spawnBindings"];
    if (bindingsJson.is_array())
    {
        for (const auto& bindingJson : bindingsJson)
        {
            LegacySpawnEntry entry{};
            entry.id = bindingJson.value("id", entry.id);
            DeserializeSpawnData(bindingJson, entry.payload);
            legacySpawnEntries.push_back(entry);
        }
    }

    const json& eventTrackJson = jsonData["eventTrack"];
    if (eventTrackJson.is_array())
    {
        for (const auto& eventJson : eventTrackJson)
        {
            CINEMATIC_EVENT event{};
            DeserializeEvent(eventJson, event);

            if (auto* spawn = get_if<CinematicSpawnEventData>(&event.payload);
                spawn != nullptr && spawn->prefabName.empty())
            {
                for (const LegacySpawnEntry& entry : legacySpawnEntries)
                {
                    if (entry.id != event.targetId)
                        continue;

                    *spawn = entry.payload;
                    break;
                }
            }

            loadedSequence.eventTrack.push_back(event);
        }

        std::sort(loadedSequence.eventTrack.begin(), loadedSequence.eventTrack.end(),
            [](const CINEMATIC_EVENT& lhs, const CINEMATIC_EVENT& rhs)
            {
                return lhs.frame < rhs.frame;
            });
    }

    const json& actorTransformTracksJson = jsonData["actorTransformTracks"];
    if (actorTransformTracksJson.is_array())
    {
        for (const auto& trackJson : actorTransformTracksJson)
        {
            CINEMATIC_ACTOR_TRANSFORM_TRACK track{};
            DeserializeActorTransformTrack(trackJson, track);
            loadedSequence.actorTransformTracks.push_back(std::move(track));
        }
    }

    outSequence = loadedSequence;
    return true;
}
