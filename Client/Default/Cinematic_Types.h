#pragma once

enum class InterpolationMode
{
    
};

typedef struct CameraPose
{
    Vec3 position;
    Vec3 rotation;
    float fovY = XMConvertToRadians(60.f);
    float nearPlane;
    float farPlane;
    
}CAMERAPOSE;


typedef struct CinematicKeyFrame
{
    int        frame;
    CAMERAPOSE camPose;
    
}CINEMATIC_KEYFRAME;

typedef struct CinematicTrack
{
    string trackName;
    vector<CINEMATIC_KEYFRAME> keyframes;

}CINEMATIC_TRACK;

typedef struct CinematicSequence
{
    vector<CINEMATIC_TRACK> tracks;
    int  startFrame;
    int  endFrame;

}CINEMATIC_SEQUENCE;
