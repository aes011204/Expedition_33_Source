#pragma once

#include <variant>

#include "Delegate.h"
#include "ScalarTrackTypes.h"
#include "CameraModifierTypes.h"

enum class CinematicType
{
    Cinematic,
    Interaction
};

typedef struct CameraPose
{
    Vec3  position = Vec3::Zero;
    Vec3  rotation = Vec3::Zero;
    float fovY = XMConvertToRadians(60.f);
    float nearPlane = 0.1f;
    float farPlane = 500.f;

}CAMERAPOSE;


typedef struct CinematicKeyframe
{
    int               frame = 0;
    CAMERAPOSE        camPose;
    InterpolationMode interpolation = InterpolationMode::Linear;

}CINEMATIC_KEYFRAME;

// Stores shot parameters relative to two actors rather than world-space camera keys.
typedef struct InteractionKeyframe
{
    int   frame = 0;
    float distance = 0.f;
    float height = 0.f;
    float sideOffset = 0.f;
    float yawOffset = 0.f;
    float pitch = 0.f;
    float lookHeight = 0.f;

    InterpolationMode interpolation = InterpolationMode::Linear;

}INTERACTION_KEYFRAME;


typedef struct CinematicTrack
{
    string name = "Camera Track";
    bool   enabled = true;
    vector<CINEMATIC_KEYFRAME> keyframes;

}CINEMATIC_TRACK;

typedef struct InteractionTrack
{
    string name = "Interaction Track";
    bool   enabled = true;
    vector<INTERACTION_KEYFRAME> keyframes;

}INTERACTION_TRACK;

typedef struct CinematicActorTransformKeyframe
{
    int frame = 0;
    Vec3 position = Vec3::Zero;
    Vec3 rotation = Vec3::Zero;
    InterpolationMode interpolation = InterpolationMode::Linear;

}CINEMATIC_ACTOR_TRANSFORM_KEYFRAME;

typedef struct CinematicActorTransformTrack
{
    string targetId;
    string name = "Actor Transform Track";
    bool enabled = true;
    vector<CINEMATIC_ACTOR_TRANSFORM_KEYFRAME> keyframes;

}CINEMATIC_ACTOR_TRANSFORM_TRACK;

enum class CinematicCleanupPolicy
{
    DespawnOnStop,
    Keep,
};

struct CinematicSpawnEventData
{
    string prefabName;
    wstring layerTag = TEXT("Layer_GameObject");
    Vec3 position = Vec3::Zero;
    Vec3 rotation = Vec3::Zero;
    Vec3 scale = Vec3::One;
    CinematicCleanupPolicy cleanupPolicy = CinematicCleanupPolicy::DespawnOnStop;
};

struct CinematicPlayAnimEventData
{
    string animName;
    float blendDuration = 0.25f;
    bool loop = false;
    float startFrame = 0.f;
    float speed = 1.f;
};

struct CinematicDespawnEventData
{
};

using CinematicEventPayload = variant<
    CinematicSpawnEventData,
    CinematicPlayAnimEventData,
    CinematicDespawnEventData>;

typedef struct CinematicEvent
{
    int frame = 0;
    string targetId;
    CinematicEventPayload payload{ CinematicSpawnEventData{} };

}CINEMATIC_EVENT;

typedef struct CinematicSequence
{
    string name = "New Sequence";
    int    fps = 30;
    vector<CINEMATIC_TRACK> cinematicTracks;
    Client::CameraModifierClip modifierClip;
    vector<CINEMATIC_EVENT> eventTrack;
    vector<CINEMATIC_ACTOR_TRANSFORM_TRACK> actorTransformTracks;
    vector<INTERACTION_TRACK> interactionTracks;
    int    startFrame = 0;
    int    endFrame = 300;

}CINEMATIC_SEQUENCE;

// 시네마틱이 종료됐을 떄 알림용
DECLARE_DELEGATE(FOnCinematicCompleted);
