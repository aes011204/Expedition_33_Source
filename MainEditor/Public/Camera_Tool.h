#pragma once
#include "Camera_Tool.h"
#include "Client_Enum.h"
#include "Editor_Window.h"
#include "PreviewShotTypes.h"
#include "TargetCamera.h"

NS_BEGIN(Client)
class FreeCamera;
class TargetCamera;
class PreviewCamera;
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
    void Render_PreviewCameraProperties(const Shared<Client::PreviewCamera>& previewCamera);

public:
    void Select_EditingCameraState();
    void Edit_TargetCameraPreset();
    void Select_EditingPreviewPanel();
    void Select_EditingPreviewHero();
    void Edit_PreviewCameraPreset();

private:
    Shared<Camera> _previewCamera;
    int _selectedCameraIndex = { 0 };
    Client::TargetCamera::CAMERA_STATE _editingCameraState = Client::TargetCamera::CAMERA_STATE::NORMAL;
    PreviewShotPanel _editingPreviewPanel = PreviewShotPanel::CharacterStatus;
    Client::HeroId _editingPreviewHeroId = Client::HeroId::Gustave;
    PreviewShotPreset _editingPreviewPreset = {};
    PreviewShotPanel _loadedPreviewPanel = PreviewShotPanel::CharacterStatus;
    Client::HeroId _loadedPreviewHeroId = Client::HeroId::END;
    bool _isPreviewPresetLoaded = false;

public:
    static Shared<Camera_Tool> Create();
};



NS_END
