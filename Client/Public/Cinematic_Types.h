#pragma once

enum class InterpolationMode
{
    Step,
    Linear,
};

typedef struct CameraPose
{
    Vec3 position = Vec3::Zero;
    Vec3 rotation = Vec3::Zero;
    float fovY = XMConvertToRadians(60.f);
    float nearPlane = 0.1f;
    float farPlane = 500.f;
    
}CAMERAPOSE;


typedef struct CinematicKeyFrame
{
    int        frame = 0;
    CAMERAPOSE camPose;
    InterpolationMode interpolation = InterpolationMode::Linear;
    
}CINEMATIC_KEYFRAME;

typedef struct CinematicTrack
{
    string name = "Camera Track";
    bool   enabled = true;
    vector<CINEMATIC_KEYFRAME> keyframes;

}CINEMATIC_TRACK;

typedef struct CinematicSequence
{
    string name = "New Sequence";
    int    fps = 30;
    vector<CINEMATIC_TRACK> cinematicTracks;
    int    startFrame = 0;
    int    endFrame = 300;

}CINEMATIC_SEQUENCE;
