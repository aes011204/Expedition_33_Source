#include "Camera_Tool.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "GameInstance.h"

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
            //카메라 키 설명
            //등록된 카메라 리스트 및 swtich 버튼
            //기본 공통 정보 Transform, fovY, near, far, mouseSensor 등
            //카메라별 특수 정보 followspeed 등
        }

        if(ImGui::CollapsingHeader("Common", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Render_CommonSettings();
        }

        if(ImGui::CollapsingHeader("Specific"))
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

        if(ImGui::Button("Set Active", ImVec2(90.f, 0.f)))
        {
            auto camera = GAME->Get_Camera(_selectedCameraIndex);

            if(!camera)
                return;

            GAME->Change_ActiveCamera(camera);
        }
    }
}

void Camera_Tool::Render_CommonSettings()
{

    int cameraCount =  GAME->Get_CameraCount();

    if(cameraCount<= 0)
        return;

    auto  camera = GAME->Get_Camera(_selectedCameraIndex);
    float fovY = camera->Get_FovY();
    float fNear = camera->Get_Near();
    float fFar = camera->Get_Far();
    float mouseSensor = camera->Get_MouseSensor();
    Vec3  pos = camera->Get_Transform()->Get_Position();

    if(ImGui::DragFloat("fovY", &fovY, 0.1f))
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

    if(ImGui::DragFloat("mouse sensor",&mouseSensor, 0.01f, 0.01, 10.f))
    {
        camera->Set_MouseSensor(mouseSensor);
    }

    if(ImGui::DragFloat3("position", &pos.x))
    {
        camera->Get_Transform()->Set_Position(pos);  
    }
}

void Camera_Tool::Render_SpecificSettings()
{
    auto camera = GAME->Get_Camera(_selectedCameraIndex);

    auto targetCamera = dynamic_pointer_cast<TargetCamera>(camera);
    auto freeCamera = dynamic_pointer_cast<FreeCamera>(camera);

    if(nullptr != targetCamera)
        Render_TargetCameraProperties(targetCamera);

    if(nullptr != freeCamera)
        Render_FreeCameraProperties(freeCamera);
}

void Camera_Tool::Render_FreeCameraProperties(const Shared<FreeCamera>& freeCamera)
{
}

void Camera_Tool::Render_TargetCameraProperties(const Shared<TargetCamera>& targetCamera)
{
    float followSpeed = targetCamera->Get_FollowSpeed();
    float  sideOffset      = targetCamera->Get_SideOffset();
    float  heightOffset      = targetCamera->Get_HeightOffset();

    if(ImGui::DragFloat("follow speed", &followSpeed, 0.01f))
    {
        targetCamera->Set_FollowSpeed(followSpeed);  
    }

  /*  if(ImGui::DragFloat("side offset", &sideOffset, 0.01f))
    {
        targetCamera->Set_SideOffset(sideOffset);  
    }*/

    if(ImGui::DragFloat("height offset", &heightOffset, 0.01f))
    {
        targetCamera->Set_HeightOffset(heightOffset);  
    }

}

Shared<Camera_Tool> Camera_Tool::Create()
{
    return make_shared<Camera_Tool>();
}
