#include "BattleCameraPreview_View.h"
#include "BattleController.h"
#include "BattleCameraClipIO.h"
#include "BattleClipSequencerAdapter.h"
#include "BattleTurnCamera.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "Editor_Camera_Free.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "RenderTarget.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include "magic_enum/magic_enum.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
constexpr float kBattleClipPreviewFps = 30.f;

int Get_BattleClipFrameMax(const BattleCameraClip& clip)
{
    return std::max(0, static_cast<int>(std::lround(clip.duration * kBattleClipPreviewFps)));
}

float Sample_BattleCameraTrack(const BattleCameraTrack& track, float time)
{
    if (track.keyframes.empty())
        return 0.f;

    if (time <= track.keyframes.front().time)
        return track.keyframes.front().value;

    if (time >= track.keyframes.back().time)
        return track.keyframes.back().value;

    for (size_t i = 0; i + 1 < track.keyframes.size(); ++i)
    {
        const BattleCameraKeyframe& lhs = track.keyframes[i];
        const BattleCameraKeyframe& rhs = track.keyframes[i + 1];

        if (time < lhs.time || time > rhs.time)
            continue;

        const float range = std::max(rhs.time - lhs.time, 0.0001f);
        const float ratio = std::clamp((time - lhs.time) / range, 0.f, 1.f);
        return lhs.value + (rhs.value - lhs.value) * ratio;
    }

    return 0.f;
}

BattleCameraClipSample Sample_BattleCameraClip(const BattleCameraClip& clip, float time)
{
    const float sampleTime = std::clamp(time, 0.f, std::max(clip.duration, 0.f));

    BattleCameraClipSample sample{};
    sample.distanceOffset = Sample_BattleCameraTrack(clip.distanceOffset, sampleTime);
    sample.fovOffset = Sample_BattleCameraTrack(clip.fovOffset, sampleTime);
    sample.shakeAmplitude = Sample_BattleCameraTrack(clip.shakeAmplitude, sampleTime);
    return sample;
}

template <typename TEnum>
bool Select_EnumCombo(const char* label, TEnum& currentValue, float width = 160.f)
{
    bool changed = false;
    const char* currentLabel = magic_enum::enum_name(currentValue).data();

    ImGui::SetNextItemWidth(width);
    if (!ImGui::BeginCombo(label, currentLabel))
        return false;

    for (TEnum value : magic_enum::enum_values<TEnum>())
    {
        const bool isSelected = (currentValue == value);
        if (ImGui::Selectable(magic_enum::enum_name(value).data(), isSelected))
        {
            currentValue = value;
            changed = true;
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
    return changed;
}

void Apply_DefaultPreviewCameraTransform(const Shared<Editor_Camera_Free>& camera)
{
    if (camera == nullptr)
        return;

    Shared<TransformCom> cameraTransform = camera->Get_Transform();
    if (cameraTransform == nullptr)
        return;

    cameraTransform->Set_WorldPosition(Vec3(0.f, 2.2f, -6.f));
    cameraTransform->LookAt(Vec3(0.f, 1.f, 0.f));
}

}

BattleCameraPreview_View::BattleCameraPreview_View()
    : Editor_Window(L"Battle Camera Preview")
{
    Reset_DefaultFormation();
    _enemyPrefabNames = { "MB_Bishop", "MB_Lancelier", "MB_Portier", "MB_Volester" };
    Set_Open(false);
}

void BattleCameraPreview_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    if (_previewCamera != nullptr && _isPreviewHovered)
        _previewCamera->Priority_Update(timeDelta);

    if (_timelinePlaying)
    {
        _battleClipCurrentFrame += std::max(1, static_cast<int>(std::round(timeDelta * kBattleClipPreviewFps)));

        const int frameMax = Get_BattleClipFrameMax(_editingBattleClip);
        if (_battleClipCurrentFrame >= frameMax)
        {
            _battleClipCurrentFrame = frameMax;
            _timelinePlaying = false;
        }

        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
    }

    if (_autoPreview && _previewDirty)
    {
        Apply_PreviewContext();
        _previewDirty = false;
    }
}

void BattleCameraPreview_View::Pre_Render()
{
    if (!Is_Open())
        return;

    Ensure_PreviewCamera();
    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);

    if (_previewRenderTarget == nullptr || _previewCamera == nullptr)
        return;

    if (_previewObjectsDirty)
        Rebuild_PreviewObjects();

    Refresh_PreviewAnimationList();
    if (_previewDirty)
    {
        Apply_PreviewContext();
        _previewDirty = false;
    }

    Position_PreviewObjects();

    const Matrix savedView = *GAME->Get_Transform(D3DTS::View);
    const Matrix savedProj = *GAME->Get_Transform(D3DTS::Proj);

    if (FAILED(_previewRenderTarget->Begin(true, true)))
        return;

    const Matrix previewView = _previewCamera->Get_ViewMatrix();
    const float aspect = static_cast<float>(_previewWidth) / static_cast<float>(std::max(1u, _previewHeight));
    const Matrix previewProj = XMMatrixPerspectiveFovLH(_previewCamera->Get_FovY(), aspect, _previewCamera->Get_Near(), _previewCamera->Get_Far());

    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(previewView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(previewProj));
    GAME->UpdateFrustum(previewView, previewProj);

    const float dt = ImGui::GetIO().DeltaTime;
    if (_previewHeroObject != nullptr)
    {
        if (!_previewObjectsBegunPlay)
            _previewHeroObject->Try_BeginPlay();

        _previewHeroObject->Priority_Update(dt);
        _previewHeroObject->Update(dt);
    }

    if (_previewEnemyObject != nullptr)
    {
        if (!_previewObjectsBegunPlay)
            _previewEnemyObject->Try_BeginPlay();

        _previewEnemyObject->Priority_Update(dt);
        _previewEnemyObject->Update(dt);
    }

    _previewObjectsBegunPlay = true;

    GAME->Backup_RenderGroup();

    if (_previewHeroObject != nullptr)
        _previewHeroObject->Late_Update(dt);

    if (_previewEnemyObject != nullptr)
        _previewEnemyObject->Late_Update(dt);

    Position_PreviewObjects();

    GAME->Draw_Preview();
    GAME->Restore_RenderGroup();

    GAME->Bind_BackBuffer();
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
}

void BattleCameraPreview_View::Render()
{
    if (!Is_Open())
        return;

    Refresh_SkillList();
    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Render_Header();
        ImGui::Separator();

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float minTopSectionHeight = 280.f;
        const float maxTopSectionHeight = std::max(minTopSectionHeight, availableHeight - 170.f);
        _topSectionHeight = std::clamp(_topSectionHeight, minTopSectionHeight, maxTopSectionHeight);

        if (ImGui::BeginChild("BattleCameraPreviewTopSection", ImVec2(0.f, _topSectionHeight), false))
        {
            if (ImGui::BeginTable("##BattleCameraPreviewLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed, 320.f);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 330.f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                Render_ContextPanel();

                ImGui::TableSetColumnIndex(1);
                Render_PreviewCanvas();

                ImGui::TableSetColumnIndex(2);
                Render_PreviewPanel();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        const ImVec2 splitterPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("BattleCameraPreviewTopBottomSplitter", ImVec2(-FLT_MIN, 6.f));
        if (ImGui::IsItemActive())
            _topSectionHeight = std::clamp(_topSectionHeight + ImGui::GetIO().MouseDelta.y, minTopSectionHeight, maxTopSectionHeight);

        const bool splitterHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitterPos,
            ImVec2(splitterPos.x + ImGui::GetContentRegionAvail().x, splitterPos.y + 6.f),
            splitterHovered ? IM_COL32(110, 110, 118, 255) : IM_COL32(72, 72, 78, 255),
            2.f);

        Render_TimelinePanel();
    }

    if (!isOpen)
        Clear_PreviewContext();

    Set_Open(isOpen);
    ImGui::End();
}

void BattleCameraPreview_View::Refresh_SkillList()
{
    if (!_skillIds.empty())
        return;

    _skillIds = CLIENT->Get_AllSkillIds();

    if (!_skillIds.empty() && _selectedSkillIndex < 0)
        _selectedSkillIndex = 0;
}

void BattleCameraPreview_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
    width = std::max(1u, width);
    height = std::max(1u, height);

    if (_previewRenderTarget == nullptr)
    {
        _previewRenderTarget = RenderTarget::Create(
            L"BattleCameraPreviewRenderTarget",
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

void BattleCameraPreview_View::Ensure_PreviewCamera()
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
    desc.farPlane = 100.f;
    desc.yaw = 0.f;
    desc.pitch = 0.f;
    desc.mouseSensor = 0.15f;
    desc.speedPerSec = 5.f;
    desc.degreePerSec = 180.f;

    if (FAILED(_previewCamera->Initialize(&desc)))
    {
        _previewCamera = nullptr;
        return;
    }

    _previewCamera->Apply_EditorDesc(desc);
    Apply_DefaultPreviewCameraTransform(_previewCamera);
}

void BattleCameraPreview_View::Reset_DefaultFormation()
{
    _previewFormation.name = "Default Battle Layout";
    _previewFormation.origin = Vec3(22.f, 2.5f, -10.24f);

    _previewFormation.heroes =
    {
        PreviewSlot{ "Hero 0", Vec3(-3.f, 0.f, 0.f), Vec3::Zero },
        PreviewSlot{ "Hero 1", Vec3(0.f, 0.f, 0.f), Vec3::Zero },
        PreviewSlot{ "Hero 2", Vec3(3.f, 0.f, 0.f), Vec3::Zero },
    };

    _previewFormation.enemies =
    {
        PreviewSlot{ "Enemy 0", Vec3(-3.f, 0.f, 10.f), Vec3(0.f, 180.f, 0.f) },
        PreviewSlot{ "Enemy 1", Vec3(0.f, 0.f, 10.f), Vec3(0.f, 180.f, 0.f) },
        PreviewSlot{ "Enemy 2", Vec3(3.f, 0.f, 10.f), Vec3(0.f, 180.f, 0.f) },
    };
}

void BattleCameraPreview_View::Rebuild_PreviewObjects()
{
    Clear_PreviewObjects();

    const string heroPrefabName = Get_SelectedHeroPrefabName();
    const string enemyPrefabName = Get_SelectedEnemyPrefabName();

    if (!heroPrefabName.empty())
        _previewHeroObject = GAME->Instantiate_GameObjectJson(Client::Spawn_Prefab::Resolve_PrefabPath(heroPrefabName));

    if (!enemyPrefabName.empty())
        _previewEnemyObject = GAME->Instantiate_GameObjectJson(Client::Spawn_Prefab::Resolve_PrefabPath(enemyPrefabName));

    _previewObjectsDirty = false;
    _previewObjectsBegunPlay = false;
    _animationListDirty = true;
    _selectedPreviewAnimationIndex = -1;
    Position_PreviewObjects();
    _previewDirty = true;
}

void BattleCameraPreview_View::Clear_PreviewObjects()
{
    _previewHeroObject.reset();
    _previewEnemyObject.reset();
    _previewObjectsBegunPlay = false;
}

void BattleCameraPreview_View::Position_PreviewObjects() const
{
    PreviewSlotSide heroSide = PreviewSlotSide::Hero;
    int heroSlot = 1;
    if (_previewContext.subjectSide == PreviewSlotSide::Hero)
        heroSlot = _previewContext.subjectSlot;
    else if (_previewContext.opponentSide == PreviewSlotSide::Hero)
        heroSlot = _previewContext.opponentSlot;

    PreviewSlotSide enemySide = PreviewSlotSide::Enemy;
    int enemySlot = 1;
    if (_previewContext.subjectSide == PreviewSlotSide::Enemy)
        enemySlot = _previewContext.subjectSlot;
    else if (_previewContext.opponentSide == PreviewSlotSide::Enemy)
        enemySlot = _previewContext.opponentSlot;

    if (_previewHeroObject != nullptr)
    {
        if (Shared<TransformCom> transform = _previewHeroObject->Get_Transform())
        {
            transform->Set_WorldPosition(Get_PreviewSlotWorldPosition(heroSide, heroSlot));
            const PreviewSlot* slot = Get_PreviewSlot(heroSide, heroSlot);
            if (slot != nullptr)
                transform->Set_WorldRotationEuler(slot->rotation);
        }
    }

    if (_previewEnemyObject != nullptr)
    {
        if (Shared<TransformCom> transform = _previewEnemyObject->Get_Transform())
        {
            transform->Set_WorldPosition(Get_PreviewSlotWorldPosition(enemySide, enemySlot));
            const PreviewSlot* slot = Get_PreviewSlot(enemySide, enemySlot);
            if (slot != nullptr)
                transform->Set_WorldRotationEuler(slot->rotation);
        }
    }
}

void BattleCameraPreview_View::Render_Header()
{
    ImGui::TextUnformatted("Battle Camera Preview");
    ImGui::SameLine();
    ImGui::TextDisabled("independent formation preview");

    ImGui::SameLine(ImGui::GetWindowWidth() - 220.f);
    if (ImGui::Button("Apply Preview"))
    {
        Apply_PreviewContext();
        _previewDirty = false;
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop Preview"))
        Clear_PreviewContext();
}

void BattleCameraPreview_View::Render_ContextPanel()
{
    ImGui::BeginChild("BattleCameraContextPanel", ImVec2(0.f, 0.f), true);

    ImGui::TextDisabled("Preview Context");
    ImGui::Separator();

    if (Select_EnumCombo("Mode", _previewContext.mode))
        _previewDirty = true;

    if (ImGui::Checkbox("Use Skill Override", &_useSkillOverride))
        _previewDirty = true;

    if (_useSkillOverride)
    {
        const char* currentSkillLabel = "None";
        if (_selectedSkillIndex >= 0 && _selectedSkillIndex < static_cast<int>(_skillIds.size()))
            currentSkillLabel = _skillIds[_selectedSkillIndex].c_str();

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("Skill", currentSkillLabel))
        {
            for (int i = 0; i < static_cast<int>(_skillIds.size()); ++i)
            {
                const bool isSelected = (_selectedSkillIndex == i);
                if (ImGui::Selectable(_skillIds[i].c_str(), isSelected))
                {
                    _selectedSkillIndex = i;
                    _previewDirty = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
    }

    Render_PreviewActorSelector();

    Render_SlotSelector("Subject", _previewContext.subjectSide, _previewContext.subjectSlot);
    Render_SlotSelector("Opponent", _previewContext.opponentSide, _previewContext.opponentSlot);

    if (ImGui::DragFloat("Time", &_previewContext.timeSec, 0.01f, 0.f, 30.f, "%.3f s"))
    {
        _previewContext.timeSec = std::max(0.f, _previewContext.timeSec);
        _previewDirty = true;
    }

    ImGui::Separator();

    ImGui::Checkbox("Auto Preview", &_autoPreview);

    Render_FormationEditor();
    ImGui::EndChild();
}

void BattleCameraPreview_View::Render_PreviewActorSelector()
{
    ImGui::Separator();
    ImGui::TextDisabled("Preview Actors");

    if (Select_EnumCombo("Hero", _selectedHeroId, -FLT_MIN))
        _previewObjectsDirty = true;

    const char* currentEnemyLabel = Get_SelectedEnemyPrefabName().empty() ? "None" : Get_SelectedEnemyPrefabName().c_str();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("Enemy", currentEnemyLabel))
    {
        for (int i = 0; i < static_cast<int>(_enemyPrefabNames.size()); ++i)
        {
            const bool isSelected = _selectedEnemyPrefabIndex == i;
            if (ImGui::Selectable(_enemyPrefabNames[i].c_str(), isSelected))
            {
                _selectedEnemyPrefabIndex = i;
                _previewObjectsDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

void BattleCameraPreview_View::Render_SlotSelector(const char* label, PreviewSlotSide& side, int& slotIndex)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);

    ImGui::SetNextItemWidth(120.f);
    if (ImGui::BeginCombo("Side", Get_SlotSideLabel(side)))
    {
        const PreviewSlotSide sides[] = { PreviewSlotSide::Hero, PreviewSlotSide::Enemy };
        for (PreviewSlotSide value : sides)
        {
            const bool isSelected = side == value;
            if (ImGui::Selectable(Get_SlotSideLabel(value), isSelected))
            {
                side = value;
                _previewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    const PreviewSlot* currentSlot = Get_PreviewSlot(side, slotIndex);
    const char* currentLabel = currentSlot ? currentSlot->label.c_str() : "None";
    if (ImGui::BeginCombo("Slot", currentLabel))
    {
        for (int i = 0; i < 3; ++i)
        {
            const PreviewSlot* slot = Get_PreviewSlot(side, i);
            if (slot == nullptr)
                continue;

            const bool isSelected = slotIndex == i;
            if (ImGui::Selectable(slot->label.c_str(), isSelected))
            {
                slotIndex = i;
                _previewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopID();
}

void BattleCameraPreview_View::Render_FormationEditor()
{
    ImGui::Separator();
    if (!ImGui::CollapsingHeader("Preview Formation", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    float origin[3] = { _previewFormation.origin.x, _previewFormation.origin.y, _previewFormation.origin.z };
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::DragFloat3("Origin", origin, 0.05f))
    {
        _previewFormation.origin = Vec3(origin[0], origin[1], origin[2]);
        _previewDirty = true;
    }

    if (ImGui::Button("Reset Runtime Default Layout"))
    {
        Reset_DefaultFormation();
        _previewDirty = true;
    }

    auto drawSlots = [this](const char* label, array<PreviewSlot, 3>& slots)
    {
        if (!ImGui::TreeNode(label))
            return;

        for (int i = 0; i < static_cast<int>(slots.size()); ++i)
        {
            PreviewSlot& slot = slots[i];
            ImGui::PushID(i);
            ImGui::TextUnformatted(slot.label.c_str());

            float offset[3] = { slot.offset.x, slot.offset.y, slot.offset.z };
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat3("Offset", offset, 0.05f))
            {
                slot.offset = Vec3(offset[0], offset[1], offset[2]);
                _previewDirty = true;
            }

            float rotation[3] = { slot.rotation.x, slot.rotation.y, slot.rotation.z };
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat3("Rotation", rotation, 0.25f))
            {
                slot.rotation = Vec3(rotation[0], rotation[1], rotation[2]);
                _previewDirty = true;
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    };

    drawSlots("Hero Slots", _previewFormation.heroes);
    drawSlots("Enemy Slots", _previewFormation.enemies);
}

void BattleCameraPreview_View::Render_PreviewCanvas()
{
    ImGui::BeginChild("BattleCameraCenterPreviewPanel", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::TextDisabled("independent render target");
    ImGui::Separator();

    ImVec2 previewSize = ImGui::GetContentRegionAvail();
    if (previewSize.x < 1.f)
        previewSize.x = 1.f;
    if (previewSize.y < 1.f)
        previewSize.y = 1.f;

    _previewWidth = std::max(1u, static_cast<uint32>(previewSize.x));
    _previewHeight = std::max(1u, static_cast<uint32>(previewSize.y));

    const ImVec2 previewPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(
        "BattleCameraPreviewCanvas",
        previewSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    _isPreviewHovered = ImGui::IsItemHovered();
    _isPreviewActive = ImGui::IsItemActive();

    const ImVec2 previewMax = ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(previewPos, previewMax, IM_COL32(18, 20, 24, 255));
    drawList->AddRect(previewPos, previewMax, IM_COL32(55, 60, 70, 255));

    if (_previewRenderTarget != nullptr && _previewRenderTarget->Get_SRV() != nullptr)
    {
        const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(_previewRenderTarget->Get_SRV()));
        drawList->AddImage(textureId, previewPos, previewMax);

        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewPos.y + 12.f),
            IM_COL32(190, 198, 210, 255),
            "Independent preview target");

    }
    else
    {
        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewPos.y + 12.f),
            IM_COL32(210, 216, 224, 255),
            "Battle camera preview target is not ready.");
    }

    ImGui::EndChild();
}

void BattleCameraPreview_View::Render_PreviewPanel()
{
    ImGui::BeginChild("BattleCameraDetailsPanel", ImVec2(0.f, 0.f), true);

    ImGui::TextDisabled("Preview Context");
    ImGui::Separator();

    ImGui::Text("Mode : %s", magic_enum::enum_name(_previewContext.mode).data());
    ImGui::Text("Skill : %s", Get_SelectedSkillId().empty() ? "(default)" : Get_SelectedSkillId().c_str());
    ImGui::Text("Hero Prefab : %s", Get_SelectedHeroPrefabName().empty() ? "(none)" : Get_SelectedHeroPrefabName().c_str());
    ImGui::Text("Enemy Prefab : %s", Get_SelectedEnemyPrefabName().empty() ? "(none)" : Get_SelectedEnemyPrefabName().c_str());

    const PreviewSlot* subjectSlot = Get_PreviewSlot(_previewContext.subjectSide, _previewContext.subjectSlot);
    const PreviewSlot* opponentSlot = Get_PreviewSlot(_previewContext.opponentSide, _previewContext.opponentSlot);
    const Vec3 subjectPos = Get_PreviewSlotWorldPosition(_previewContext.subjectSide, _previewContext.subjectSlot);
    const Vec3 opponentPos = Get_PreviewSlotWorldPosition(_previewContext.opponentSide, _previewContext.opponentSlot);
    const float distance = (opponentPos - subjectPos).Length();

    ImGui::Text("Subject : %s %s", Get_SlotSideLabel(_previewContext.subjectSide), subjectSlot ? subjectSlot->label.c_str() : "(none)");
    ImGui::Text("Opponent : %s %s", Get_SlotSideLabel(_previewContext.opponentSide), opponentSlot ? opponentSlot->label.c_str() : "(none)");
    ImGui::Text("Time : %.3f s", _previewContext.timeSec);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Formation");
    ImGui::Text("Origin : %.2f, %.2f, %.2f", _previewFormation.origin.x, _previewFormation.origin.y, _previewFormation.origin.z);
    ImGui::Text("Subject Pos : %.2f, %.2f, %.2f", subjectPos.x, subjectPos.y, subjectPos.z);
    ImGui::Text("Opponent Pos : %.2f, %.2f, %.2f", opponentPos.x, opponentPos.y, opponentPos.z);
    ImGui::Text("Distance : %.2f", distance);

    ImGui::Spacing();
    ImGui::TextWrapped("This view now owns a deterministic preview formation instead of reading live battle actors.");

    ImGui::EndChild();
}

void BattleCameraPreview_View::Render_TimelinePanel()
{
    ImGui::BeginChild("BattleCameraTimelinePanel", ImVec2(0.f, 0.f), true);

    ImGui::TextDisabled("Battle Clip Timeline");
    ImGui::Separator();

    const string selectedSkillId = Get_SelectedSkillId();
    if (!_hasLoadedBattleClip ||
        _loadedBattleClipMode != _previewContext.mode ||
        _loadedBattleClipSkillId != selectedSkillId)
    {
        Client::BattleCameraClipIO::LoadBattleCameraClipFromFile(_previewContext.mode, _editingBattleClip, selectedSkillId);
        _loadedBattleClipMode = _previewContext.mode;
        _loadedBattleClipSkillId = selectedSkillId;
        _hasLoadedBattleClip = true;
        _battleClipCurrentFrame = 0;
        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
    }

    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("Duration", &_editingBattleClip.duration, 0.01f, 0.f, 30.f))
    {
        _editingBattleClip.duration = std::max(0.f, _editingBattleClip.duration);
        _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipFrameMax(_editingBattleClip));
        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Clip"))
    {
        Client::BattleCameraClipIO::LoadBattleCameraClipFromFile(_previewContext.mode, _editingBattleClip, selectedSkillId);
        _battleClipCurrentFrame = 0;
        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Clip"))
        Client::BattleCameraClipIO::SaveBattleCameraClipToFile(_previewContext.mode, _editingBattleClip, selectedSkillId);

    ImGui::SameLine();
    if (ImGui::Button(_timelinePlaying ? "Pause" : "Play"))
        _timelinePlaying = !_timelinePlaying;

    ImGui::SameLine();
    if (ImGui::Button("Go To Start"))
    {
        _timelinePlaying = false;
        _battleClipCurrentFrame = 0;
        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Frame %d | Time %.3f s", _battleClipCurrentFrame, _previewContext.timeSec);

    ImGui::Separator();
    ImGui::TextDisabled("Animation");

    const char* currentAnimationLabel = "(none)";
    if (_selectedPreviewAnimationIndex >= 0 &&
        _selectedPreviewAnimationIndex < static_cast<int>(_previewAnimationNames.size()))
    {
        currentAnimationLabel = _previewAnimationNames[_selectedPreviewAnimationIndex].c_str();
    }

    ImGui::SetNextItemWidth(320.f);
    if (ImGui::BeginCombo("Subject Animation", currentAnimationLabel))
    {
        for (int i = 0; i < static_cast<int>(_previewAnimationNames.size()); ++i)
        {
            const bool isSelected = _selectedPreviewAnimationIndex == i;
            if (ImGui::Selectable(_previewAnimationNames[i].c_str(), isSelected))
            {
                _selectedPreviewAnimationIndex = i;
                _previewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::Spacing();
    Render_BattleClipSequencer();
    Render_SelectedBattleClipKeyframe();
    Render_BattleClipInsertKeyframePopup();
    Render_BattleClipRemoveKeyframePopup();

    ImGui::EndChild();
}

void BattleCameraPreview_View::Render_BattleClipSequencer()
{
    bool consumedKeyClick = false;
    bool requestOpenInsertPopup = false;
    bool requestRemoveKeyPopup = false;
    int sequencerSelectedTrackIndex = _battleClipSelectedTrackIndex;
    const int previousFrame = _battleClipCurrentFrame;
    const int previousTrackIndex = _battleClipSelectedTrackIndex;
    const int previousKeyIndex = _battleClipSelectedKeyIndex;

    BattleClipSequencerAdapter adapter(
        &_editingBattleClip,
        &_battleClipSelectedTrackIndex,
        &_battleClipSelectedKeyIndex,
        &_battleClipCurrentFrame,
        &consumedKeyClick,
        &requestOpenInsertPopup,
        &requestRemoveKeyPopup,
        &_battleClipContextTrackIndex,
        &_battleClipContextFrame);

    static bool expanded = true;
    static int firstFrame = 0;
    ImSequencer::Sequencer(&adapter, &_battleClipCurrentFrame, &expanded, &sequencerSelectedTrackIndex, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);

    if (!consumedKeyClick)
        _battleClipSelectedTrackIndex = sequencerSelectedTrackIndex;

    if (requestOpenInsertPopup)
        _requestOpenBattleClipInsertPopup = true;

    if (requestRemoveKeyPopup)
        _requestRemoveBattleClipKeyPopup = true;

    if (previousFrame != _battleClipCurrentFrame ||
        previousTrackIndex != _battleClipSelectedTrackIndex ||
        previousKeyIndex != _battleClipSelectedKeyIndex)
    {
        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
    }
}

void BattleCameraPreview_View::Render_SelectedBattleClipKeyframe()
{
    BattleCameraKeyframe* keyframe = Get_SelectedBattleClipKeyframe();
    if (keyframe == nullptr)
        return;

    ImGui::Separator();
    ImGui::TextDisabled("Selected Key | Track %d | Key %d", _battleClipSelectedTrackIndex, _battleClipSelectedKeyIndex);

    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("Key Time", &keyframe->time, 0.01f, 0.f, _editingBattleClip.duration))
    {
        keyframe->time = std::clamp(keyframe->time, 0.f, _editingBattleClip.duration);
        if (BattleCameraTrack* track = Get_SelectedBattleClipTrack())
        {
            const float selectionTime = keyframe->time;
            Sort_BattleClipTrackKeyframes(*track);
            for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
            {
                if (fabsf(track->keyframes[i].time - selectionTime) <= 0.0001f)
                {
                    _battleClipSelectedKeyIndex = i;
                    break;
                }
            }
        }

        _battleClipCurrentFrame = static_cast<int>(std::lround(keyframe->time * kBattleClipPreviewFps));
        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("Key Value", &keyframe->value, 0.01f))
        _previewDirty = true;
}

void BattleCameraPreview_View::Render_BattleClipInsertKeyframePopup()
{
    if (_requestOpenBattleClipInsertPopup)
    {
        ImGui::OpenPopup("Insert Battle Preview Clip Keyframe");
        _requestOpenBattleClipInsertPopup = false;
    }

    if (!ImGui::BeginPopup("Insert Battle Preview Clip Keyframe"))
        return;

    ImGui::TextDisabled("Track %d", _battleClipContextTrackIndex);
    ImGui::TextDisabled("Frame %d", _battleClipContextFrame);
    ImGui::Separator();

    if (ImGui::Button("Insert Keyframe"))
    {
        Insert_BattleClipKeyAtFrame(_battleClipContextTrackIndex, _battleClipContextFrame);
        Sync_PreviewTimeFromTimeline();
        _previewDirty = true;
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void BattleCameraPreview_View::Render_BattleClipRemoveKeyframePopup()
{
    if (_requestRemoveBattleClipKeyPopup)
    {
        ImGui::OpenPopup("Remove Battle Preview Clip Keyframe");
        _requestRemoveBattleClipKeyPopup = false;
    }

    if (!ImGui::BeginPopup("Remove Battle Preview Clip Keyframe"))
        return;

    if (ImGui::Button("Remove Selected Key"))
    {
        BattleCameraTrack* track = Get_SelectedBattleClipTrack();
        if (track != nullptr &&
            _battleClipSelectedKeyIndex >= 0 &&
            _battleClipSelectedKeyIndex < static_cast<int>(track->keyframes.size()))
        {
            track->keyframes.erase(track->keyframes.begin() + _battleClipSelectedKeyIndex);
            _battleClipSelectedKeyIndex = std::clamp(_battleClipSelectedKeyIndex, -1, static_cast<int>(track->keyframes.size()) - 1);
            _previewDirty = true;
        }

        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void BattleCameraPreview_View::Apply_PreviewContext()
{
    Apply_PreviewAnimation(_previewContext.timeSec);
    Apply_BattleCameraPreviewPose(_previewContext.timeSec);
    _previewDirty = false;
}

void BattleCameraPreview_View::Apply_PreviewAnimation(float previewTimeSec) const
{
    Shared<ModelCom> model = Resolve_PreviewSubjectModel();
    if (model == nullptr)
        return;

    if (_selectedPreviewAnimationIndex >= 0 &&
        _selectedPreviewAnimationIndex < static_cast<int>(_previewAnimationNames.size()))
    {
        const string& animationName = _previewAnimationNames[_selectedPreviewAnimationIndex];
        const char* currentAnimationName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
        if (currentAnimationName == nullptr || animationName != currentAnimationName)
            model->Set_Animation(animationName, false, 0.f);
    }

    model->Set_AnimationPaused(true);
    model->Set_CurrentAnimationTimeSec(previewTimeSec);
    model->Refresh_CurrentAnimationPose();
}

void BattleCameraPreview_View::Apply_BattleCameraPreviewPose(float previewTimeSec)
{
    if (_previewCamera == nullptr)
        return;

    Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera();
    if (battleTurnCamera == nullptr)
        return;

    const BattleCameraPreset& preset = battleTurnCamera->Get_Preset(_previewContext.mode);
    const BattleCameraClipSample clipSample = Sample_BattleCameraClip(_editingBattleClip, previewTimeSec);
    const float finalFovY = std::clamp(preset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = preset.distance + clipSample.distanceOffset;

    const Vec3 subjectPos = Get_PreviewSlotWorldPosition(_previewContext.subjectSide, _previewContext.subjectSlot);
    const Vec3 opponentPos = Get_PreviewSlotWorldPosition(_previewContext.opponentSide, _previewContext.opponentSlot);

    Vec3 desiredPos = Vec3::Zero;
    Vec3 lookTarget = Vec3::Zero;

    if (_previewContext.mode == BattleCameraMode::ENEMYATTACK && _previewContext.opponentSide != _previewContext.subjectSide)
    {
        Vec3 forward = opponentPos - subjectPos;
        forward.y = 0.f;
        if (forward.LengthSquared() <= 0.0001f)
            forward = Vec3::Forward;
        forward.Normalize();

        Vec3 right;
        right.x = forward.z;
        right.y = 0.f;
        right.z = -forward.x;
        right.Normalize();

        const Vec3 backDir = -forward;
        const float yawRad = XMConvertToRadians(preset.yawOffset);
        const Vec3 yawAdjustedBack = backDir * cosf(yawRad) + right * sinf(yawRad);

        desiredPos = opponentPos + yawAdjustedBack * finalDistance + right * preset.sideOffset + Vec3::Up * preset.heightOffset;
        lookTarget = subjectPos + forward * preset.aimOffset + Vec3::Up * preset.lookHeight;
    }
    else
    {
        const float yawRad = XMConvertToRadians(preset.yawOffset);
        const float pitchRad = XMConvertToRadians(preset.pitchOffset);

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

        const Vec3 baseOffset = backDir * finalDistance;
        const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
        const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);
        const Vec3 pivotPos = subjectPos + Vec3::Up * preset.pivotHeight;
        const Vec3 framingPos = pivotPos + rightDir * preset.aimOffset;

        desiredPos = framingPos + orbitOffset + Vec3::Up * preset.heightOffset + rightDir * preset.sideOffset;
        lookTarget = framingPos + Vec3::Up * preset.lookHeight;
    }

    _previewCamera->Set_FovY(finalFovY);
    if (Shared<TransformCom> transform = _previewCamera->Get_Transform())
    {
        transform->Set_WorldPosition(desiredPos);
        transform->LookAt(lookTarget);
        transform->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, preset.rollOffset));
    }
}

void BattleCameraPreview_View::Clear_PreviewContext()
{
    Clear_PreviewObjects();
    _previewObjectsDirty = true;
    _timelinePlaying = false;
}

void BattleCameraPreview_View::Refresh_PreviewAnimationList()
{
    if (!_animationListDirty)
        return;

    _previewAnimationNames.clear();
    Shared<ModelCom> model = Resolve_PreviewSubjectModel();
    if (model == nullptr)
        return;

    const uint32 animationCount = static_cast<uint32>(model->Get_NumAnimations());
    _previewAnimationNames.reserve(animationCount);

    for (uint32 i = 0; i < animationCount; ++i)
    {
        if (const char* animationName = model->Get_AnimationName(i))
            _previewAnimationNames.push_back(animationName);
    }

    if (!_previewAnimationNames.empty())
        _selectedPreviewAnimationIndex = 0;

    _animationListDirty = false;
}

void BattleCameraPreview_View::Sort_BattleClipTrackKeyframes(BattleCameraTrack& track)
{
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const BattleCameraKeyframe& lhs, const BattleCameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });
}

void BattleCameraPreview_View::Insert_BattleClipKeyAtFrame(int trackIndex, int frame)
{
    BattleCameraTrack* track = nullptr;

    switch (trackIndex)
    {
    case 0:
        track = &_editingBattleClip.distanceOffset;
        break;
    case 1:
        track = &_editingBattleClip.fovOffset;
        break;
    case 2:
        track = &_editingBattleClip.shakeAmplitude;
        break;
    default:
        return;
    }

    const float keyTime = std::clamp(static_cast<float>(frame) / kBattleClipPreviewFps, 0.f, _editingBattleClip.duration);

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (fabsf(track->keyframes[i].time - keyTime) <= 0.0001f)
        {
            _battleClipSelectedTrackIndex = trackIndex;
            _battleClipSelectedKeyIndex = i;
            _battleClipCurrentFrame = frame;
            return;
        }
    }

    BattleCameraKeyframe keyframe{};
    keyframe.time = keyTime;
    track->keyframes.push_back(keyframe);
    Sort_BattleClipTrackKeyframes(*track);

    _battleClipSelectedTrackIndex = trackIndex;
    _battleClipCurrentFrame = frame;

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (fabsf(track->keyframes[i].time - keyTime) <= 0.0001f)
        {
            _battleClipSelectedKeyIndex = i;
            break;
        }
    }
}

void BattleCameraPreview_View::Sync_PreviewTimeFromTimeline()
{
    _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipFrameMax(_editingBattleClip));
    _previewContext.timeSec = static_cast<float>(_battleClipCurrentFrame) / kBattleClipPreviewFps;
}

Shared<Client::BattleTurnCamera> BattleCameraPreview_View::Get_BattleTurnCamera() const
{
    auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr)
        return nullptr;

    return battleController->Get_BattleTurnCamera();
}

Shared<ModelCom> BattleCameraPreview_View::Resolve_PreviewSubjectModel() const
{
    Shared<GameObject> subjectObject =
        (_previewContext.subjectSide == PreviewSlotSide::Hero) ? _previewHeroObject : _previewEnemyObject;

    if (subjectObject == nullptr)
        return nullptr;

    if (Shared<ModelCom> directModel = subjectObject->Get_Component<ModelCom>())
        return directModel;

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(subjectObject);
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

string BattleCameraPreview_View::Get_SelectedSkillId() const
{
    if (!_useSkillOverride)
        return "";

    if (_selectedSkillIndex < 0 || _selectedSkillIndex >= static_cast<int>(_skillIds.size()))
        return "";

    return _skillIds[_selectedSkillIndex];
}

const char* BattleCameraPreview_View::Get_SlotSideLabel(PreviewSlotSide side) const
{
    switch (side)
    {
    case PreviewSlotSide::Hero:
        return "Hero";
    case PreviewSlotSide::Enemy:
        return "Enemy";
    default:
        return "Unknown";
    }
}

const BattleCameraPreview_View::PreviewSlot* BattleCameraPreview_View::Get_PreviewSlot(PreviewSlotSide side, int slotIndex) const
{
    slotIndex = std::clamp(slotIndex, 0, 2);

    switch (side)
    {
    case PreviewSlotSide::Hero:
        return &_previewFormation.heroes[slotIndex];
    case PreviewSlotSide::Enemy:
        return &_previewFormation.enemies[slotIndex];
    default:
        return nullptr;
    }
}

Vec3 BattleCameraPreview_View::Get_PreviewSlotWorldPosition(PreviewSlotSide side, int slotIndex) const
{
    const PreviewSlot* slot = Get_PreviewSlot(side, slotIndex);
    if (slot == nullptr)
        return _previewFormation.origin;

    return _previewFormation.origin + slot->offset;
}

string BattleCameraPreview_View::Get_SelectedHeroPrefabName() const
{
    const Client::HeroSpawnDesc* spawnDesc = CLIENT->Find_HeroSpawnDesc(_selectedHeroId);
    if (spawnDesc == nullptr)
        return "";

    return spawnDesc->battlePrefabName;
}

string BattleCameraPreview_View::Get_SelectedEnemyPrefabName() const
{
    if (_selectedEnemyPrefabIndex < 0 || _selectedEnemyPrefabIndex >= static_cast<int>(_enemyPrefabNames.size()))
        return "";

    return _enemyPrefabNames[_selectedEnemyPrefabIndex];
}

BattleCameraTrack* BattleCameraPreview_View::Get_SelectedBattleClipTrack()
{
    switch (_battleClipSelectedTrackIndex)
    {
    case 0:
        return &_editingBattleClip.distanceOffset;
    case 1:
        return &_editingBattleClip.fovOffset;
    case 2:
        return &_editingBattleClip.shakeAmplitude;
    default:
        return nullptr;
    }
}

BattleCameraKeyframe* BattleCameraPreview_View::Get_SelectedBattleClipKeyframe()
{
    BattleCameraTrack* track = Get_SelectedBattleClipTrack();
    if (track == nullptr)
        return nullptr;

    if (_battleClipSelectedKeyIndex < 0 || _battleClipSelectedKeyIndex >= static_cast<int>(track->keyframes.size()))
        return nullptr;

    return &track->keyframes[_battleClipSelectedKeyIndex];
}

Shared<BattleCameraPreview_View> BattleCameraPreview_View::Create()
{
    return make_shared<BattleCameraPreview_View>();
}
