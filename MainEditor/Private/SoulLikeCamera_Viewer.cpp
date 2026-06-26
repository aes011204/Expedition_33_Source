#include "pch.h"
#include "SoulLikeCamera_Viewer.h"

#include "Battle_Enum.h"
#include "BattleCameraClipIO.h"
#include "BattleCameraClipUtils.h"
#include "BattleClipSequencerAdapter.h"
#include "CameraPresetIO.h"
#include "AnimNotify_Types.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "Editor_Camera_Free.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "RenderTarget.h"
#include "SkyBoxFlow.h"
#include "SkyCloudCard.h"
#include "Spawn_Prefab.h"
#include "SoulLikeCamera.h"
#include "SoulLikeController.h"
#include "SL_Base.h"
#include "SL_Maelle.h"
#include "SL_Skill.h"
#include "SL_StateMachineCom.h"
#include "TransformCom.h"
#include "magic_enum/magic_enum.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>

NS_BEGIN(MainEditor)

namespace fs = std::filesystem;

namespace
{
    constexpr float kPresetInputWidth = 150.f;
    const Vec3 kPlayerPreviewPosition = Vec3(0.f, 0.f, 0.f);
    const Vec3 kPreviewSkyCloudLocalOffset = Vec3(28.f, 34.f, 82.f);

    const char* Get_ProfileLabel(SoulLikeCamera_Viewer::CameraProfileType type)
    {
        switch (type)
        {
        case SoulLikeCamera_Viewer::CameraProfileType::Base:
            return "기본";
        case SoulLikeCamera_Viewer::CameraProfileType::PlayerSkill:
            return "플레이어 스킬";
        case SoulLikeCamera_Viewer::CameraProfileType::MonsterAttack:
            return "몬스터 공격";
        case SoulLikeCamera_Viewer::CameraProfileType::Parry:
            return "패링";
        case SoulLikeCamera_Viewer::CameraProfileType::PlayerHit:
            return "플레이어 피격";
        case SoulLikeCamera_Viewer::CameraProfileType::JumpCounter:
            return "점프 카운터";
        default:
            return "알 수 없음";
        }
    }

    bool Select_ProfileCombo(const char* label, SoulLikeCamera_Viewer::CameraProfileType& value, float width)
    {
        bool changed = false;
        ImGui::SetNextItemWidth(width);
        if (!ImGui::BeginCombo(label, Get_ProfileLabel(value)))
            return false;

        const SoulLikeCamera_Viewer::CameraProfileType values[] =
        {
            SoulLikeCamera_Viewer::CameraProfileType::Base,
            SoulLikeCamera_Viewer::CameraProfileType::PlayerSkill,
            SoulLikeCamera_Viewer::CameraProfileType::MonsterAttack,
            SoulLikeCamera_Viewer::CameraProfileType::Parry,
            SoulLikeCamera_Viewer::CameraProfileType::PlayerHit,
            SoulLikeCamera_Viewer::CameraProfileType::JumpCounter,
        };

        for (SoulLikeCamera_Viewer::CameraProfileType candidate : values)
        {
            const bool selected = value == candidate;
            if (ImGui::Selectable(Get_ProfileLabel(candidate), selected))
            {
                value = candidate;
                changed = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
        return changed;
    }

    bool Select_FollowEasingCombo(const char* label, BattleCameraTransitionEasing& value, float width)
    {
        bool changed = false;
        const string currentLabel = string(magic_enum::enum_name(value));

        ImGui::SetNextItemWidth(width);
        if (!ImGui::BeginCombo(label, currentLabel.c_str()))
            return false;

        for (BattleCameraTransitionEasing candidate : magic_enum::enum_values<BattleCameraTransitionEasing>())
        {
            const string itemLabel = string(magic_enum::enum_name(candidate));
            const bool selected = value == candidate;
            if (ImGui::Selectable(itemLabel.c_str(), selected))
            {
                value = candidate;
                changed = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
        return changed;
    }

    bool Select_FocusModeCombo(const char* label, SoulLikeCameraFocusMode& value, float width)
    {
        bool changed = false;
        const string currentLabel = string(magic_enum::enum_name(value));

        ImGui::SetNextItemWidth(width);
        if (!ImGui::BeginCombo(label, currentLabel.c_str()))
            return false;

        for (SoulLikeCameraFocusMode candidate : magic_enum::enum_values<SoulLikeCameraFocusMode>())
        {
            const string itemLabel = string(magic_enum::enum_name(candidate));
            const bool selected = value == candidate;
            if (ImGui::Selectable(itemLabel.c_str(), selected))
            {
                value = candidate;
                changed = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
        return changed;
    }

    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return fabsf(lhs - rhs) <= epsilon;
    }

    bool Is_Different(float lhs, float rhs)
    {
        return !NearlyEqual(lhs, rhs);
    }

    void Push_DiffInputColor(bool different)
    {
        if (!different)
            return;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.27f, 0.16f, 0.10f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.36f, 0.20f, 0.12f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.45f, 0.25f, 0.14f, 1.f));
    }

    void Pop_DiffInputColor(bool different)
    {
        if (different)
            ImGui::PopStyleColor(3);
    }

    fs::path Get_SoulLikePresetDirectory()
    {
        return "../../../Client/Bin/Resources/Data/json/CameraPreset";
    }

    bool Try_ExtractSoulLikePresetKey(const fs::path& path, string& outKey)
    {
        const string fileName = path.filename().string();
        constexpr const char* prefix = "CameraPreset_SOULLIKE";
        constexpr const char* extension = ".json";

        if (fileName.rfind(prefix, 0) != 0)
            return false;

        if (fileName.size() < strlen(extension) ||
            fileName.substr(fileName.size() - strlen(extension)) != extension)
        {
            return false;
        }

        string key = fileName.substr(strlen(prefix), fileName.size() - strlen(prefix) - strlen(extension));
        if (!key.empty() && key.front() == '_')
            key.erase(key.begin());

        outKey = key;
        return true;
    }

    bool Contains_CaseInsensitive(const string& text, const string& pattern)
    {
        if (pattern.empty())
            return true;

        string lowerText = text;
        string lowerPattern = pattern;
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
            [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(),
            [](unsigned char ch) { return static_cast<char>(tolower(ch)); });

        return lowerText.find(lowerPattern) != string::npos;
    }

    bool Same_Preset(const TargetCameraPreset& lhs, const TargetCameraPreset& rhs)
    {
        return lhs.snapOnEnter == rhs.snapOnEnter &&
            lhs.allowMouseRotation == rhs.allowMouseRotation &&
            lhs.doCollisionTest == rhs.doCollisionTest &&
            lhs.focusMode == rhs.focusMode &&
            lhs.targetFollowEasing == rhs.targetFollowEasing &&
            NearlyEqual(lhs.camera.fovy, rhs.camera.fovy) &&
            NearlyEqual(lhs.camera.nearPlane, rhs.camera.nearPlane) &&
            NearlyEqual(lhs.camera.farPlane, rhs.camera.farPlane) &&
            NearlyEqual(lhs.camera.mouseSensor, rhs.camera.mouseSensor) &&
            NearlyEqual(lhs.camera.mouseSensitivity, rhs.camera.mouseSensitivity) &&
            NearlyEqual(lhs.followSpeed, rhs.followSpeed) &&
            NearlyEqual(lhs.targetFollowSpeed, rhs.targetFollowSpeed) &&
            NearlyEqual(lhs.verticalFollowSpeed, rhs.verticalFollowSpeed) &&
            NearlyEqual(lhs.distance, rhs.distance) &&
            NearlyEqual(lhs.sideOffset, rhs.sideOffset) &&
            NearlyEqual(lhs.heightOffset, rhs.heightOffset) &&
            NearlyEqual(lhs.aimOffset, rhs.aimOffset) &&
            NearlyEqual(lhs.pitchLookScale, rhs.pitchLookScale) &&
            NearlyEqual(lhs.pitchMin, rhs.pitchMin) &&
            NearlyEqual(lhs.pitchMax, rhs.pitchMax) &&
            NearlyEqual(lhs.yawOffset, rhs.yawOffset) &&
            NearlyEqual(lhs.pitchOffset, rhs.pitchOffset) &&
            NearlyEqual(lhs.rollOffset, rhs.rollOffset) &&
            NearlyEqual(lhs.lookHeight, rhs.lookHeight) &&
            NearlyEqual(lhs.pivotHeight, rhs.pivotHeight) &&
            NearlyEqual(lhs.collisionProbeRadius, rhs.collisionProbeRadius) &&
            NearlyEqual(lhs.collisionSkin, rhs.collisionSkin);
    }
}

SoulLikeCamera_Viewer::SoulLikeCamera_Viewer()
    : Editor_Window(L"SoulLikeCamera_Viewer")
{
    _editingClip.timeMode = BattleCameraClipTimeMode::Independent;
    Set_Open(false);
}

void SoulLikeCamera_Viewer::Update(float timeDelta)
{
    if (_autoSyncRuntimeSkill)
        Sync_CurrentRuntimeSkill();

    if (_timelinePlaying)
    {
        const float duration = Resolve_CurrentAnimationDurationSec();
        _previewTimeSec += timeDelta;
        if (duration > 0.f && _previewTimeSec > duration)
            _previewTimeSec = 0.f;

        Sync_ClipFrameFromPreviewTime();
    }
}

void SoulLikeCamera_Viewer::Pre_Render()
{
    if (!Is_Open())
        return;

    Ensure_PreviewCamera();
    Ensure_PreviewSkyBackdrop();
    Ensure_PreviewActors();
    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);

    if (_previewCamera == nullptr || _previewRenderTarget == nullptr)
        return;

    Refresh_PlayerAnimationList();
    Refresh_MonsterAnimationList();

    const float dt = ImGui::GetIO().DeltaTime;
    Apply_PreviewAnimation(dt);
    Apply_SoulLikeCameraPreviewPose();
    Render_PreviewScene(dt);
}

void SoulLikeCamera_Viewer::Render()
{
    if (!Is_Open())
        return;

    Refresh_SkillList();

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isVisible = true;
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Render_Header();
        ImGui::Separator();

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float minTopHeight = 280.f;
        const float maxTopHeight = std::max(minTopHeight, availableHeight - 120.f);
        _topSectionHeight = std::clamp(_topSectionHeight, minTopHeight, maxTopHeight);

        if (ImGui::BeginChild("SoulLikeCameraViewerTop", ImVec2(0.f, _topSectionHeight), false))
        {
            if (ImGui::BeginTable("##SoulLikeCameraViewerLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed, 300.f);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Preset", ImGuiTableColumnFlags_WidthFixed, 340.f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                Render_LeftPanel();

                ImGui::TableSetColumnIndex(1);
                Render_PreviewCanvas();

                ImGui::TableSetColumnIndex(2);
                Render_PresetPanel();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        const ImVec2 splitterPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("SoulLikeCameraViewerTopBottomSplitter", ImVec2(-FLT_MIN, 6.f));
        if (ImGui::IsItemActive())
            _topSectionHeight = std::clamp(_topSectionHeight + ImGui::GetIO().MouseDelta.y, minTopHeight, maxTopHeight);

        const bool splitterHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitterPos,
            ImVec2(splitterPos.x + ImGui::GetContentRegionAvail().x, splitterPos.y + 6.f),
            splitterHovered ? IM_COL32(110, 110, 118, 255) : IM_COL32(72, 72, 78, 255),
            2.f);

        Render_TimelinePanel();
    }

    Set_Open(isOpen);
    if (!isOpen)
        Clear_PreviewActors();

    ImGui::End();
}

void SoulLikeCamera_Viewer::Save()
{
    Save_CurrentPreset();
}

void SoulLikeCamera_Viewer::Refresh_SkillList()
{
    string selectedSkillKey{};
    if (_selectedSkillIndex >= 0 && _selectedSkillIndex < static_cast<int>(_skillKeys.size()))
        selectedSkillKey = _skillKeys[_selectedSkillIndex];

    vector<string> nextSkillKeys{};
    auto appendSkillKey = [&nextSkillKeys](const string& skillKey)
        {
            if (skillKey.empty())
                return;

            if (std::find(nextSkillKeys.begin(), nextSkillKeys.end(), skillKey) == nextSkillKeys.end())
                nextSkillKeys.push_back(skillKey);
        };

    if (const Shared<Client::SoulLikeController> controller = CLIENT->Get_SoulLikeController())
    {
        if (const Shared<Client::SL_Maelle> player = controller->Get_Player())
        {
            for (uint32 slotIndex = 0; slotIndex < 4; ++slotIndex)
            {
                Skill_Data skillData{};
                if (player->Try_GetEquippedSkill(slotIndex, skillData))
                    appendSkillKey(skillData.skillKey);
            }
        }
    }

    if (nextSkillKeys.empty())
    {
        const vector<string> allSkillIds = CLIENT->Get_AllSkillIds();
        for (const string& skillId : allSkillIds)
        {
            const Skill_Data* skillData = CLIENT->Get_Skill(skillId);
            if (skillData == nullptr)
                continue;

            string characterId = skillData->characterId;
            std::transform(characterId.begin(), characterId.end(), characterId.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            if (characterId != "maelle")
                continue;

            if (skillData->skillKey == "Maelle_Default_Attack")
                continue;

            appendSkillKey(skillData->skillKey);
        }
    }

    _skillKeys = std::move(nextSkillKeys);

    if (!selectedSkillKey.empty())
    {
        const auto iter = std::find(_skillKeys.begin(), _skillKeys.end(), selectedSkillKey);
        if (iter != _skillKeys.end())
        {
            _selectedSkillIndex = static_cast<int>(std::distance(_skillKeys.begin(), iter));
            return;
        }
    }

    if (!_skillKeys.empty() && _selectedSkillIndex < 0)
        _selectedSkillIndex = 0;
    else if (_skillKeys.empty())
        _selectedSkillIndex = -1;
    else
        _selectedSkillIndex = std::clamp(_selectedSkillIndex, 0, static_cast<int>(_skillKeys.size()) - 1);
}

void SoulLikeCamera_Viewer::Refresh_PlayerAnimationList()
{
    if (!_playerAnimationListDirty && !_playerAnimationNames.empty())
        return;

    _playerAnimationNames.clear();

    Shared<ModelCom> model = Resolve_PlayerModel();
    if (model != nullptr)
    {
        const size_t count = model->Get_NumAnimations();
        for (uint32 i = 0; i < static_cast<uint32>(count); ++i)
        {
            const char* name = model->Get_AnimationName(i);
            if (name != nullptr)
                _playerAnimationNames.push_back(name);
        }
    }

    _selectedPlayerAnimationIndex = _playerAnimationNames.empty()
        ? -1
        : std::clamp(_selectedPlayerAnimationIndex, 0, static_cast<int>(_playerAnimationNames.size()) - 1);

    _playerAnimationListDirty = false;
}

void SoulLikeCamera_Viewer::Refresh_MonsterAnimationList()
{
    if (!_monsterAnimationListDirty && !_monsterAnimationNames.empty())
        return;

    _monsterAnimationNames.clear();

    Shared<ModelCom> model = Resolve_MonsterModel();
    if (model != nullptr)
    {
        const size_t count = model->Get_NumAnimations();
        for (uint32 i = 0; i < static_cast<uint32>(count); ++i)
        {
            const char* name = model->Get_AnimationName(i);
            if (name != nullptr)
                _monsterAnimationNames.push_back(name);
        }
    }

    _selectedMonsterAnimationIndex = _monsterAnimationNames.empty()
        ? -1
        : std::clamp(_selectedMonsterAnimationIndex, 0, static_cast<int>(_monsterAnimationNames.size()) - 1);

    _monsterAnimationListDirty = false;
}

void SoulLikeCamera_Viewer::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
    width = std::max(1u, width);
    height = std::max(1u, height);

    if (_previewRenderTarget == nullptr)
    {
        _previewRenderTarget = RenderTarget::Create(
            L"SoulLikeCameraViewerRenderTarget",
            GAME->Get_Device(),
            GAME->Get_Context(),
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4(0.08f, 0.09f, 0.11f, 1.f),
            true);
        return;
    }

    if (_previewRenderTarget->Get_Width() == width && _previewRenderTarget->Get_Height() == height)
        return;

    _previewRenderTarget->Resize(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4(0.08f, 0.09f, 0.11f, 1.f),
        true);
}

void SoulLikeCamera_Viewer::Ensure_PreviewCamera()
{
    if (_previewCamera != nullptr)
        return;

    _previewCamera = Editor_Camera_Free::Create(GAME->Get_Device(), GAME->Get_Context());
    if (_previewCamera == nullptr)
        return;

    Editor_Camera_Free::EDITOR_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(0.f, 2.2f, -6.f, 1.f);
    desc.at = XMFLOAT4(0.f, 1.f, 0.f, 1.f);
    desc.fovy = XM_PIDIV4;
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.15f;
    desc.speedPerSec = 5.f;
    desc.degreePerSec = 180.f;

    if (FAILED(_previewCamera->Initialize(&desc)))
    {
        _previewCamera = nullptr;
        return;
    }

    _previewCamera->Apply_EditorDesc(desc);
}

void SoulLikeCamera_Viewer::Ensure_PreviewActors()
{
    if (!_previewActorsDirty && _previewPlayer != nullptr && _previewMonster != nullptr)
        return;

    Clear_PreviewActors();

    if (!_playerPrefab.empty())
        _previewPlayer = GAME->Instantiate_GameObjectJson(Client::Spawn_Prefab::Resolve_PrefabPath(_playerPrefab));

    if (!_monsterPrefab.empty())
        _previewMonster = GAME->Instantiate_GameObjectJson(Client::Spawn_Prefab::Resolve_PrefabPath(_monsterPrefab));

    _previewActorsDirty = false;
    _previewObjectsBegunPlay = false;
    _playerAnimationListDirty = true;
    _monsterAnimationListDirty = true;
    Position_PreviewActors();
}

void SoulLikeCamera_Viewer::Ensure_PreviewSkyBackdrop()
{
    if (_previewSkyBox != nullptr)
        return;

    Shared<Client::SkyBoxFlow> skyBoxPrototype = Client::SkyBoxFlow::Create(GAME->Get_Device(), GAME->Get_Context());
    if (skyBoxPrototype == nullptr)
        return;

    Client::SkyBoxFlow::SKYBOXFLOW_DESC skyBoxDesc{};
    _previewSkyBox = dynamic_pointer_cast<Client::SkyBoxFlow>(skyBoxPrototype->Clone(&skyBoxDesc));
    if (_previewSkyBox != nullptr)
        _previewSkyBox->Set_Visible(true);

    _previewSkyCloudCards.clear();

    Shared<Client::SkyCloudCard> skyCloudPrototype = Client::SkyCloudCard::Create(GAME->Get_Device(), GAME->Get_Context());
    if (skyCloudPrototype == nullptr)
        return;

    Client::SkyCloudCard::SKYCLOUDCARD_DESC cloudDesc{};
    cloudDesc.modelTag = TEXT("Model_Cloud_Mesh");
    cloudDesc.shaderTag = TEXT("Shader_SkyCloudCard");
    cloudDesc.cloudTextureTag = TEXT("Texture_T_Cloud_13_H");
    cloudDesc.position = kPreviewSkyCloudLocalOffset;
    cloudDesc.rotation = Vec3(-4.763f, -119.267f, -349.155f);
    cloudDesc.scale = Vec3(1.5f, 0.9f, 0.9f);

    Shared<Client::SkyCloudCard> skyCloudCard =
        dynamic_pointer_cast<Client::SkyCloudCard>(skyCloudPrototype->Clone(&cloudDesc));
    if (skyCloudCard != nullptr)
    {
        skyCloudCard->Set_Visible(true);
        _previewSkyCloudCards.push_back(skyCloudCard);
    }
}

void SoulLikeCamera_Viewer::Clear_PreviewActors()
{
    _previewPlayer.reset();
    _previewMonster.reset();
    _previewObjectsBegunPlay = false;
    _playerAnimationListDirty = true;
    _monsterAnimationListDirty = true;
}

void SoulLikeCamera_Viewer::Render_Header()
{
    ImGui::TextUnformatted("SoulLikeCamera_Viewer");
    ImGui::SameLine();
    ImGui::TextDisabled("플레이어 기반 3D 카메라 프리셋 튜닝");

    const float headerButtonStart = std::max(ImGui::GetCursorPosX() + 20.f, ImGui::GetWindowWidth() - 790.f);
    ImGui::SameLine(headerButtonStart);
    if (ImGui::Button("런타임 캡처"))
        Capture_RuntimePreset();

    ImGui::SameLine();
    if (ImGui::Button("런타임 적용"))
        Apply_RuntimePreset();

    ImGui::SameLine();
    if (ImGui::Button(_viewingSavedPreset ? "편집값 보기" : "저장값 보기"))
        Toggle_SavedComparison();

    ImGui::SameLine();
    ImGui::BeginDisabled(_undoStack.empty());
    if (ImGui::Button("되돌리기"))
        Undo_Preset();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(_redoStack.empty());
    if (ImGui::Button("다시하기"))
        Redo_Preset();
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("불러오기"))
        Load_CurrentPreset();

    ImGui::SameLine();
    if (ImGui::Button("기본 불러오기"))
        Load_CurrentPreset(true);

    ImGui::SameLine();
    if (ImGui::Button("저장"))
    {
        _savePresetKey = Build_CurrentPresetKey();
        ImGui::OpenPopup("Save SoulLike Camera Preset");
    }

    Render_SavePresetPopup();

    if (!_statusMessage.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", _statusMessage.c_str());
    }
}

void SoulLikeCamera_Viewer::Render_SavePresetPopup()
{
    if (!ImGui::BeginPopupModal("Save SoulLike Camera Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    char presetKeyBuffer[192] = {};
    strncpy_s(presetKeyBuffer, _savePresetKey.c_str(), _TRUNCATE);

    ImGui::TextUnformatted("Preset key");
    ImGui::SetNextItemWidth(360.f);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##SoulLikeCameraSavePresetKey", presetKeyBuffer, IM_ARRAYSIZE(presetKeyBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        _savePresetKey = presetKeyBuffer;
        if (!_savePresetKey.empty())
        {
            Save_CurrentPreset(_savePresetKey);
            ImGui::CloseCurrentPopup();
        }
    }
    else if (strcmp(presetKeyBuffer, _savePresetKey.c_str()) != 0)
    {
        _savePresetKey = presetKeyBuffer;
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(_savePresetKey.empty());
    if (ImGui::Button("Save", ImVec2(120.f, 0.f)))
    {
        Save_CurrentPreset(_savePresetKey);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void SoulLikeCamera_Viewer::Render_LeftPanel()
{
    ImGui::BeginChild("SoulLikeCameraViewerLeft", ImVec2(0.f, 0.f), true);

    if (Select_ProfileCombo("프로필", _profileType, 180.f))
        Load_CurrentPreset();

    if (ImGui::Checkbox("런타임 스킬 자동 선택", &_autoSyncRuntimeSkill) && _autoSyncRuntimeSkill)
        Sync_CurrentRuntimeSkill();

    ImGui::SameLine();
    if (ImGui::Button("스킬 동기화"))
        Sync_CurrentRuntimeSkill();

    ImGui::Separator();

    char playerPrefabBuffer[128] = {};
    strncpy_s(playerPrefabBuffer, _playerPrefab.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::InputText("플레이어 프리팹", playerPrefabBuffer, IM_ARRAYSIZE(playerPrefabBuffer)))
    {
        _playerPrefab = playerPrefabBuffer;
        _previewActorsDirty = true;
    }

    char monsterPrefabBuffer[128] = {};
    strncpy_s(monsterPrefabBuffer, _monsterPrefab.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::InputText("몬스터 프리팹", monsterPrefabBuffer, IM_ARRAYSIZE(monsterPrefabBuffer)))
    {
        _monsterPrefab = monsterPrefabBuffer;
        _previewActorsDirty = true;
        _loadedPresetKey.clear();
    }

    if (ImGui::Button("액터 다시 로드"))
        _previewActorsDirty = true;

    ImGui::Separator();

    ImGui::TextDisabled("프리뷰 배치");
    ImGui::SetNextItemWidth(220.f);
    ImGui::DragFloat("몬스터 거리", &_previewMonsterDistance, 0.05f, 0.f, 20.f, "%.2f");
    ImGui::SetNextItemWidth(220.f);
    ImGui::DragFloat("몬스터 좌우", &_previewMonsterSideOffset, 0.05f, -10.f, 10.f, "%.2f");

    if (ImGui::Button("근거리"))
        Set_PreviewPose(2.2f, 0.f);
    ImGui::SameLine();
    if (ImGui::Button("중거리"))
        Set_PreviewPose(4.f, 0.f);
    ImGui::SameLine();
    if (ImGui::Button("원거리"))
        Set_PreviewPose(7.f, 0.f);

    if (ImGui::Button("왼쪽 배치"))
        Set_PreviewPose(_previewMonsterDistance, -2.5f);
    ImGui::SameLine();
    if (ImGui::Button("오른쪽 배치"))
        Set_PreviewPose(_previewMonsterDistance, 2.5f);

    ImGui::Separator();

    if (_profileType == CameraProfileType::PlayerSkill)
    {
        const char* currentSkill = (_selectedSkillIndex >= 0 && _selectedSkillIndex < static_cast<int>(_skillKeys.size()))
            ? _skillKeys[_selectedSkillIndex].c_str()
            : "(없음)";

        ImGui::SetNextItemWidth(220.f);
        if (ImGui::BeginCombo("스킬 키", currentSkill))
        {
            for (int i = 0; i < static_cast<int>(_skillKeys.size()); ++i)
            {
                const bool selected = i == _selectedSkillIndex;
                if (ImGui::Selectable(_skillKeys[i].c_str(), selected))
                {
                    _selectedSkillIndex = i;
                    Load_CurrentPreset();
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

    }

    if (_profileType == CameraProfileType::PlayerSkill ||
        _profileType == CameraProfileType::Parry ||
        _profileType == CameraProfileType::PlayerHit ||
        _profileType == CameraProfileType::JumpCounter)
    {
        const char* currentAnimation = (_selectedPlayerAnimationIndex >= 0 && _selectedPlayerAnimationIndex < static_cast<int>(_playerAnimationNames.size()))
            ? _playerAnimationNames[_selectedPlayerAnimationIndex].c_str()
            : "(없음)";

        ImGui::SetNextItemWidth(220.f);
        if (ImGui::BeginCombo("플레이어 애니메이션", currentAnimation))
        {
            for (int i = 0; i < static_cast<int>(_playerAnimationNames.size()); ++i)
            {
                const bool selected = i == _selectedPlayerAnimationIndex;
                if (ImGui::Selectable(_playerAnimationNames[i].c_str(), selected))
                {
                    _selectedPlayerAnimationIndex = i;
                    _previewTimeSec = 0.f;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if (_profileType == CameraProfileType::MonsterAttack)
    {
        const char* currentAnimation = (_selectedMonsterAnimationIndex >= 0 && _selectedMonsterAnimationIndex < static_cast<int>(_monsterAnimationNames.size()))
            ? _monsterAnimationNames[_selectedMonsterAnimationIndex].c_str()
            : "(없음)";

        ImGui::SetNextItemWidth(220.f);
        if (ImGui::BeginCombo("몬스터 애니메이션", currentAnimation))
        {
            for (int i = 0; i < static_cast<int>(_monsterAnimationNames.size()); ++i)
            {
                const bool selected = i == _selectedMonsterAnimationIndex;
                if (ImGui::Selectable(_monsterAnimationNames[i].c_str(), selected))
                {
                    _selectedMonsterAnimationIndex = i;
                    _previewTimeSec = 0.f;
                    Load_CurrentPreset();
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();
    const string presetKey = Build_CurrentPresetKey();
    const bool hasRuntimeCamera = Resolve_RuntimeSoulLikeCamera() != nullptr;
    const bool activeRuntimeCamera = GAME->Is_ActiveCameraType(ObjectType::SoulLikeCamera);
    const bool presetFileExists = Current_PresetFileExists();
    ImGui::Text("프리셋 키 : %s", presetKey.empty() ? "(기본)" : presetKey.c_str());
    ImGui::Text("파일 : %s", presetFileExists ? "있음" : "없음");
    ImGui::Text("런타임 : %s / %s",
        hasRuntimeCamera ? "카메라 있음" : "카메라 없음",
        activeRuntimeCamera ? "활성" : "비활성");
    ImGui::Text("자동 적용 : %s", Is_CurrentProfileRuntimeConnected() ? "연결됨" : "저장 전용");
    ImGui::Text("수정됨 : %s", _presetDirty ? "예" : "아니오");
    if (ImGui::Checkbox("런타임 실시간 반영", &_runtimeFollowEnabled) && _runtimeFollowEnabled)
        Apply_RuntimePreset();

    ImGui::Separator();
    char copyPresetBuffer[160] = {};
    strncpy_s(copyPresetBuffer, _copyPresetKey.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::InputText("복사 키", copyPresetBuffer, IM_ARRAYSIZE(copyPresetBuffer)))
        _copyPresetKey = copyPresetBuffer;

    if (ImGui::Button("현재 키 사용"))
        _copyPresetKey = presetKey;

    ImGui::SameLine();
    if (ImGui::Button("복사 저장"))
        Save_CopyPreset();

    ImGui::Separator();
    Render_PresetBrowser();

    ImGui::EndChild();
}

void SoulLikeCamera_Viewer::Render_PreviewCanvas()
{
    ImGui::BeginChild("SoulLikeCameraViewerPreview", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextUnformatted("프리뷰");
    ImGui::SameLine();
    ImGui::TextDisabled("소울라이크 카메라 포즈");
    ImGui::Separator();

    ImVec2 previewSize = ImGui::GetContentRegionAvail();
    previewSize.x = std::max(1.f, previewSize.x);
    previewSize.y = std::max(1.f, previewSize.y);

    _previewWidth = std::max(1u, static_cast<uint32>(previewSize.x));
    _previewHeight = std::max(1u, static_cast<uint32>(previewSize.y));

    const ImVec2 previewPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("SoulLikeCameraViewerCanvas", previewSize);

    const ImVec2 previewMax = ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(previewPos, previewMax, IM_COL32(18, 20, 24, 255));
    drawList->AddRect(previewPos, previewMax, IM_COL32(55, 60, 70, 255));

    if (_previewRenderTarget != nullptr && _previewRenderTarget->Get_SRV() != nullptr)
    {
        const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(_previewRenderTarget->Get_SRV()));
        drawList->AddImage(textureId, previewPos, previewMax);
    }
    else
    {
        drawList->AddText(ImVec2(previewPos.x + 12.f, previewPos.y + 12.f), IM_COL32(210, 216, 224, 255), "프리뷰 준비 안 됨");
    }

    Update_PreviewCameraControls(previewPos, previewMax);

    drawList->AddText(
        ImVec2(previewPos.x + 12.f, previewMax.y - 24.f),
        _previewCameraControlActive ? IM_COL32(255, 210, 120, 255) : IM_COL32(180, 186, 196, 210),
        "우클릭 + WASD/QE: 카메라 이동   마우스: 회전   휠: 거리   Shift/Ctrl: 속도");

    ImGui::EndChild();
}

void SoulLikeCamera_Viewer::Update_PreviewCameraControls(const ImVec2& previewMin, const ImVec2& previewMax)
{
    const bool canvasHovered = ImGui::IsMouseHoveringRect(previewMin, previewMax, false);

    if (!_previewCameraControlActive && canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        _previewCameraControlActive = true;
        _previewCameraControlStartPreset = _editingPreset;
    }

    if (!_previewCameraControlActive)
        return;

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        _previewCameraControlActive = false;
        Push_UndoSnapshot(_previewCameraControlStartPreset);
        return;
    }

    const TargetCameraPreset beforeEditPreset = _editingPreset;
    const ImGuiIO& io = ImGui::GetIO();
    const float dt = std::max(io.DeltaTime, 0.0001f);
    float moveSpeed = std::max(0.75f, _editingPreset.distance) * 0.8f;

    if (io.KeyShift)
        moveSpeed *= 3.f;
    if (io.KeyCtrl)
        moveSpeed *= 0.25f;

    const float moveDelta = moveSpeed * dt;
    bool changed = false;

    if (io.MouseWheel != 0.f)
    {
        _editingPreset.distance = std::max(0.1f, _editingPreset.distance - io.MouseWheel * moveSpeed * 0.12f);
        changed = true;
    }

    if (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f)
    {
        constexpr float mouseSensitivity = 0.12f;
        _editingPreset.yawOffset += io.MouseDelta.x * mouseSensitivity;
        _editingPreset.pitchOffset = std::clamp(
            _editingPreset.pitchOffset + io.MouseDelta.y * mouseSensitivity,
            _editingPreset.pitchMin,
            _editingPreset.pitchMax);
        changed = true;
    }

    Vec3 localMove = Vec3::Zero;
    if (ImGui::IsKeyDown(ImGuiKey_W))
        localMove.z += moveDelta;
    if (ImGui::IsKeyDown(ImGuiKey_S))
        localMove.z -= moveDelta;
    if (ImGui::IsKeyDown(ImGuiKey_A))
        localMove.x -= moveDelta;
    if (ImGui::IsKeyDown(ImGuiKey_D))
        localMove.x += moveDelta;
    if (ImGui::IsKeyDown(ImGuiKey_Q))
        localMove.y -= moveDelta;
    if (ImGui::IsKeyDown(ImGuiKey_E))
        localMove.y += moveDelta;

    if (localMove.LengthSquared() > 0.000001f)
    {
        const float pitchRad = XMConvertToRadians(_editingPreset.pitchOffset);
        const float cosPitch = cosf(pitchRad);
        const float sinPitch = sinf(pitchRad);

        _editingPreset.aimOffset += localMove.x;
        _editingPreset.distance = std::max(
            0.1f,
            _editingPreset.distance - localMove.z * cosPitch + localMove.y * sinPitch);
        _editingPreset.pivotHeight += localMove.z * sinPitch + localMove.y * cosPitch;
        changed = true;
    }

    if (!changed)
        return;

    _presetDirty = true;
    _viewingSavedPreset = false;

    if (!Same_Preset(beforeEditPreset, _editingPreset))
        _redoStack.clear();

    if (Has_ClipTrackKeys(_editingClip.distanceOffset, _clipDistanceEnabled) ||
        Has_ClipTrackKeys(_editingClip.sideOffset, _clipSideEnabled) ||
        Has_ClipTrackKeys(_editingClip.pivotOffset, _clipPivotEnabled) ||
        Has_ClipTrackKeys(_editingClip.heightOffset, _clipHeightEnabled) ||
        Has_ClipTrackKeys(_editingClip.aimOffset, _clipAimEnabled) ||
        Has_ClipTrackKeys(_editingClip.lookHeightOffset, _clipLookHeightEnabled) ||
        Has_ClipTrackKeys(_editingClip.yawOffset, _clipYawEnabled))
    {
        Insert_CurrentPoseKeysAtFrame(_clipCurrentFrame);
    }
    else if (Update_CurrentPoseKeysAtFrame(_clipCurrentFrame))
    {
        _clipDirty = true;
    }

    Apply_SoulLikeCameraPreviewPose();

    if (_runtimeFollowEnabled && Is_CurrentProfileRuntimeConnected())
        Apply_RuntimePreset();
}

void SoulLikeCamera_Viewer::Render_PresetPanel()
{
    ImGui::BeginChild("SoulLikeCameraViewerPreset", ImVec2(0.f, 0.f), true);

    ImGui::TextDisabled("프리셋");
    if (_viewingSavedPreset)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.72f, 0.35f, 1.f), "저장값 보는 중");
    }
    ImGui::Separator();

    TargetCameraPreset basePreset{};
    const bool hasBasePreset = Client::CameraPresetIO::LoadSoulLikePresetFromFile("", basePreset);
    const TargetCameraPreset beforeEditPreset = _editingPreset;

    bool changed = false;
    auto resetFloatButton = [&changed, hasBasePreset](const char* id, float& value, float baseValue, bool different)
    {
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasBasePreset || !different);
        if (ImGui::SmallButton(("기본##" + string(id)).c_str()))
        {
            value = baseValue;
            changed = true;
        }
        ImGui::EndDisabled();
    };

    auto dragFloat = [&changed, &resetFloatButton, hasBasePreset](
        const char* label,
        float& value,
        float baseValue,
        float speed,
        float minValue,
        float maxValue,
        const char* format = "%.3f")
    {
        const bool different = hasBasePreset && Is_Different(value, baseValue);
        Push_DiffInputColor(different);
        ImGui::SetNextItemWidth(kPresetInputWidth);
        if (ImGui::DragFloat(label, &value, speed, minValue, maxValue, format))
            changed = true;
        Pop_DiffInputColor(different);
        resetFloatButton(label, value, baseValue, different);
    };

    auto checkboxField = [&changed, hasBasePreset](const char* label, bool& value, bool baseValue)
    {
        const bool different = hasBasePreset && value != baseValue;
        Push_DiffInputColor(different);
        if (ImGui::Checkbox(label, &value))
            changed = true;
        Pop_DiffInputColor(different);

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasBasePreset || !different);
        if (ImGui::SmallButton(("기본##" + string(label)).c_str()))
        {
            value = baseValue;
            changed = true;
        }
        ImGui::EndDisabled();
    };

    float fovDeg = XMConvertToDegrees(_editingPreset.camera.fovy);
    const float baseFovDeg = XMConvertToDegrees(basePreset.camera.fovy);
    const bool fovDifferent = hasBasePreset && Is_Different(_editingPreset.camera.fovy, basePreset.camera.fovy);
    Push_DiffInputColor(fovDifferent);
    ImGui::SetNextItemWidth(kPresetInputWidth);
    if (ImGui::DragFloat("시야각", &fovDeg, 0.1f, 10.f, 120.f, "%.2f"))
    {
        _editingPreset.camera.fovy = XMConvertToRadians(fovDeg);
        changed = true;
    }
    Pop_DiffInputColor(fovDifferent);
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasBasePreset || !fovDifferent);
    if (ImGui::SmallButton("기본##FOV Deg"))
    {
        _editingPreset.camera.fovy = XMConvertToRadians(baseFovDeg);
        changed = true;
    }
    ImGui::EndDisabled();

    const bool focusModeDifferent = hasBasePreset && _editingPreset.focusMode != basePreset.focusMode;
    Push_DiffInputColor(focusModeDifferent);
    if (Select_FocusModeCombo("Focus Mode", _editingPreset.focusMode, kPresetInputWidth))
        changed = true;
    Pop_DiffInputColor(focusModeDifferent);
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasBasePreset || !focusModeDifferent);
    if (ImGui::SmallButton("Base##Focus Mode"))
    {
        _editingPreset.focusMode = basePreset.focusMode;
        changed = true;
    }
    ImGui::EndDisabled();

    dragFloat("거리", _editingPreset.distance, basePreset.distance, 0.01f, 0.1f, 50.f);
    dragFloat("좌우 오프셋", _editingPreset.sideOffset, basePreset.sideOffset, 0.01f, -20.f, 20.f);
    dragFloat("높이 오프셋", _editingPreset.heightOffset, basePreset.heightOffset, 0.01f, -20.f, 20.f);
    dragFloat("조준 오프셋", _editingPreset.aimOffset, basePreset.aimOffset, 0.01f, -20.f, 20.f);
    dragFloat("바라볼 높이", _editingPreset.lookHeight, basePreset.lookHeight, 0.01f, -10.f, 20.f);
    dragFloat("피벗 높이", _editingPreset.pivotHeight, basePreset.pivotHeight, 0.01f, -10.f, 20.f);

    ImGui::Separator();
    dragFloat("요 오프셋", _editingPreset.yawOffset, basePreset.yawOffset, 0.05f, -360.f, 360.f);
    dragFloat("피치 오프셋", _editingPreset.pitchOffset, basePreset.pitchOffset, 0.05f, -89.f, 89.f);
    dragFloat("롤 오프셋", _editingPreset.rollOffset, basePreset.rollOffset, 0.001f, -XM_PI, XM_PI);

    ImGui::Separator();
    dragFloat("추적 속도", _editingPreset.followSpeed, basePreset.followSpeed, 0.01f, 0.f, 30.f);
    dragFloat("타겟 추적 속도", _editingPreset.targetFollowSpeed, basePreset.targetFollowSpeed, 0.01f, 0.f, 30.f);
    dragFloat("수직 추적 속도", _editingPreset.verticalFollowSpeed, basePreset.verticalFollowSpeed, 0.01f, 0.f, 30.f);

    const bool easingDifferent = hasBasePreset && _editingPreset.targetFollowEasing != basePreset.targetFollowEasing;
    Push_DiffInputColor(easingDifferent);
    if (Select_FollowEasingCombo("타겟 추적 곡선", _editingPreset.targetFollowEasing, kPresetInputWidth))
        changed = true;
    Pop_DiffInputColor(easingDifferent);
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasBasePreset || !easingDifferent);
    if (ImGui::SmallButton("기본##Target Follow Easing"))
    {
        _editingPreset.targetFollowEasing = basePreset.targetFollowEasing;
        changed = true;
    }
    ImGui::EndDisabled();

    dragFloat("근평면", _editingPreset.camera.nearPlane, basePreset.camera.nearPlane, 0.001f, 0.001f, 10.f);
    dragFloat("원평면", _editingPreset.camera.farPlane, basePreset.camera.farPlane, 1.f, 10.f, 5000.f, "%.1f");

    checkboxField("진입 시 즉시 스냅", _editingPreset.snapOnEnter, basePreset.snapOnEnter);
    checkboxField("마우스 회전 허용", _editingPreset.allowMouseRotation, basePreset.allowMouseRotation);

    ImGui::Separator();
    if (ImGui::Button("기본값으로 리셋"))
        Reset_CurrentPresetFromBase();

    ImGui::SameLine();
    if (ImGui::Button("런타임 적용##PresetPanel"))
        Apply_RuntimePreset();

    if (changed)
    {
        if (!Same_Preset(beforeEditPreset, _editingPreset))
            Push_UndoSnapshot(beforeEditPreset);

        if (_viewingSavedPreset)
        {
            _viewingSavedPreset = false;
            _statusMessage = "저장값 프리뷰를 편집값으로 전환";
        }

        _presetDirty = true;
        if (_runtimeFollowEnabled)
            Apply_RuntimePreset();
    }

    ImGui::EndChild();
}

void SoulLikeCamera_Viewer::Render_PresetBrowser()
{
    Refresh_PresetBrowser();

    ImGui::TextDisabled("프리셋 브라우저");

    char searchBuffer[128] = {};
    strncpy_s(searchBuffer, _presetBrowserSearch.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::InputText("검색", searchBuffer, IM_ARRAYSIZE(searchBuffer)))
        _presetBrowserSearch = searchBuffer;

    if (ImGui::Button("목록 새로고침"))
    {
        _presetBrowserDirty = true;
        Refresh_PresetBrowser();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(
        _selectedPresetBrowserIndex < 0 ||
        _selectedPresetBrowserIndex >= static_cast<int>(_presetBrowserKeys.size()));
    if (ImGui::Button("선택 항목 불러오기"))
        Load_BrowserPreset(_presetBrowserKeys[_selectedPresetBrowserIndex]);
    ImGui::EndDisabled();

    if (ImGui::BeginChild("SoulLikePresetBrowserList", ImVec2(0.f, 150.f), true))
    {
        for (int i = 0; i < static_cast<int>(_presetBrowserKeys.size()); ++i)
        {
            const string& presetKey = _presetBrowserKeys[i];
            const string displayName = presetKey.empty() ? "(기본)" : presetKey;
            if (!Contains_CaseInsensitive(displayName, _presetBrowserSearch))
                continue;

            const bool selected = i == _selectedPresetBrowserIndex;
            if (ImGui::Selectable(displayName.c_str(), selected))
                _selectedPresetBrowserIndex = i;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                Load_BrowserPreset(presetKey);
        }
    }
    ImGui::EndChild();
}

void SoulLikeCamera_Viewer::Render_TimelinePanel()
{
    Render_ClipTimelinePanel();
    return;

    ImGui::BeginChild("SoulLikeCameraViewerTimeline", ImVec2(0.f, 0.f), true);

    const float duration = Resolve_CurrentAnimationDurationSec();
    if (ImGui::Button(_timelinePlaying ? "일시정지" : "재생"))
        _timelinePlaying = !_timelinePlaying;

    ImGui::SameLine();
    if (ImGui::Button("처음"))
    {
        _previewTimeSec = 0.f;
        _timelinePlaying = false;
    }

    ImGui::SameLine();
    ImGui::Text("시간 %.3f / %.3f", _previewTimeSec, duration);

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##SoulLikeCameraViewerTime", &_previewTimeSec, 0.f, std::max(0.001f, duration), "%.3f s");

    ImGui::EndChild();
}

void SoulLikeCamera_Viewer::Render_ClipTimelinePanel()
{
    ImGui::BeginChild("SoulLikeCameraViewerClipTimeline", ImVec2(0.f, 0.f), true);

    const float duration = std::max(0.001f, Resolve_CurrentAnimationDurationSec());
    _editingClip.duration = duration;
    _editingClip.timeMode = BattleCameraClipTimeMode::Independent;

    Handle_ClipShortcuts();

    if (ImGui::Button(_timelinePlaying ? "일시정지" : "재생"))
        _timelinePlaying = !_timelinePlaying;

    ImGui::SameLine();
    if (ImGui::Button("처음"))
    {
        _previewTimeSec = 0.f;
        _clipCurrentFrame = 0;
        _timelinePlaying = false;
    }

    ImGui::SameLine();
    ImGui::Text("시간 %.3f / %.3f", _previewTimeSec, duration);

    ImGui::SameLine();
    if (ImGui::Button("현재 구도 키 삽입(K)"))
        Insert_CurrentPoseKeysAtFrame(_clipCurrentFrame);

    ImGui::SameLine();
    ImGui::BeginDisabled(_clipSelectedKeyIndex < 0);
    if (ImGui::Button("선택 키 삭제(Delete)"))
        Remove_SelectedClipKey();
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("클립 로드"))
        Load_CurrentClip();

    ImGui::SameLine();
    if (ImGui::Button("클립 저장"))
        Save_CurrentClip();

    ImGui::SameLine();
    if (ImGui::Button("클립 초기화"))
    {
        Reset_CurrentClip();
        _clipDirty = true;
    }

    ImGui::SameLine();
    if (_clipDirty)
        ImGui::TextColored(ImVec4(1.f, 0.72f, 0.35f, 1.f), "클립 수정됨");
    else
        ImGui::TextDisabled("클립 저장됨");

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##SoulLikeCameraViewerClipTime", &_previewTimeSec, 0.f, duration, "%.3f s"))
        Sync_ClipFrameFromPreviewTime();

    Render_ClipSequencer();
    Render_SelectedClipKeyEditor();

    ImGui::EndChild();
}

void SoulLikeCamera_Viewer::Render_ClipSequencer()
{
    bool consumedKeyClick = false;
    bool requestOpenInsertPopup = false;
    bool requestRemoveKeyPopup = false;
    bool previewDirty = false;
    int sequencerSelectedTrackIndex = _clipSelectedTrackIndex;
    int displayFrameMax = Get_ClipFrameMax();
    const int previousFrame = _clipCurrentFrame;
    const int previousTrackIndex = _clipSelectedTrackIndex;
    const int previousKeyIndex = _clipSelectedKeyIndex;

    BattleClipSequencerAdapter adapter(
        &_editingClip,
        &_clipSelectedTrackIndex,
        &_clipSelectedKeyIndex,
        &_clipCurrentFrame,
        &consumedKeyClick,
        &requestOpenInsertPopup,
        &requestRemoveKeyPopup,
        &_clipContextTrackIndex,
        &_clipContextFrame,
        &displayFrameMax,
        false,
        &previewDirty,
        &_clipDistanceEnabled,
        &_clipFovEnabled,
        &_clipShakeEnabled,
        &_clipSideEnabled,
        &_clipPivotEnabled,
        &_clipHeightEnabled,
        &_clipAimEnabled,
        &_clipLookHeightEnabled,
        &_clipYawEnabled,
        &_clipOrbitYawEnabled,
        &_clipOrbitRadiusEnabled,
        &_clipOrbitHeightEnabled,
        &_clipOrbitLookHeightEnabled,
        &_clipShakeFrequencyXEnabled,
        &_clipShakeFrequencyYEnabled,
        &_clipShakeDecayRateXEnabled,
        &_clipShakeDecayRateEnabled,
        &_clipShakeVerticalRatioEnabled,
        &_clipShakeStrengthScaleEnabled);
    adapter.Set_UseDisplayFrameTimeScale(true);

    static bool expanded = true;
    static int firstFrame = 0;
    const ImVec2 sequencerMin = ImGui::GetCursorScreenPos();
    const float sequencerWidth = ImGui::GetContentRegionAvail().x;
    ImSequencer::Sequencer(&adapter, &_clipCurrentFrame, &expanded, &sequencerSelectedTrackIndex, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);
    const ImVec2 sequencerMax = ImVec2(sequencerMin.x + sequencerWidth, ImGui::GetCursorScreenPos().y);

    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    const bool sequencerHovered =
        mousePos.x >= sequencerMin.x && mousePos.x <= sequencerMax.x &&
        mousePos.y >= sequencerMin.y && mousePos.y <= sequencerMax.y;
    if (sequencerHovered && io.KeyShift && io.MouseWheel != 0.f)
    {
        const int scrollStep = std::max(1, displayFrameMax / 20);
        firstFrame = std::clamp(
            firstFrame - static_cast<int>(std::lround(io.MouseWheel * static_cast<float>(scrollStep))),
            0,
            displayFrameMax);
    }

    if (!consumedKeyClick)
        _clipSelectedTrackIndex = sequencerSelectedTrackIndex;

    if (requestOpenInsertPopup)
        Insert_ClipKeyAtFrame(_clipContextTrackIndex, _clipContextFrame);

    if (requestRemoveKeyPopup)
        Remove_SelectedClipKey();

    if (previousFrame != _clipCurrentFrame ||
        previousTrackIndex != _clipSelectedTrackIndex ||
        previousKeyIndex != _clipSelectedKeyIndex)
    {
        Sync_PreviewTimeFromClipFrame();
    }

    if (previewDirty)
        _clipDirty = true;
}

void SoulLikeCamera_Viewer::Render_SelectedClipKeyEditor()
{
    CameraTrack* track = Get_ClipTrack(_clipSelectedTrackIndex);
    if (track == nullptr ||
        _clipSelectedKeyIndex < 0 ||
        _clipSelectedKeyIndex >= static_cast<int>(track->keyframes.size()))
    {
        ImGui::TextDisabled("선택된 키 없음");
        return;
    }

    CameraKeyframe& keyframe = track->keyframes[_clipSelectedKeyIndex];
    bool changed = false;

    ImGui::Separator();
    ImGui::Text("선택 키 Track %d / Key %d", _clipSelectedTrackIndex, _clipSelectedKeyIndex);

    ImGui::SetNextItemWidth(160.f);
    if (ImGui::DragFloat("키 시간", &keyframe.time, 0.01f, 0.f, std::max(0.001f, _editingClip.duration)))
    {
        keyframe.time = std::clamp(keyframe.time, 0.f, std::max(0.001f, _editingClip.duration));
        const float selectedTime = keyframe.time;
        Sort_ClipTrack(*track);
        for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
        {
            if (fabsf(track->keyframes[i].time - selectedTime) <= 0.0001f)
            {
                _clipSelectedKeyIndex = i;
                break;
            }
        }

        _clipCurrentFrame = Get_ClipFrameFromTime(selectedTime);
        Sync_PreviewTimeFromClipFrame();
        changed = true;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.f);
    if (ImGui::DragFloat("키 값", &keyframe.value, 0.01f))
        changed = true;

    ImGui::SameLine();
    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    ImGui::SetNextItemWidth(180.f);
    if (ImGui::BeginCombo("보간", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool selected = keyframe.interpolation == mode;
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), selected))
            {
                keyframe.interpolation = mode;
                changed = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (changed)
        _clipDirty = true;
}

void SoulLikeCamera_Viewer::Load_CurrentPreset(bool loadBaseOnly)
{
    const TargetCameraPreset beforeLoadPreset = _editingPreset;

    _editingPreset = TargetCameraPreset{};
    const bool baseLoaded = Client::CameraPresetIO::LoadSoulLikePresetFromFile("", _editingPreset);

    const string presetKey = Build_CurrentPresetKey();
    bool profileLoaded = false;
    if (!loadBaseOnly && !presetKey.empty())
        profileLoaded = Client::CameraPresetIO::LoadSoulLikePresetFromFile(presetKey, _editingPreset);

    _loadedPresetKey = loadBaseOnly ? "" : presetKey;
    _presetDirty = false;
    _viewingSavedPreset = false;

    if (baseLoaded && !Same_Preset(beforeLoadPreset, _editingPreset))
        Push_UndoSnapshot(beforeLoadPreset);

    if (!baseLoaded)
        _statusMessage = "기본 프리셋 파일 없음";
    else if (!loadBaseOnly && !presetKey.empty() && !profileLoaded)
        _statusMessage = "개별 프리셋 없음, 기본값으로 로드";
    else
        _statusMessage = "프리셋 로드 완료";
    Load_CurrentClip();
}

void SoulLikeCamera_Viewer::Load_CurrentClip()
{
    const string clipKey = Build_CurrentClipKey();
    Reset_CurrentClip();

    if (Client::BattleCameraClipIO::LoadSoulLikeCameraClipFromFile(clipKey, _editingClip))
    {
        _loadedClipKey = clipKey;
        _clipDirty = false;
        _clipCurrentFrame = std::clamp(_clipCurrentFrame, 0, Get_ClipFrameMax());
        Sync_PreviewTimeFromClipFrame();
        return;
    }

    _loadedClipKey = clipKey;
    _clipDirty = false;
}

void SoulLikeCamera_Viewer::Save_CurrentClip()
{
    Save_CurrentClip(Build_CurrentClipKey());
}

void SoulLikeCamera_Viewer::Save_CurrentClip(const string& clipKey)
{
    _editingClip.duration = std::max(0.001f, Resolve_CurrentAnimationDurationSec());
    _editingClip.timeMode = BattleCameraClipTimeMode::Independent;
    Client::BattleCameraClipIO::SaveSoulLikeCameraClipToFile(clipKey, _editingClip);
    _loadedClipKey = clipKey;
    _clipDirty = false;
}

void SoulLikeCamera_Viewer::Reset_CurrentClip()
{
    _editingClip = BattleCameraClip{ std::max(0.001f, Resolve_CurrentAnimationDurationSec()) };
    _editingClip.timeMode = BattleCameraClipTimeMode::Independent;
    _clipCurrentFrame = std::clamp(_clipCurrentFrame, 0, Get_ClipFrameMax());
    _clipSelectedTrackIndex = 0;
    _clipSelectedKeyIndex = -1;
}

string SoulLikeCamera_Viewer::Build_CurrentClipKey() const
{
    return Build_CurrentPresetKey();
}

void SoulLikeCamera_Viewer::Save_CurrentPreset()
{
    Save_CurrentPreset(Build_CurrentPresetKey());
}

void SoulLikeCamera_Viewer::Save_CurrentPreset(const string& presetKey)
{
    Client::CameraPresetIO::SaveSoulLikePresetToFile(presetKey, _editingPreset);
    _loadedPresetKey = presetKey;
    _presetDirty = false;
    _viewingSavedPreset = false;
    _presetBrowserDirty = true;
    _statusMessage = "프리셋 저장 완료";
    Save_CurrentClip(presetKey);
}

string SoulLikeCamera_Viewer::Build_CurrentPresetKey() const
{
    switch (_profileType)
    {
    case CameraProfileType::Base:
        return {};

    case CameraProfileType::PlayerSkill:
        if (_selectedSkillIndex >= 0 && _selectedSkillIndex < static_cast<int>(_skillKeys.size()))
            return "PlayerSkill_" + _skillKeys[_selectedSkillIndex];
        return "PlayerSkill_None";

    case CameraProfileType::MonsterAttack:
        if (_selectedMonsterAnimationIndex >= 0 && _selectedMonsterAnimationIndex < static_cast<int>(_monsterAnimationNames.size()))
            return "MonsterAttack_" + _monsterPrefab + "_" + _monsterAnimationNames[_selectedMonsterAnimationIndex];
        return "MonsterAttack_" + _monsterPrefab + "_None";

    case CameraProfileType::Parry:
        return "Parry";
    case CameraProfileType::PlayerHit:
        return "PlayerHit";
    case CameraProfileType::JumpCounter:
        return "JumpCounter";
    default:
        return {};
    }
}

void SoulLikeCamera_Viewer::Refresh_PresetBrowser()
{
    if (!_presetBrowserDirty)
        return;

    _presetBrowserKeys.clear();

    const fs::path presetDirectory = Get_SoulLikePresetDirectory();
    if (fs::exists(presetDirectory))
    {
        for (const fs::directory_entry& entry : fs::directory_iterator(presetDirectory))
        {
            if (!entry.is_regular_file())
                continue;

            string presetKey{};
            if (Try_ExtractSoulLikePresetKey(entry.path(), presetKey))
                _presetBrowserKeys.push_back(presetKey);
        }
    }

    std::sort(_presetBrowserKeys.begin(), _presetBrowserKeys.end(),
        [](const string& lhs, const string& rhs)
        {
            if (lhs.empty() && rhs.empty())
                return false;
            if (lhs.empty())
                return true;
            if (rhs.empty())
                return false;
            return lhs < rhs;
        });

    _selectedPresetBrowserIndex = _presetBrowserKeys.empty()
        ? -1
        : std::clamp(_selectedPresetBrowserIndex, 0, static_cast<int>(_presetBrowserKeys.size()) - 1);

    _presetBrowserDirty = false;
}

void SoulLikeCamera_Viewer::Load_BrowserPreset(const string& presetKey)
{
    TargetCameraPreset loadedPreset{};
    if (!Client::CameraPresetIO::LoadSoulLikePresetFromFile("", loadedPreset))
    {
        _statusMessage = "기본 프리셋 파일 없음";
        return;
    }

    if (!presetKey.empty() &&
        !Client::CameraPresetIO::LoadSoulLikePresetFromFile(presetKey, loadedPreset))
    {
        _statusMessage = "선택한 프리셋 파일 없음";
        return;
    }

    if (!Same_Preset(_editingPreset, loadedPreset))
        Push_UndoSnapshot(_editingPreset);

    _editingPreset = loadedPreset;
    _loadedPresetKey = presetKey;
    _copyPresetKey = presetKey;
    _presetDirty = false;
    _viewingSavedPreset = false;

    Apply_BrowserPresetContext(presetKey);
    Load_CurrentClip();

    if (_runtimeFollowEnabled)
        Apply_RuntimePreset();

    _statusMessage = presetKey.empty()
        ? "브라우저 프리셋 로드: 기본"
        : "브라우저 프리셋 로드: " + presetKey;
}

void SoulLikeCamera_Viewer::Apply_BrowserPresetContext(const string& presetKey)
{
    if (presetKey.empty())
    {
        _profileType = CameraProfileType::Base;
        return;
    }

    constexpr const char* playerSkillPrefix = "PlayerSkill_";
    constexpr const char* monsterAttackPrefix = "MonsterAttack_";

    if (presetKey.rfind(playerSkillPrefix, 0) == 0)
    {
        const string skillKey = presetKey.substr(strlen(playerSkillPrefix));
        auto iter = std::find(_skillKeys.begin(), _skillKeys.end(), skillKey);
        if (iter == _skillKeys.end())
        {
            _skillKeys.push_back(skillKey);
            _selectedSkillIndex = static_cast<int>(_skillKeys.size()) - 1;
        }
        else
        {
            _selectedSkillIndex = static_cast<int>(std::distance(_skillKeys.begin(), iter));
        }

        _profileType = CameraProfileType::PlayerSkill;
        return;
    }

    if (presetKey == "Parry")
    {
        _profileType = CameraProfileType::Parry;
        return;
    }

    if (presetKey == "PlayerHit")
    {
        _profileType = CameraProfileType::PlayerHit;
        return;
    }

    if (presetKey == "JumpCounter")
    {
        _profileType = CameraProfileType::JumpCounter;
        return;
    }

    if (presetKey.rfind(monsterAttackPrefix, 0) == 0)
    {
        _profileType = CameraProfileType::MonsterAttack;

        Refresh_MonsterAnimationList();
        const string body = presetKey.substr(strlen(monsterAttackPrefix));
        for (int i = 0; i < static_cast<int>(_monsterAnimationNames.size()); ++i)
        {
            const string suffix = "_" + _monsterAnimationNames[i];
            if (body.size() < suffix.size())
                continue;

            if (body.compare(body.size() - suffix.size(), suffix.size(), suffix) != 0)
                continue;

            _monsterPrefab = body.substr(0, body.size() - suffix.size());
            _selectedMonsterAnimationIndex = i;
            _previewActorsDirty = true;
            return;
        }
    }
}

bool SoulLikeCamera_Viewer::Load_CurrentPresetFile(TargetCameraPreset& outPreset) const
{
    outPreset = TargetCameraPreset{};
    if (!Client::CameraPresetIO::LoadSoulLikePresetFromFile("", outPreset))
        return false;

    const string presetKey = Build_CurrentPresetKey();
    if (presetKey.empty())
        return true;

    return Client::CameraPresetIO::LoadSoulLikePresetFromFile(presetKey, outPreset);
}

bool SoulLikeCamera_Viewer::Current_PresetFileExists() const
{
    TargetCameraPreset preset{};
    return Load_CurrentPresetFile(preset);
}

Shared<Client::SoulLikeCamera> SoulLikeCamera_Viewer::Resolve_RuntimeSoulLikeCamera() const
{
    if (Shared<Client::SoulLikeCamera> soulLikeCamera =
        dynamic_pointer_cast<Client::SoulLikeCamera>(GAME->Get_ActiveCamera()))
    {
        return soulLikeCamera;
    }

    return dynamic_pointer_cast<Client::SoulLikeCamera>(
        GAME->Get_CameraByType(ObjectType::SoulLikeCamera));
}

void SoulLikeCamera_Viewer::Capture_RuntimePreset()
{
    const Shared<Client::SoulLikeCamera> soulLikeCamera = Resolve_RuntimeSoulLikeCamera();
    if (soulLikeCamera == nullptr)
    {
        _statusMessage = "런타임 소울라이크 카메라 없음";
        return;
    }

    Push_UndoSnapshot(_editingPreset);
    _editingPreset = soulLikeCamera->Get_Preset();
    Capture_RuntimeAnimationPose();
    _presetDirty = true;
    _viewingSavedPreset = false;
    _statusMessage = "런타임 프리셋 캡처 완료";
}

bool SoulLikeCamera_Viewer::Capture_RuntimeAnimationPose()
{
    const Shared<Client::SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
        return false;

    Shared<ModelCom> runtimeModel = nullptr;
    vector<string>* animationNames = nullptr;
    int* selectedAnimationIndex = nullptr;

    if (_profileType == CameraProfileType::MonsterAttack)
    {
        runtimeModel = Resolve_PrimaryModel(controller->Get_Monster());
        Refresh_MonsterAnimationList();
        animationNames = &_monsterAnimationNames;
        selectedAnimationIndex = &_selectedMonsterAnimationIndex;
    }
    else
    {
        runtimeModel = Resolve_PrimaryModel(controller->Get_Player());
        Refresh_PlayerAnimationList();
        animationNames = &_playerAnimationNames;
        selectedAnimationIndex = &_selectedPlayerAnimationIndex;
    }

    if (runtimeModel == nullptr || animationNames == nullptr || selectedAnimationIndex == nullptr)
        return false;

    const char* runtimeAnimationName = runtimeModel->Get_AnimationName(runtimeModel->Get_CurrentAnimationIndex());
    if (runtimeAnimationName == nullptr || runtimeAnimationName[0] == '\0')
        return false;

    auto iter = std::find(animationNames->begin(), animationNames->end(), runtimeAnimationName);
    if (iter == animationNames->end())
    {
        animationNames->push_back(runtimeAnimationName);
        *selectedAnimationIndex = static_cast<int>(animationNames->size()) - 1;
    }
    else
    {
        *selectedAnimationIndex = static_cast<int>(std::distance(animationNames->begin(), iter));
    }

    const float durationSec = std::max(0.f, runtimeModel->Get_CurrentAnimationDurationSec());
    _previewTimeSec = durationSec > 0.f
        ? std::clamp(runtimeModel->Get_CurrentAnimationTimeSec(), 0.f, durationSec)
        : std::max(0.f, runtimeModel->Get_CurrentAnimationTimeSec());
    Sync_ClipFrameFromPreviewTime();
    Apply_PreviewAnimation(0.f);
    Apply_SoulLikeCameraPreviewPose();

    return true;
}

void SoulLikeCamera_Viewer::Apply_RuntimePreset()
{
    const Shared<Client::SoulLikeCamera> soulLikeCamera = Resolve_RuntimeSoulLikeCamera();
    if (soulLikeCamera == nullptr)
    {
        _statusMessage = "런타임 소울라이크 카메라 없음";
        return;
    }

    soulLikeCamera->Apply_PreviewPreset(_editingPreset);
    _statusMessage = "런타임 카메라 적용 완료";
}

void SoulLikeCamera_Viewer::Reset_CurrentPresetFromBase()
{
    TargetCameraPreset basePreset{};
    if (!Client::CameraPresetIO::LoadSoulLikePresetFromFile("", basePreset))
    {
        _statusMessage = "기본 프리셋 파일 없음";
        return;
    }

    Push_UndoSnapshot(_editingPreset);
    _editingPreset = basePreset;
    _viewingSavedPreset = false;
    _presetDirty = _profileType != CameraProfileType::Base;
    _statusMessage = _profileType == CameraProfileType::Base
        ? "기본 프리셋 다시 로드"
        : "현재 프리셋을 기본값으로 리셋";

    if (_runtimeFollowEnabled)
        Apply_RuntimePreset();
}

void SoulLikeCamera_Viewer::Toggle_SavedComparison()
{
    if (_viewingSavedPreset)
    {
        _editingPreset = _compareEditPreset;
        _presetDirty = _comparePresetDirty;
        _viewingSavedPreset = false;
        _statusMessage = "편집값으로 복귀";

        if (_runtimeFollowEnabled)
            Apply_RuntimePreset();

        return;
    }

    TargetCameraPreset savedPreset{};
    if (!Load_CurrentPresetFile(savedPreset))
    {
        _statusMessage = "저장된 프리셋 파일 없음";
        return;
    }

    _compareEditPreset = _editingPreset;
    _comparePresetDirty = _presetDirty;
    _editingPreset = savedPreset;
    _viewingSavedPreset = true;
    _presetDirty = false;
    _statusMessage = "저장값 보는 중";

    if (_runtimeFollowEnabled)
        Apply_RuntimePreset();
}

void SoulLikeCamera_Viewer::Save_CopyPreset()
{
    if (_copyPresetKey.empty())
    {
        _statusMessage = "복사 키가 비어 있음";
        return;
    }

    Client::CameraPresetIO::SaveSoulLikePresetToFile(_copyPresetKey, _editingPreset);
    _presetBrowserDirty = true;
    _statusMessage = "복사 저장 완료";
}

void SoulLikeCamera_Viewer::Set_PreviewPose(float monsterDistance, float monsterSideOffset)
{
    _previewMonsterDistance = monsterDistance;
    _previewMonsterSideOffset = monsterSideOffset;
}

bool SoulLikeCamera_Viewer::Sync_CurrentRuntimeSkill()
{
    const Shared<Client::SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
    {
        _statusMessage = "소울라이크 컨트롤러 없음";
        return false;
    }

    const Shared<Client::SL_Maelle> player = controller->Get_Player();
    if (player == nullptr)
    {
        _statusMessage = "런타임 소울라이크 플레이어 없음";
        return false;
    }

    const Shared<Client::SL_StateMachineCom> stateMachine = player->Get_Component<Client::SL_StateMachineCom>();
    if (stateMachine == nullptr ||
        stateMachine->Get_CurrentSoulLikeState() != Client::SL_PlayerState::Skill)
    {
        _statusMessage = "런타임 플레이어가 스킬 사용 중이 아님";
        return false;
    }

    const Shared<Client::SL_Skill> skillState =
        dynamic_pointer_cast<Client::SL_Skill>(stateMachine->Get_State(Client::SL_PlayerState::Skill));
    if (skillState == nullptr || skillState->Get_CurrentSkillKey().empty())
    {
        _statusMessage = "런타임 스킬 키 없음";
        return false;
    }

    const string& skillKey = skillState->Get_CurrentSkillKey();
    auto iter = std::find(_skillKeys.begin(), _skillKeys.end(), skillKey);
    if (iter == _skillKeys.end())
    {
        _skillKeys.push_back(skillKey);
        _selectedSkillIndex = static_cast<int>(_skillKeys.size()) - 1;
    }
    else
    {
        const int nextIndex = static_cast<int>(std::distance(_skillKeys.begin(), iter));
        if (_selectedSkillIndex == nextIndex && _profileType == CameraProfileType::PlayerSkill)
        {
            _statusMessage = "런타임 스킬 이미 선택됨: " + skillKey;
            return true;
        }

        _selectedSkillIndex = nextIndex;
    }

    _profileType = CameraProfileType::PlayerSkill;
    Load_CurrentPreset();
    _statusMessage = "런타임 스킬 동기화: " + skillKey;
    return true;
}

bool SoulLikeCamera_Viewer::Is_CurrentProfileRuntimeConnected() const
{
    return _profileType == CameraProfileType::Base ||
        _profileType == CameraProfileType::PlayerSkill ||
        _profileType == CameraProfileType::MonsterAttack;
}

void SoulLikeCamera_Viewer::Push_UndoSnapshot(const TargetCameraPreset& preset)
{
    if (Same_Preset(preset, _editingPreset))
        return;

    if (!_undoStack.empty() && Same_Preset(_undoStack.back(), preset))
        return;

    constexpr size_t maxUndoCount = 64;
    _undoStack.push_back(preset);
    if (_undoStack.size() > maxUndoCount)
        _undoStack.erase(_undoStack.begin());

    _redoStack.clear();
}

void SoulLikeCamera_Viewer::Undo_Preset()
{
    if (_undoStack.empty())
        return;

    _redoStack.push_back(_editingPreset);
    _editingPreset = _undoStack.back();
    _undoStack.pop_back();

    _presetDirty = true;
    _viewingSavedPreset = false;
    _statusMessage = "프리셋 되돌리기";

    if (_runtimeFollowEnabled)
        Apply_RuntimePreset();
}

void SoulLikeCamera_Viewer::Redo_Preset()
{
    if (_redoStack.empty())
        return;

    _undoStack.push_back(_editingPreset);
    _editingPreset = _redoStack.back();
    _redoStack.pop_back();

    _presetDirty = true;
    _viewingSavedPreset = false;
    _statusMessage = "프리셋 다시하기";

    if (_runtimeFollowEnabled)
        Apply_RuntimePreset();
}

void SoulLikeCamera_Viewer::Handle_ClipShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (!_isFocused || io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
    {
        Undo_Preset();
        return;
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
    {
        Redo_Preset();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_K))
        Insert_CurrentPoseKeysAtFrame(_clipCurrentFrame);

    if (ImGui::IsKeyPressed(ImGuiKey_Space))
        _timelinePlaying = !_timelinePlaying;

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        Remove_SelectedClipKey();
}

int SoulLikeCamera_Viewer::Get_ClipFrameMax() const
{
    return std::max(1, static_cast<int>(std::lround(std::max(0.001f, _editingClip.duration) * static_cast<float>(Resolve_CurrentAnimationFps()))));
}

float SoulLikeCamera_Viewer::Get_ClipTimeFromFrame(int frame) const
{
    const int frameMax = Get_ClipFrameMax();
    const float duration = std::max(0.001f, _editingClip.duration);
    const int clampedFrame = std::clamp(frame, 0, frameMax);
    return duration * static_cast<float>(clampedFrame) / static_cast<float>(frameMax);
}

int SoulLikeCamera_Viewer::Get_ClipFrameFromTime(float timeSec) const
{
    const int frameMax = Get_ClipFrameMax();
    const float duration = std::max(0.001f, _editingClip.duration);
    const float clampedTime = std::clamp(timeSec, 0.f, duration);
    return std::clamp(static_cast<int>(std::lround(clampedTime / duration * static_cast<float>(frameMax))), 0, frameMax);
}

void SoulLikeCamera_Viewer::Sync_ClipFrameFromPreviewTime()
{
    _clipCurrentFrame = Get_ClipFrameFromTime(_previewTimeSec);
}

void SoulLikeCamera_Viewer::Sync_PreviewTimeFromClipFrame()
{
    _clipCurrentFrame = std::clamp(_clipCurrentFrame, 0, Get_ClipFrameMax());
    _previewTimeSec = Get_ClipTimeFromFrame(_clipCurrentFrame);
}

CameraTrack* SoulLikeCamera_Viewer::Get_ClipTrack(int trackIndex)
{
    switch (trackIndex)
    {
    case 0:
        return &_editingClip.distanceOffset;
    case 1:
        return &_editingClip.fovOffset;
    case 2:
        return &_editingClip.shakeAmplitude;
    case 3:
        return &_editingClip.sideOffset;
    case 4:
        return &_editingClip.pivotOffset;
    case 5:
        return &_editingClip.heightOffset;
    case 6:
        return &_editingClip.aimOffset;
    case 7:
        return &_editingClip.lookHeightOffset;
    case 8:
        return &_editingClip.yawOffset;
    case 9:
        return &_editingClip.orbitYaw;
    case 10:
        return &_editingClip.orbitRadius;
    case 11:
        return &_editingClip.orbitHeight;
    case 12:
        return &_editingClip.orbitLookHeight;
    case 13:
        return &_editingClip.shakeFrequencyX;
    case 14:
        return &_editingClip.shakeFrequencyY;
    case 15:
        return &_editingClip.shakeDecayRateX;
    case 16:
        return &_editingClip.shakeDecayRate;
    case 17:
        return &_editingClip.shakeVerticalRatio;
    case 18:
        return &_editingClip.shakeStrengthScale;
    default:
        return nullptr;
    }
}

const CameraTrack* SoulLikeCamera_Viewer::Get_ClipTrack(int trackIndex) const
{
    return const_cast<SoulLikeCamera_Viewer*>(this)->Get_ClipTrack(trackIndex);
}

float SoulLikeCamera_Viewer::Get_CurrentClipTrackValue(int trackIndex) const
{
    switch (trackIndex)
    {
    case 0:
        return _editingPreset.distance;
    case 1:
        return _editingPreset.camera.fovy;
    case 2:
        return 0.f;
    case 3:
        return _editingPreset.sideOffset;
    case 4:
        return _editingPreset.pivotHeight;
    case 5:
        return _editingPreset.heightOffset;
    case 6:
        return _editingPreset.aimOffset;
    case 7:
        return _editingPreset.lookHeight;
    case 8:
        return _editingPreset.yawOffset;
    case 13:
        return 32.f;
    case 14:
        return 47.f;
    case 16:
        return 8.f;
    case 17:
        return 0.8f;
    case 18:
        return 0.05f;
    default:
        return 0.f;
    }
}

void SoulLikeCamera_Viewer::Sort_ClipTrack(CameraTrack& track)
{
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const CameraKeyframe& lhs, const CameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });
}

void SoulLikeCamera_Viewer::Insert_ClipKeyAtFrame(int trackIndex, int frame)
{
    CameraTrack* track = Get_ClipTrack(trackIndex);
    if (track == nullptr)
        return;

    const int targetFrame = std::clamp(frame, 0, Get_ClipFrameMax());
    const float keyTime = Get_ClipTimeFromFrame(targetFrame);
    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (Get_ClipFrameFromTime(track->keyframes[i].time) == targetFrame)
        {
            track->keyframes[i].time = keyTime;
            track->keyframes[i].value = Get_CurrentClipTrackValue(trackIndex);
            _clipSelectedTrackIndex = trackIndex;
            _clipSelectedKeyIndex = i;
            _clipCurrentFrame = targetFrame;
            _clipDirty = true;
            return;
        }
    }

    CameraKeyframe keyframe{};
    keyframe.time = keyTime;
    keyframe.value = Get_CurrentClipTrackValue(trackIndex);
    keyframe.interpolation = InterpolationMode::Linear;
    track->keyframes.push_back(keyframe);
    Sort_ClipTrack(*track);

    _clipSelectedTrackIndex = trackIndex;
    _clipCurrentFrame = targetFrame;
    _clipDirty = true;

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (fabsf(track->keyframes[i].time - keyTime) <= 0.0001f)
        {
            _clipSelectedKeyIndex = i;
            break;
        }
    }
}

void SoulLikeCamera_Viewer::Insert_CurrentPoseKeysAtFrame(int frame)
{
    const int poseTracks[] = { 0, 1, 3, 4, 5, 6, 7, 8 };
    for (int trackIndex : poseTracks)
        Insert_ClipKeyAtFrame(trackIndex, frame);

    _clipDirty = true;
}

bool SoulLikeCamera_Viewer::Update_CurrentPoseKeysAtFrame(int frame)
{
    const int targetFrame = std::clamp(frame, 0, Get_ClipFrameMax());
    const int poseTracks[] = { 0, 1, 3, 4, 5, 6, 7, 8 };
    bool changed = false;

    for (int trackIndex : poseTracks)
    {
        CameraTrack* track = Get_ClipTrack(trackIndex);
        if (track == nullptr)
            continue;

        for (CameraKeyframe& keyframe : track->keyframes)
        {
            if (Get_ClipFrameFromTime(keyframe.time) != targetFrame)
                continue;

            const float nextValue = Get_CurrentClipTrackValue(trackIndex);
            if (fabsf(keyframe.value - nextValue) <= 0.0001f)
                break;

            keyframe.value = nextValue;
            changed = true;
            break;
        }
    }

    return changed;
}

void SoulLikeCamera_Viewer::Remove_SelectedClipKey()
{
    CameraTrack* track = Get_ClipTrack(_clipSelectedTrackIndex);
    if (track == nullptr ||
        _clipSelectedKeyIndex < 0 ||
        _clipSelectedKeyIndex >= static_cast<int>(track->keyframes.size()))
    {
        return;
    }

    track->keyframes.erase(track->keyframes.begin() + _clipSelectedKeyIndex);
    _clipSelectedKeyIndex = std::clamp(_clipSelectedKeyIndex, -1, static_cast<int>(track->keyframes.size()) - 1);
    _clipDirty = true;
}

bool SoulLikeCamera_Viewer::Has_ClipTrackKeys(const CameraTrack& track, bool enabled) const
{
    return enabled && !track.keyframes.empty();
}

void SoulLikeCamera_Viewer::Apply_PreviewAnimation(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    Shared<ModelCom> playerModel = Resolve_PlayerModel();
    if (playerModel != nullptr &&
        _selectedPlayerAnimationIndex >= 0 &&
        _selectedPlayerAnimationIndex < static_cast<int>(_playerAnimationNames.size()))
    {
        const string& animationName = _playerAnimationNames[_selectedPlayerAnimationIndex];
        const char* currentName = playerModel->Get_AnimationName(playerModel->Get_CurrentAnimationIndex());
        if (currentName == nullptr || animationName != currentName)
            playerModel->Set_Animation(animationName, false, 0.f);

        playerModel->Set_AnimationPaused(true);
        playerModel->Set_CurrentAnimationTimeSec(_previewTimeSec);
        playerModel->Refresh_CurrentAnimationPose();
    }

    Shared<ModelCom> monsterModel = Resolve_MonsterModel();
    if (monsterModel != nullptr &&
        _selectedMonsterAnimationIndex >= 0 &&
        _selectedMonsterAnimationIndex < static_cast<int>(_monsterAnimationNames.size()))
    {
        const string& animationName = _monsterAnimationNames[_selectedMonsterAnimationIndex];
        const char* currentName = monsterModel->Get_AnimationName(monsterModel->Get_CurrentAnimationIndex());
        if (currentName == nullptr || animationName != currentName)
            monsterModel->Set_Animation(animationName, false, 0.f);

        monsterModel->Set_AnimationPaused(true);
        monsterModel->Set_CurrentAnimationTimeSec(_previewTimeSec);
        monsterModel->Refresh_CurrentAnimationPose();
    }
}

void SoulLikeCamera_Viewer::Apply_SoulLikeCameraPreviewPose()
{
    if (_previewCamera == nullptr)
        return;

    Vec3 subjectPos = kPlayerPreviewPosition;
    Shared<TransformCom> playerTransform = _previewPlayer != nullptr ? _previewPlayer->Get_Transform() : nullptr;
    Shared<TransformCom> monsterTransform = _previewMonster != nullptr ? _previewMonster->Get_Transform() : nullptr;
    if (playerTransform != nullptr)
        subjectPos = playerTransform->Get_WorldPosition();
    if (_editingPreset.focusMode == SoulLikeCameraFocusMode::PlayerOpponentMidpoint &&
        playerTransform != nullptr && monsterTransform != nullptr)
    {
        subjectPos = (playerTransform->Get_WorldPosition() + monsterTransform->Get_WorldPosition()) * 0.5f;
    }
    else if (_editingPreset.focusMode == SoulLikeCameraFocusMode::Opponent && monsterTransform != nullptr)
    {
        subjectPos = monsterTransform->Get_WorldPosition();
    }

    TargetCameraPreset previewPreset = _editingPreset;
    const float clipTimeSec = std::clamp(_previewTimeSec, 0.f, std::max(0.001f, _editingClip.duration));

    if (Has_ClipTrackKeys(_editingClip.distanceOffset, _clipDistanceEnabled))
        previewPreset.distance = Client::Sample_BattleCameraTrack(_editingClip.distanceOffset, clipTimeSec);
    if (Has_ClipTrackKeys(_editingClip.fovOffset, _clipFovEnabled))
        previewPreset.camera.fovy = Client::Sample_BattleCameraTrack(_editingClip.fovOffset, clipTimeSec);
    if (Has_ClipTrackKeys(_editingClip.sideOffset, _clipSideEnabled))
        previewPreset.sideOffset = Client::Sample_BattleCameraTrack(_editingClip.sideOffset, clipTimeSec);
    if (Has_ClipTrackKeys(_editingClip.pivotOffset, _clipPivotEnabled))
        previewPreset.pivotHeight = Client::Sample_BattleCameraTrack(_editingClip.pivotOffset, clipTimeSec);
    if (Has_ClipTrackKeys(_editingClip.heightOffset, _clipHeightEnabled))
        previewPreset.heightOffset = Client::Sample_BattleCameraTrack(_editingClip.heightOffset, clipTimeSec);
    if (Has_ClipTrackKeys(_editingClip.aimOffset, _clipAimEnabled))
        previewPreset.aimOffset = Client::Sample_BattleCameraTrack(_editingClip.aimOffset, clipTimeSec);
    if (Has_ClipTrackKeys(_editingClip.lookHeightOffset, _clipLookHeightEnabled))
        previewPreset.lookHeight = Client::Sample_BattleCameraTrack(_editingClip.lookHeightOffset, clipTimeSec);
    if (Has_ClipTrackKeys(_editingClip.yawOffset, _clipYawEnabled))
        previewPreset.yawOffset = Client::Sample_BattleCameraTrack(_editingClip.yawOffset, clipTimeSec);

    const float finalYaw = previewPreset.yawOffset;
    const float finalPitch = previewPreset.pitchOffset;

    const float yawRad = XMConvertToRadians(finalYaw);
    const float pitchRad = XMConvertToRadians(finalPitch);

    Vec3 backDir;
    backDir.x = -sinf(yawRad);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRad);
    backDir.Normalize();

    Vec3 rightDir;
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * previewPreset.distance;
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    const Vec3 pivotPos = subjectPos + Vec3::Up * previewPreset.pivotHeight;
    const Vec3 framingPos = pivotPos + rightDir * previewPreset.aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * previewPreset.heightOffset + rightDir * previewPreset.sideOffset;
    const Vec3 lookTarget = framingPos + Vec3::Up * previewPreset.lookHeight;

    if (Has_ClipTrackKeys(_editingClip.shakeAmplitude, _clipShakeEnabled))
    {
        const BattleCameraClipSample clipSample = Client::Sample_BattleCameraClip(_editingClip, clipTimeSec);
        desiredPos += Client::Sample_BattleCameraShakeOffset(
            clipSample.shakeAmplitude,
            clipSample.shakeVerticalRatio,
            clipSample.shakeStrengthScale,
            clipSample.shakeFrequencyX,
            clipSample.shakeFrequencyY,
            clipSample.shakeDecayRateX,
            clipSample.shakeDecayRate,
            clipTimeSec,
            rightDir,
            Vec3::Up);
    }

    _previewCamera->Set_FovY(previewPreset.camera.fovy);
    _previewCamera->Set_Near(previewPreset.camera.nearPlane);
    _previewCamera->Set_Far(previewPreset.camera.farPlane);

    if (Shared<TransformCom> transform = _previewCamera->Get_Transform())
    {
        transform->Set_WorldPosition(desiredPos);
        transform->LookAt(lookTarget);
        transform->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, previewPreset.rollOffset));
    }
}

void SoulLikeCamera_Viewer::Position_PreviewActors() const
{
    if (_previewPlayer != nullptr)
    {
        if (Shared<TransformCom> transform = _previewPlayer->Get_Transform())
        {
            transform->Set_WorldPosition(kPlayerPreviewPosition);
            transform->Set_WorldRotationEuler(Vec3(0.f, 0.f, 0.f));
        }
    }

    if (_previewMonster != nullptr)
    {
        if (Shared<TransformCom> transform = _previewMonster->Get_Transform())
        {
            const Vec3 monsterPosition = kPlayerPreviewPosition +
                Vec3::Right * _previewMonsterSideOffset +
                Vec3::Look * _previewMonsterDistance;
            transform->Set_WorldPosition(monsterPosition);
            transform->Set_WorldRotationEuler(Vec3(0.f, 180.f, 0.f));
        }
    }
}

void SoulLikeCamera_Viewer::Render_PreviewScene(float timeDelta)
{
    const Matrix savedView = *GAME->Get_Transform(D3DTS::View);
    const Matrix savedProj = *GAME->Get_Transform(D3DTS::Proj);
    const Shared<Camera> savedActiveCamera = GAME->Get_ActiveCamera();
    const uint32 savedViewportWidth = GAME->Get_ViewportWidth();
    const uint32 savedViewportHeight = GAME->Get_ViewportHeight();

    EDITOR->Apply_PreviewViewportSizeImmediate(_previewWidth, _previewHeight);

    GAME->Bind_BackBuffer();
    if (FAILED(GAME->Clear_Buffers(Vec4(0.f, 0.f, 0.f, 1.f))))
    {
        EDITOR->Apply_PreviewViewportSizeImmediate(savedViewportWidth, savedViewportHeight);
        return;
    }

    const Matrix previewView = _previewCamera->Get_ViewMatrix();
    const float aspect = static_cast<float>(_previewWidth) / static_cast<float>(std::max(1u, _previewHeight));
    const Matrix previewProj = XMMatrixPerspectiveFovLH(_previewCamera->Get_FovY(), aspect, _previewCamera->Get_Near(), _previewCamera->Get_Far());

    GAME->Change_ActiveCamera(_previewCamera);
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(previewView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(previewProj));
    GAME->UpdateFrustum(previewView, previewProj);

    Position_PreviewActors();

    if (_previewCamera != nullptr)
    {
        Shared<TransformCom> cameraTransform = _previewCamera->Get_Transform();
        if (cameraTransform != nullptr)
        {
            const Vec3 cameraPos = cameraTransform->Get_WorldPosition();
            for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
            {
                if (skyCloudCard == nullptr)
                    continue;

                if (Shared<TransformCom> skyTransform = skyCloudCard->Get_Transform())
                    skyTransform->Set_WorldPosition(cameraPos + kPreviewSkyCloudLocalOffset);
            }
        }
    }

    if (_previewPlayer != nullptr)
    {
        if (!_previewObjectsBegunPlay)
            _previewPlayer->Try_BeginPlay();

        _previewPlayer->Priority_Update(0.f);
        _previewPlayer->Update(0.f);
    }

    if (_previewMonster != nullptr)
    {
        if (!_previewObjectsBegunPlay)
            _previewMonster->Try_BeginPlay();

        _previewMonster->Priority_Update(0.f);
        _previewMonster->Update(0.f);
    }

    if (_previewSkyBox != nullptr)
    {
        if (!_previewObjectsBegunPlay)
            _previewSkyBox->Try_BeginPlay();

        _previewSkyBox->Set_Visible(true);
        _previewSkyBox->Priority_Update(timeDelta);
        _previewSkyBox->Update(timeDelta);
    }

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard == nullptr)
            continue;

        if (!_previewObjectsBegunPlay)
            skyCloudCard->Try_BeginPlay();

        skyCloudCard->Set_Visible(true);
        skyCloudCard->Priority_Update(timeDelta);
        skyCloudCard->Update(timeDelta);
    }

    _previewObjectsBegunPlay = true;

    GAME->Backup_RenderGroup();

    if (_previewPlayer != nullptr)
        _previewPlayer->Late_Update(0.f);

    if (_previewMonster != nullptr)
        _previewMonster->Late_Update(0.f);

    if (_previewSkyBox != nullptr)
        _previewSkyBox->Late_Update(timeDelta);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard != nullptr)
            skyCloudCard->Late_Update(timeDelta);
    }

    GAME->Draw_RendererOnly();
    GAME->Restore_RenderGroup();
    EDITOR->Capture_BackBuffer_ToRenderTarget(_previewRenderTarget);

    GAME->Bind_BackBuffer();
    if (savedActiveCamera != nullptr)
        GAME->Change_ActiveCamera(savedActiveCamera);
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
    GAME->UpdateFrustum(savedView, savedProj);
    EDITOR->Apply_PreviewViewportSizeImmediate(savedViewportWidth, savedViewportHeight);
}

float SoulLikeCamera_Viewer::Resolve_CurrentAnimationDurationSec() const
{
    Shared<ModelCom> model = nullptr;
    if (_profileType == CameraProfileType::MonsterAttack)
        model = Resolve_MonsterModel();
    else
        model = Resolve_PlayerModel();

    if (model == nullptr)
        return 0.f;

    return std::max(0.f, model->Get_CurrentAnimationDurationSec());
}

int SoulLikeCamera_Viewer::Resolve_CurrentAnimationFps() const
{
    Shared<ModelCom> model = nullptr;
    if (_profileType == CameraProfileType::MonsterAttack)
        model = Resolve_MonsterModel();
    else
        model = Resolve_PlayerModel();

    if (model == nullptr)
        return 60;

    if (const AnimNotifyData* animationData = model->Find_AnimNotifyDataByState(model->Get_AnimationStateIndex()))
        return std::max(1, animationData->displayFps);

    return 60;
}

Shared<ModelCom> SoulLikeCamera_Viewer::Resolve_PlayerModel() const
{
    return Resolve_PrimaryModel(_previewPlayer);
}

Shared<ModelCom> SoulLikeCamera_Viewer::Resolve_MonsterModel() const
{
    return Resolve_PrimaryModel(_previewMonster);
}

Shared<ModelCom> SoulLikeCamera_Viewer::Resolve_PrimaryModel(const Shared<GameObject>& object)
{
    if (object == nullptr)
        return nullptr;

    if (Shared<ModelCom> directModel = object->Get_Component<ModelCom>())
        return directModel;

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(object);
    if (container == nullptr)
        return nullptr;

    if (PartObject* body = container->Get_PartObject(L"Part_Body"))
    {
        if (Shared<ModelCom> bodyModel = body->Get_Component<ModelCom>())
            return bodyModel;
    }

    for (const auto& [tag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(tag);

        if (partObject == nullptr)
            continue;

        if (Shared<ModelCom> partModel = partObject->Get_Component<ModelCom>())
            return partModel;
    }

    return nullptr;
}

Shared<SoulLikeCamera_Viewer> SoulLikeCamera_Viewer::Create()
{
    return make_shared<SoulLikeCamera_Viewer>();
}

NS_END
