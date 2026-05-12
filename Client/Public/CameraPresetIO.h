#pragma once
#include "TargetCamera.h"

NS_BEGIN(Client)
    class TargetCamera;
class CameraPresetIO
{
public:
    CameraPresetIO();
    virtual ~CameraPresetIO();

public:
    static void SavePresetToFile(TargetCamera::CAMERA_STATE cameraState, const TargetCamera::TARGETCAMERA_DESC& preset);
    static bool LoadPresetFromFile(TargetCamera::CAMERA_STATE cameraState, TargetCamera::TARGETCAMERA_DESC& outPreset);
};
NS_END
