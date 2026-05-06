#include "Camera_Tool.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "GameInstance.h"
#include <algorithm>

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

    if(nullptr != targetCamera)
        Render_TargetCameraProperties(targetCamera);

    if(nullptr != freeCamera)
        Render_FreeCameraProperties(freeCamera);
}

void Camera_Tool::Render_FreeCameraProperties(const Shared<FreeCamera>& freeCamera)
{
    (void)freeCamera;
    int moveSpeed = freeCamera->Get_MoveSpeed();

    ImGui::TextDisabled("move speed : %d", moveSpeed);
}

void Camera_Tool::Render_TargetCameraProperties(const Shared<TargetCamera>& targetCamera)
{
    float followSpeed = targetCamera->Get_FollowSpeed();
    float sideOffset = targetCamera->Get_SideOffset();
    float heightOffset = targetCamera->Get_HeightOffset();
    float aimOffset = targetCamera->Get_AimOffset();
    float distance = targetCamera->Get_Distance();
    float pitchLookScale = targetCamera->Get_PitchLookScale();
    float pitchMin = targetCamera->Get_PitchMin();
    float pitchMax = targetCamera->Get_PitchMax();
    float lookHeight = targetCamera->Get_LookHeight();
    float pivotHeight = targetCamera->Get_PivotHeight();

    Draw_SectionHeader("Follow");
    if(ImGui::DragFloat("follow speed", &followSpeed, 0.01f))
    {
        targetCamera->Set_FollowSpeed(followSpeed);  
    }

    Draw_SectionHeader("Orbit");
    if(ImGui::DragFloat("distance", &distance, 0.01f))
    {
        targetCamera->Set_Distance(distance);  
    }

    if(ImGui::DragFloat("side offset", &sideOffset, 0.01f))
    {
        targetCamera->Set_SideOffset(sideOffset);  
    }

    if(ImGui::DragFloat("height offset", &heightOffset, 0.01f))
    {
        targetCamera->Set_HeightOffset(heightOffset);  
    }

    if(ImGui::DragFloat("pivot height", &pivotHeight, 0.01f))
    {
        targetCamera->Set_PivotHeight(pivotHeight);  
    }

    Draw_SectionHeader("Framing");
    if(ImGui::DragFloat("aim offset", &aimOffset, 0.01f))
    {
        targetCamera->Set_AimOffset(aimOffset);  
    }

    if(ImGui::DragFloat("look height", &lookHeight, 0.01f))
    {
        targetCamera->Set_LookHeight(lookHeight);  
    }

    Draw_SectionHeader("Pitch");
    if(ImGui::DragFloat("pitch look scale", &pitchLookScale, 0.01f))
    {
        targetCamera->Set_PitchLookScale(pitchLookScale);  
    }
    
    ImGui::TextDisabled("Pitch Limit");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(90.f);
    if(ImGui::DragFloat("Min##Pitch", &pitchMin, 0.1f, -89.f, pitchMax))
    {
        targetCamera->Set_PitchMin(pitchMin);
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(90.f);
    if(ImGui::DragFloat("Max##Pitch", &pitchMax, 0.1f, pitchMin, 89.f))
    {
        targetCamera->Set_PitchMax(pitchMax);
    }

}

Shared<Camera_Tool> Camera_Tool::Create()
{
    return make_shared<Camera_Tool>();
}
