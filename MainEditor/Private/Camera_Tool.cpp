#include "Camera_Tool.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "GameInstance.h"
#include <magic_enum/magic_enum.hpp>
#include <algorithm>
#include "CameraPresetIO.h"
#include "PreviewCamera.h"
#include "PreviewShotTypes.h"
#include "PreviewShotTable.h"
#include "ClientInstance.h"

namespace
{
void Draw_SectionHeader(const char* title)
{
    ImGui::Spacing();
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
}

Shared<Camera> Get_SelectedCamera(int selectedCameraIndex)
{
    const int cameraCount = GAME->Get_CameraCount();
    if (cameraCount <= 0)
        return nullptr;

    const int clampedIndex = std::clamp(selectedCameraIndex, 0, cameraCount - 1);
    return GAME->Get_Camera(clampedIndex);
}
}

Camera_Tool::Camera_Tool() : Editor_Window {L"Camera Tool"}
{
}

void Camera_Tool::Render()
{
    if(!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if(ImGui::Begin(windowName.c_str(),&isOpen))
    {

        if(ImGui::CollapsingHeader("Cameras", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Render_CameraList();
        }

        if(ImGui::CollapsingHeader("Common", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Render_CommonSettings();
        }

        if(ImGui::CollapsingHeader("Specific", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Render_SpecificSettings();
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Camera_Tool::Render_CameraList()
{
    int cameraCount =  GAME->Get_CameraCount();

    if(cameraCount > 0)
        _selectedCameraIndex = std::clamp(_selectedCameraIndex, 0, cameraCount - 1);

    ImGui::TextDisabled("Registered Cameras");
    ImGui::Separator();

    if(cameraCount<=0)
    {
        ImGui::TextDisabled("No cameras registered.");
    }

    else if( ImGui::BeginListBox("##Camera List", ImVec2(-FLT_MIN,140.f)))
    {
        for(int i = 0; i < cameraCount ; ++i)
        {
            auto camera = GAME->Get_Camera(i);

            if(!camera)
                continue;

            const string cameraName = String::ToString(camera->Get_Name());
            bool isActive = GAME->Is_ActiveCamera(camera);

            char label[160]{};
            sprintf_s(label, "%s %d. %s",isActive? "[Active]" : "　　　",i,cameraName.c_str());

            const bool isSelected = (_selectedCameraIndex == i);

            if(ImGui::Selectable(label, isSelected))
                _selectedCameraIndex = i;

            if(isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndListBox();

        auto selectedCamera = Get_SelectedCamera(_selectedCameraIndex);

        if(selectedCamera)
        {
            const string selectedCameraName = String::ToString(selectedCamera->Get_Name());
            ImGui::TextDisabled("Selected : %s", selectedCameraName.c_str());
        }
        ImGui::SameLine();
        if(ImGui::Button("Set Active", ImVec2(100.f, 0.f)))
        {
            auto camera = Get_SelectedCamera(_selectedCameraIndex);

            if(!camera)
                return;

            GAME->Change_ActiveCamera(camera);
        }
    }
}

void Camera_Tool::Render_CommonSettings()
{
    auto camera = Get_SelectedCamera(_selectedCameraIndex);
    if(!camera)
        return;

    float fovY = camera->Get_FovY();
    float fNear = camera->Get_Near();
    float fFar = camera->Get_Far();
    float mouseSensor = camera->Get_MouseSensor();
    Vec3  pos = camera->Get_Transform()->Get_Position();

    const string cameraName = String::ToString(camera->Get_Name());
    ImGui::TextDisabled("Editing : %s", cameraName.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("| Active : %s", GAME->Is_ActiveCamera(camera) ? "Yes" : "No");

    Draw_SectionHeader("Transform");
    if(ImGui::DragFloat3("position", &pos.x, 0.05f))
    {
        camera->Get_Transform()->Set_Position(pos);  
    }

    Draw_SectionHeader("Lens");
    if(ImGui::DragFloat("fovY", &fovY, 0.1f, 0.1f, 3.13f))
    {
      camera->Set_FovY(fovY);  
    }

    if(ImGui::DragFloat("near", &fNear, 0.01f, 0.01f,fFar - 0.01f))
    {
        camera->Set_Near(fNear);  
    }

    if(ImGui::DragFloat("far", &fFar, 0.01f, fNear + 0.01f, 10000.f))
    {
        camera->Set_Far(fFar);  
    }

    Draw_SectionHeader("Input");
    if(ImGui::DragFloat("mouse sensor",&mouseSensor, 0.01f, 0.01f, 10.f))
    {
        camera->Set_MouseSensor(mouseSensor);
    }
}

void Camera_Tool::Render_SpecificSettings()
{
    auto camera = Get_SelectedCamera(_selectedCameraIndex);
    if(!camera)
        return;

    auto targetCamera = dynamic_pointer_cast<TargetCamera>(camera);
    auto freeCamera = dynamic_pointer_cast<FreeCamera>(camera);
    auto previewCamera = dynamic_pointer_cast<PreviewCamera>(camera);

    if(nullptr != targetCamera)
        Render_TargetCameraProperties(targetCamera);

    if(nullptr != freeCamera)
        Render_FreeCameraProperties(freeCamera);

    if(nullptr != previewCamera)
        Render_PreviewCameraProperties(previewCamera);
}

void Camera_Tool::Render_FreeCameraProperties(const Shared<FreeCamera>& freeCamera)
{
    (void)freeCamera;
    int moveSpeed = freeCamera->Get_MoveSpeed();

    ImGui::TextDisabled("move speed : %d", moveSpeed);
}

void Camera_Tool::Render_TargetCameraProperties(const Shared<TargetCamera>& targetCamera)
{
    Select_EditingCameraState();
    Edit_TargetCameraPreset();
}

void Camera_Tool::Render_PreviewCameraProperties(const Shared<Client::PreviewCamera>& previewCamera)
{
    Select_EditingPreviewPanel();
    Select_EditingPreviewHero();
    Edit_PreviewCameraPreset();
}

void Camera_Tool::Select_EditingCameraState()
{
    TargetCamera::CAMERA_STATE editingState =_editingCameraState;

    const char* currentLabel = magic_enum::enum_name(editingState).data();

    if(ImGui::BeginCombo("Edit Preset", currentLabel))
    {
         for(TargetCamera::CAMERA_STATE state : magic_enum::enum_values<TargetCamera::CAMERA_STATE>())
         {
             const bool isSelected = (editingState == state);

             if(ImGui::Selectable(magic_enum::enum_name(state).data(), isSelected))
             {
                 _editingCameraState = state;
             }

             if(isSelected)
                 ImGui::SetItemDefaultFocus();
         }

        ImGui::EndCombo();
    }
}

void Camera_Tool::Edit_TargetCameraPreset()
{

    Shared<Camera> camera = Get_SelectedCamera(_selectedCameraIndex);

    auto targetCamera = static_pointer_cast<TargetCamera>(camera);

    auto preset = targetCamera->Get_Preset(_editingCameraState);

    bool isChanged = false;

    isChanged |= ImGui::DragFloat("follow speed", &preset.followSpeed, 0.01f);
    isChanged |= ImGui::DragFloat("distance", &preset.distance, 0.01f);
    isChanged |= ImGui::DragFloat("side offset", &preset.sideOffset, 0.01f);
    isChanged |= ImGui::DragFloat("height offset", &preset.heightOffset, 0.01f);
    isChanged |= ImGui::DragFloat("pivot height", &preset.pivotHeight, 0.01f);
    isChanged |= ImGui::DragFloat("aim offset", &preset.aimOffset, 0.01f);
    isChanged |= ImGui::DragFloat("look height", &preset.lookHeight, 0.01f);
    isChanged |= ImGui::DragFloat("mouse sensitivity", &preset.mouseSensitivity, 0.01f);
    isChanged |= ImGui::DragFloat("pitch look scale", &preset.pitchLookScale, 0.01f);
    isChanged |= ImGui::DragFloat("yaw offset", &preset.yawOffset, 0.001f);
    isChanged |= ImGui::DragFloat("pitch offset", &preset.pitchOffset, 0.001f);
    isChanged |= ImGui::DragFloat("roll offset", &preset.rollOffset, 0.001f);
    isChanged |= ImGui::DragFloat("Min##Pitch", &preset .pitchMin, 0.1f, -89.f, preset.pitchMax);
    isChanged |= ImGui::DragFloat("Max##Pitch", &preset.pitchMax, 0.1f, preset.pitchMin,89.f);
    isChanged |= ImGui::Checkbox("allow mouse rotation", &preset.allowMouseRotation);

    if(isChanged)
    {
        targetCamera->Set_Preset(_editingCameraState, preset);
        targetCamera->Apply_PreviewPreset(preset);
    }

    ImGui::SameLine();
    if(ImGui::Button("Save Preset"))
    {
        CameraPresetIO::SavePresetToFile(_editingCameraState,preset);
    }
}

void Camera_Tool::Select_EditingPreviewPanel()
{
    PreviewShotPanel previewShotPanel = _editingPreviewPanel;

    const char* currentLabel = magic_enum::enum_name(previewShotPanel).data();

    if(ImGui::BeginCombo("Edit Preset", currentLabel))
    {
        for(PreviewShotPanel panel : magic_enum::enum_values<PreviewShotPanel>())
        {
            const bool isSelected = previewShotPanel == panel;

            if(ImGui::Selectable(magic_enum::enum_name(panel).data(), isSelected))
            {
                _editingPreviewPanel = panel;
            }

            if(isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void Camera_Tool::Select_EditingPreviewHero()
{
    HeroId previewHeroId = _editingPreviewHeroId;
    const char* currentLabel = magic_enum::enum_name(previewHeroId).data();

    if(ImGui::BeginCombo("Edit Hero", currentLabel))
    {
        for(HeroId heroId : magic_enum::enum_values<HeroId>())
        {
            if(heroId == HeroId::END)
            continue;

            const bool isSelected = previewHeroId== heroId;

            if(ImGui::Selectable(magic_enum::enum_name(heroId).data(), isSelected))
            {
                _editingPreviewHeroId = heroId;
            }

            if(isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

}

void Camera_Tool::Edit_PreviewCameraPreset()
{

   Shared<Camera> camera = Get_SelectedCamera(_selectedCameraIndex);

   auto previewCamera = static_pointer_cast<PreviewCamera>(camera);
   const bool needReload =
       !_isPreviewPresetLoaded ||
       _loadedPreviewHeroId != _editingPreviewHeroId ||
       _loadedPreviewPanel != _editingPreviewPanel;

   if(needReload)
   {
       _editingPreviewPreset = CLIENT->Get_Preset(_editingPreviewHeroId, _editingPreviewPanel);
       _loadedPreviewHeroId = _editingPreviewHeroId;
       _loadedPreviewPanel = _editingPreviewPanel;
       _isPreviewPresetLoaded = true;

       previewCamera->Set_Preset(_editingPreviewPreset);
   }

   bool isChanged = false;

    float lookOffset[3] =
    {
        _editingPreviewPreset.lookOffset.x,
        _editingPreviewPreset.lookOffset.y,
        _editingPreviewPreset.lookOffset.z,
    };

    if(ImGui::DragFloat3("look offset", lookOffset,0.01f))
    {
        _editingPreviewPreset.lookOffset = Vec3(lookOffset[0], lookOffset[1], lookOffset[2]);
        isChanged = true;
    }

    isChanged |= ImGui::DragFloat("follow speed", &_editingPreviewPreset.followSpeed, 0.01f);
    isChanged |= ImGui::DragFloat("distance", &_editingPreviewPreset.distance, 0.01f);

    if(isChanged)
    {
        previewCamera->Set_Preset(_editingPreviewPreset);
    }

    ImGui::SameLine();
    if(ImGui::Button("Save Preview Preset"))
    {
        CameraPresetIO::SavePreviewToFile(_editingPreviewPanel, _editingPreviewHeroId, _editingPreviewPreset);
    }
}

Shared<Camera_Tool> Camera_Tool::Create()
{
    return make_shared<Camera_Tool>();
}
