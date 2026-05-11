#pragma once

enum class InterpolationMode
{
    Step,
    Linear,
    EaseInOut,
    EaseIn,
    EaseOut
};

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

//월드 좌표 키가 아닌, 두 액터 기준 샷 파라미터를 저장하는 것이 목적
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

typedef struct CinematicSequence
{
    string name = "New Sequence";
    int    fps = 30;
    vector<CINEMATIC_TRACK> cinematicTracks;
    vector<INTERACTION_TRACK> interactionTracks;
    int    startFrame = 0;
    int    endFrame = 300;

}CINEMATIC_SEQUENCE;
