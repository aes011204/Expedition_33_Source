#pragma once
#include "Editor_Window.h"
#include "TargetCamera.h"
#include "TargetCamera.h"

NS_BEGIN(Client)
    class FreeCamera;
class TargetCamera;
NS_END

NS_BEGIN(MainEditor)

class Camera_Tool : public Editor_Window
{
public:
    Camera_Tool();
    virtual ~Camera_Tool() = default;

public:
    virtual void Render() override;

private:
    void Render_CameraList();
    void Render_CommonSettings();
    void Render_SpecificSettings();
    void Render_FreeCameraProperties(const Shared<Client::FreeCamera>& freeCamera);
    void Render_TargetCameraProperties(const Shared<Client::TargetCamera>& targetCamera);

public:
    void Select_EditingCameraState();
    void Edit_TargetCameraPreset();

private:
    Shared<Camera> _previewCamera;
    int _selectedCameraIndex = { 0 };
    Client::TargetCamera::CAMERA_STATE _editingCameraState = Client::TargetCamera::CAMERA_STATE::NORMAL;

public:
    static Shared<Camera_Tool> Create();
};

NS_END
