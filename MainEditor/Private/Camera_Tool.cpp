#include "Camera_Tool.h"
#include "BattleTurnCamera.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "GameInstance.h"
#include <magic_enum/magic_enum.hpp>
#include <algorithm>
#include "BattleController.h"
#include "CameraPresetIO.h"
#include "PreviewCamera.h"
#include "CameraPresetTypes.h"
#include "PreviewShotTable.h"
#include "ClientInstance.h"
#include "Editor_Macro.h"
#include "EditorInstance.h"

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

template <typename TEnum>
void Select_PresetEnum(const char* label, TEnum& currentValue)
{
    const char* currentLabel = magic_enum::enum_name(currentValue).data();
    if (!ImGui::BeginCombo(label, currentLabel))
        return;

    for (TEnum value : magic_enum::enum_values<TEnum>())
    {
        const bool isSelected = (currentValue == value);
        if (ImGui::Selectable(magic_enum::enum_name(value).data(), isSelected))
            currentValue = value;

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
}

template <typename TPreset>
bool Edit_OrbitPresetFields(TPreset& preset)
{
    bool isChanged = false;

    isChanged |= ImGui::Checkbox("snap on enter##OrbitPreset", &preset.snapOnEnter);
    isChanged |= ImGui::DragFloat("follow speed##OrbitPreset", &preset.followSpeed, 0.01f);
    isChanged |= ImGui::DragFloat("distance##OrbitPreset", &preset.distance, 0.01f);
    isChanged |= ImGui::DragFloat("side offset##OrbitPreset", &preset.sideOffset, 0.01f);
    isChanged |= ImGui::DragFloat("height offset##OrbitPreset", &preset.heightOffset, 0.01f);
    isChanged |= ImGui::DragFloat("pivot height##OrbitPreset", &preset.pivotHeight, 0.01f);
    isChanged |= ImGui::DragFloat("aim offset##OrbitPreset", &preset.aimOffset, 0.01f);
    isChanged |= ImGui::DragFloat("look height##OrbitPreset", &preset.lookHeight, 0.01f);
    isChanged |= ImGui::DragFloat("mouse sensor##OrbitPreset", &preset.camera.mouseSensor, 0.01f);
    isChanged |= ImGui::DragFloat("mouse sensitivity##OrbitPreset", &preset.camera.mouseSensitivity, 0.01f);
    isChanged |= ImGui::DragFloat("pitch look scale##OrbitPreset", &preset.pitchLookScale, 0.01f);
    isChanged |= ImGui::DragFloat("yaw offset##OrbitPreset", &preset.yawOffset, 0.001f);
    isChanged |= ImGui::DragFloat("pitch offset##OrbitPreset", &preset.pitchOffset, 0.001f);
    isChanged |= ImGui::DragFloat("roll offset##OrbitPreset", &preset.rollOffset, 0.001f);
    isChanged |= ImGui::DragFloat("Min##Pitch", &preset.pitchMin, 0.1f, -89.f, preset.pitchMax);
    isChanged |= ImGui::DragFloat("Max##Pitch", &preset.pitchMax, 0.1f, preset.pitchMin, 89.f);

    return isChanged;
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

void Camera_Tool::Update(float timeDelta)
{
    __super::Update(timeDelta);

    //if(GAME->KeyUp(KEY_TYPE::KEY_1))
    //{
    //    _isTargetCameraEditMode = !_isTargetCameraEditMode;

    //    GAME->Set_PlayerInputBlocked(_isTargetCameraEditMode);
    //  //  GAME->Set_P(_isTargetCameraEditMode);

    //    if(_isTargetCameraEditMode)
    //        GAME->Unlock_Mouse();
    //}
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
    if(ImGui::DragFloat("mouse sensor##CommonCamera",&mouseSensor, 0.01f, 0.01f, 10.f))
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
    auto battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(camera);
    auto freeCamera = dynamic_pointer_cast<FreeCamera>(camera);
    auto previewCamera = dynamic_pointer_cast<PreviewCamera>(camera);

    if(nullptr != targetCamera)
        Render_TargetCameraProperties(targetCamera);

    if (nullptr != battleTurnCamera)
        Render_BattleTurnCameraProperties(battleTurnCamera);

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
    (void)targetCamera;
    Select_EditingCameraState();
    Edit_TargetCameraPreset();
}

void Camera_Tool::Render_BattleTurnCameraProperties(const Shared<BattleTurnCamera>& battleTurnCamera)
{
    (void)battleTurnCamera;
    Select_EditingBattleCameraMode();
    Edit_BattleTurnCameraPreset();
}

void Camera_Tool::Render_PreviewCameraProperties(const Shared<Client::PreviewCamera>& previewCamera)
{
    Select_EditingPreviewPanel();
    Select_EditingPreviewHero();
    Edit_PreviewCameraPreset();
    Reset_ToStartPos();
}

void Camera_Tool::Select_EditingCameraState()
{
    Select_PresetEnum("Edit Preset", _editingCameraState);
}

void Camera_Tool::Edit_TargetCameraPreset()
{

    Shared<Camera> camera = Get_SelectedCamera(_selectedCameraIndex);

    auto targetCamera = static_pointer_cast<TargetCamera>(camera);

    auto preset = targetCamera->Get_Preset(_editingCameraState);

    bool isChanged = Edit_OrbitPresetFields(preset);
    isChanged |= ImGui::Checkbox("allow mouse rotation", &preset.allowMouseRotation);

    if(isChanged)
    {
        targetCamera->Set_Preset(_editingCameraState, preset);
        targetCamera->Apply_PreviewPreset(preset);
    }

    ImGui::SameLine();
    if(ImGui::Button("Save Preset"))
    {
        CameraPresetIO::SaveTargetPresetToFile(_editingCameraState, preset);
    }
}

void Camera_Tool::Select_EditingBattleCameraMode()
{
    Select_PresetEnum("Edit Battle Preset", _editingBattleCameraMode);
}

void Camera_Tool::Edit_BattleTurnCameraPreset()
{
    Shared<Camera> camera = Get_SelectedCamera(_selectedCameraIndex);
    auto battleTurnCamera = static_pointer_cast<BattleTurnCamera>(camera);
    const Shared<Character> previewTarget = Resolve_BattlePreviewTarget();
    const Shared<Character> opponent = Resolve_EnemyPreview();
    if (battleTurnCamera->Is_LiveEditEnabled())
        battleTurnCamera->Update_LiveEditContext(_editingBattleCameraMode, previewTarget, opponent);

    auto preset = battleTurnCamera->Get_Preset(_editingBattleCameraMode);
    bool isChanged = false;

    isChanged |= Edit_OrbitPresetFields(preset);

    if (isChanged)
    {
        battleTurnCamera->Set_Preset(_editingBattleCameraMode, preset);
        BattleTurnCamera::BattleShotRequest request {};
        request.mode = _editingBattleCameraMode;
        request.subject = previewTarget;
        request.opponent = opponent;

        if (battleTurnCamera->Is_LiveEditEnabled())
            battleTurnCamera->Update_LiveEditContext(_editingBattleCameraMode, previewTarget, opponent);
        else
        {
            battleTurnCamera->Apply_PreviewPreset(preset);
            battleTurnCamera->Apply_ShotRequest(request);
        }
    }

    if (!battleTurnCamera->Is_LiveEditEnabled())
    {
        if (ImGui::Button("Start Live Edit"))
        {
            GAME->Change_ActiveCamera(battleTurnCamera);
            EDITOR->Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Game);
            battleTurnCamera->Begin_LiveEdit(_editingBattleCameraMode, previewTarget, opponent);
        }
    }
    else
    {
        if (ImGui::Button("Stop Live Edit"))
            battleTurnCamera->End_LiveEdit();
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Battle Preset"))
    {
        CameraPresetIO::SaveBattlePresetToFile(_editingBattleCameraMode, preset);
    }

    ImGui::TextDisabled("Live Edit Controls: Hold RMB + move mouse = yaw/pitch");
    ImGui::TextDisabled("RMB + W/S = distance, A/D = aim offset, Q/E = pivot offset, Wheel = zoom");
}

void Camera_Tool::Edit_BattleCameraClip()
{
    Shared<Camera> camera = Get_SelectedCamera(_selectedCameraIndex);

    auto battleCamera = static_pointer_cast<BattleTurnCamera>(camera);

  //  auto clip = battleCamera->Get_Clip(_editingBattleCameraClip);
}

void Camera_Tool::Select_EditingBattleClip()
{
    Select_PresetEnum("Edit Battle Preset", _editingBattleCameraMode);
}

void Camera_Tool::Select_EditingPreviewPanel()
{
    Select_PresetEnum("Edit Preview Preset", _editingPreviewPanel);
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

void Camera_Tool::Reset_ToStartPos()
{
    Shared<Camera> camera = Get_SelectedCamera(_selectedCameraIndex);

    auto previewCamera = static_pointer_cast<PreviewCamera>(camera);

    if(!previewCamera)
        return;

    if(ImGui::Button("Reset To Start Pos"))
    {
        previewCamera->Reset_ToStartPos();
    }
}

Shared<Character> Camera_Tool::Resolve_BattlePreviewTarget() const
{
    auto battleController = CLIENT->Get_BattleController();

    if (!battleController)
        return nullptr;

    if (_editingBattleCameraMode == BattleCameraMode::ENEMYFOCUS)
    {
        if (const BattleChar* preview = battleController->Get_TargetPreview())
        {
            if (preview->actor != nullptr)
                return preview->actor;
        }

        const auto enemies = battleController->Get_AliveEnemies();
        if (!enemies.empty() && enemies[0].actor != nullptr)
            return enemies[0].actor;
    }

    if (_editingBattleCameraMode == BattleCameraMode::PLAYERFOCUS)
    {
        const auto heroes = battleController->Get_AliveHeroes();
        if (!heroes.empty() && heroes[0].actor != nullptr)
            return heroes[0].actor;
    }

    if(_editingBattleCameraMode == BattleCameraMode::ENEMYATTACK)
    {

        const auto enemies = battleController->Get_AliveEnemies();

        if(!enemies.empty() && enemies[0].actor != nullptr)
            return enemies[0].actor;
      //  if (const BattleChar* preview = battleController->Get_TargetPreview())
      //  {
      //      if (preview != nullptr)
      //          return preview->actor;
      //  }
    }

    const auto heroes = battleController->Get_AliveHeroes();
    if (!heroes.empty() && heroes[0].actor != nullptr)
        return heroes[0].actor;

    const auto enemies = battleController->Get_AliveEnemies();
    if (!enemies.empty() && enemies[0].actor != nullptr)
        return enemies[0].actor;

    return nullptr;
}

Shared<Character> Camera_Tool::Resolve_EnemyPreview() const
{
    auto battleController = CLIENT->Get_BattleController();

    if(!battleController)
        return nullptr;

    if(_editingBattleCameraMode == BattleCameraMode::ENEMYATTACK)
    {
        if (auto target  = battleController->Get_TurnAction().GetInitTarget().actor)
        {
           return target;
        }
    }

    return nullptr;
}

Shared<Camera_Tool> Camera_Tool::Create()
{
    return make_shared<Camera_Tool>();
}
