#include "BattleCameraPreview_View.h"
#include "BattleController.h"
#include "BattleCameraClipIO.h"
#include "BattleCameraClipUtils.h"
#include "CameraPresetIO.h"
#include "BattleClipSequencerAdapter.h"
#include "BattleTurnCamera.h"
#include "BattleTurnCameraPoseUtils.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "Editor_Camera_Free.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "Monster_WorldBase.h"
#include "PartObject.h"
#include "RenderTarget.h"
#include "SkyBoxFlow.h"
#include "SkyCloudCard.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include "magic_enum/magic_enum.hpp"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <cmath>
#include <optional>
#include <shobjidl.h>
#include <string_view>
#include "EditorInstance.h"

namespace
{
constexpr float kBattleClipPreviewFps = 30.f;
constexpr float kBattleAnimationPreviewFps = 60.f;
constexpr float kLeftPanelComboWidth = 170.f;
constexpr float kLeftPanelWideComboWidth = 210.f;
constexpr float kSlotSideComboWidth = 86.f;
constexpr float kSlotComboWidth = 112.f;
constexpr float kFormationVec3InputWidth = 190.f;
constexpr float kPresetInputWidth = 150.f;
constexpr wchar_t kBattleCameraPresetFolder[] = L"../../../Client/Bin/Resources/Data/json/CameraPreset";
constexpr int kBattleClipDebugFallbackTrackIndex = 9;
constexpr int kBattleClipTimelineTrackCount = 19;
const Vec3 kPreviewSkyCloudLocalOffset = Vec3(28.f, 34.f, 82.f);
const std::array<Vec3, 3> kDefaultHeroSlotOffsets =
{
    Vec3(-2.5f, 0.f, 0.f),
    Vec3(0.f, 0.1f, 0.f),
    Vec3(2.5f, 0.1f, 0.f),
};
const std::array<Vec3, 3> kDefaultEnemySlotOffsets =
{
    Vec3(-5.85f, 0.f, 6.f),
    Vec3(0.f, 0.f, 6.f),
    Vec3(5.85f, 0.f, 6.f),
};
const Vec3 kDefaultEnemySlotRotation = Vec3(0.f, 180.f, 0.f);

int Resolve_EnemyPreviewSlotIndex(size_t spawnCount, size_t sortedIndex)
{
    if (spawnCount <= 1)
        return 1;

    if (spawnCount == 2)
        return sortedIndex == 0 ? 0 : 2;

    return static_cast<int>(std::min<size_t>(sortedIndex, 2));
}

string Resolve_WorldMonsterPrefabName(const string& battlePrefabName)
{
    if (battlePrefabName.empty())
        return {};

    if (battlePrefabName.starts_with("MB_"))
        return "MW_" + battlePrefabName.substr(3);

    return battlePrefabName;
}

enum class BattleAuthoringFileKind : uint8
{
    Preset,
    Clip
};

struct ParsedBattleAuthoringFile
{
    BattleAuthoringFileKind kind = BattleAuthoringFileKind::Preset;
    BattleCameraMode mode = BattleCameraMode::PLAYERATTACK;
    string skillId{};
};

int Get_BattleClipFrameMax(const BattleCameraClip& clip)
{
    return std::max(0, static_cast<int>(std::lround(clip.duration * kBattleClipPreviewFps)));
}

BattleCameraClipTimeMode Get_DefaultBattleCameraClipTimeMode(BattleCameraMode cameraMode)
{
    return cameraMode == BattleCameraMode::BATTLEINTRO ||
        cameraMode == BattleCameraMode::SELECTSKILL
        ? BattleCameraClipTimeMode::Independent
        : BattleCameraClipTimeMode::AnimationSync;
}

const char* Get_BattleClipTimeModeLabel(BattleCameraClipTimeMode timeMode)
{
    switch (timeMode)
    {
    case BattleCameraClipTimeMode::AnimationSync:
        return "Animation Sync";
    case BattleCameraClipTimeMode::Independent:
        return "Independent";
    default:
        return "Unknown";
    }
}

const CameraTrack* Get_BattleClipTrackByIndex(const BattleCameraClip& clip, int trackIndex)
{
    switch (trackIndex)
    {
    case 0: return &clip.distanceOffset;
    case 1: return &clip.fovOffset;
    case 2: return &clip.shakeAmplitude;
    case 3: return &clip.shakeFrequencyX;
    case 4: return &clip.shakeFrequencyY;
    case 5: return &clip.shakeDecayRateX;
    case 6: return &clip.shakeDecayRate;
    case 7: return &clip.shakeVerticalRatio;
    case 8: return &clip.shakeStrengthScale;
    case 9: return &clip.sideOffset;
    case 10: return &clip.pivotOffset;
    case 11: return &clip.heightOffset;
    case 12: return &clip.aimOffset;
    case 13: return &clip.lookHeightOffset;
    case 14: return &clip.yawOffset;
    case 15: return &clip.orbitYaw;
    case 16: return &clip.orbitRadius;
    case 17: return &clip.orbitHeight;
    case 18: return &clip.orbitLookHeight;
    default:
        return nullptr;
    }
}

CameraTrack* Get_BattleClipTrackByIndex(BattleCameraClip& clip, int trackIndex)
{
    switch (trackIndex)
    {
    case 0: return &clip.distanceOffset;
    case 1: return &clip.fovOffset;
    case 2: return &clip.shakeAmplitude;
    case 3: return &clip.shakeFrequencyX;
    case 4: return &clip.shakeFrequencyY;
    case 5: return &clip.shakeDecayRateX;
    case 6: return &clip.shakeDecayRate;
    case 7: return &clip.shakeVerticalRatio;
    case 8: return &clip.shakeStrengthScale;
    case 9: return &clip.sideOffset;
    case 10: return &clip.pivotOffset;
    case 11: return &clip.heightOffset;
    case 12: return &clip.aimOffset;
    case 13: return &clip.lookHeightOffset;
    case 14: return &clip.yawOffset;
    case 15: return &clip.orbitYaw;
    case 16: return &clip.orbitRadius;
    case 17: return &clip.orbitHeight;
    case 18: return &clip.orbitLookHeight;
    default:
        return nullptr;
    }
}

const char* Get_BattleClipTrackLabel(int trackIndex)
{
    switch (trackIndex)
    {
    case 0: return "Distance Offset";
    case 1: return "FOV Offset";
    case 2: return "Shake Amplitude";
    case 3: return "Shake Frequency X";
    case 4: return "Shake Frequency Y";
    case 5: return "Shake Decay Rate X";
    case 6: return "Shake Decay Rate Y";
    case 7: return "Shake Vertical Ratio";
    case 8: return "Shake Strength Scale";
    case 9: return "Side Offset";
    case 10: return "Pivot Offset";
    case 11: return "Height Offset";
    case 12: return "Aim Offset";
    case 13: return "Look Height Offset";
    case 14: return "Yaw Offset";
    case 15: return "Orbit Yaw";
    case 16: return "Orbit Radius";
    case 17: return "Orbit Height";
    case 18: return "Orbit Look Height";
    default:
        return "Unknown";
    }
}

bool Is_BattleClipTrackSorted(const CameraTrack& track)
{
    for (size_t i = 1; i < track.keyframes.size(); ++i)
    {
        if (track.keyframes[i - 1].time > track.keyframes[i].time)
            return false;
    }

    return true;
}

bool Uses_RuntimeBattleClip(BattleCameraMode mode)
{
    return true;
}

BattleCameraPreset Make_FallbackBattleCameraPreset()
{
    BattleCameraPreset preset{};
    preset.camera.fovy = XMConvertToRadians(45.f);
    preset.camera.nearPlane = 0.1f;
    preset.camera.farPlane = 500.f;
    preset.camera.mouseSensor = 0.29f;
    preset.camera.mouseSensitivity = 0.23f;
    preset.followSpeed = 10.f;
    preset.distance = 8.f;
    preset.sideOffset = 0.f;
    preset.heightOffset = 1.4f;
    preset.aimOffset = 0.f;
    preset.pitchMin = -45.f;
    preset.pitchMax = 45.f;
    preset.yawOffset = 180.f;
    preset.pitchOffset = 8.f;
    preset.lookHeight = 1.1f;
    preset.pivotHeight = 1.f;
    return preset;
}

void Apply_PreviewProceduralDrift(
    const BattleCameraDriftProfile& drift,
    float timeSec,
    Client::BattleTurnCameraEvaluatedPose& pose)
{
    if (!drift.enabled)
        return;

    Vec3 forward = pose.lookTarget - pose.cameraPos;
    if (forward.LengthSquared() <= 0.0001f)
        return;

    forward.Normalize();

    Vec3 right = Vec3::Up.Cross(forward);
    if (right.LengthSquared() <= 0.0001f)
        return;

    right.Normalize();

    Vec3 up = forward.Cross(right);
    up.Normalize();

    const float x = sinf(timeSec * drift.lateralFrequency) * drift.lateralAmplitude;
    const float y = sinf(timeSec * drift.verticalFrequency + 1.37f) * drift.verticalAmplitude;
    const float z = sinf(timeSec * drift.depthFrequency + 2.11f) * drift.depthAmplitude;
    const float roll = sinf(timeSec * drift.rollFrequency + 0.73f) * drift.rollAmplitude;

    const Vec3 driftOffset = right * x + up * y + forward * z;
    pose.cameraPos += driftOffset;
    pose.lookTarget += driftOffset * 0.6f;
    pose.rollOffset += roll;
}

template <typename TEnum>
bool Select_EnumCombo(const char* label, TEnum& currentValue, float width = 160.f)
{
    bool changed = false;
    const string currentLabel = string(magic_enum::enum_name(currentValue));

    ImGui::SetNextItemWidth(width);
    if (!ImGui::BeginCombo(label, currentLabel.c_str()))
        return false;

    for (TEnum value : magic_enum::enum_values<TEnum>())
    {
        const string itemLabel = string(magic_enum::enum_name(value));
        const bool isSelected = (currentValue == value);
        if (ImGui::Selectable(itemLabel.c_str(), isSelected))
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

bool StartsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool Contains_CaseInsensitive(std::string_view text, std::string_view query)
{
    if (query.empty())
        return true;

    auto toLower = [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); };

    std::string loweredText(text.begin(), text.end());
    std::string loweredQuery(query.begin(), query.end());
    std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), toLower);
    std::transform(loweredQuery.begin(), loweredQuery.end(), loweredQuery.begin(), toLower);

    return loweredText.find(loweredQuery) != std::string::npos;
}

bool Try_PickBattleCameraJsonFileDialog(const wchar_t* title, const wstring& defaultFolder, wstring& outFilePath)
{
    outFilePath.clear();

    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninit = SUCCEEDED(initHr);

    IFileOpenDialog* dialog = nullptr;
    const HRESULT createHr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(createHr) || dialog == nullptr)
    {
        if (shouldUninit)
            CoUninitialize();
        return false;
    }

    dialog->SetTitle(title);

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

    try
    {
        fs::path folderPath(defaultFolder);
        if (!folderPath.empty())
        {
            if (folderPath.is_relative())
                folderPath = fs::absolute(folderPath);

            folderPath = folderPath.lexically_normal();
            if (fs::exists(folderPath) && fs::is_directory(folderPath))
            {
                IShellItem* folderItem = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(folderPath.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) &&
                    folderItem != nullptr)
                {
                    dialog->SetFolder(folderItem);
                    folderItem->Release();
                }
            }
        }
    }
    catch (...)
    {
    }

    const COMDLG_FILTERSPEC filters[] =
    {
        { L"Json", L"*.json" },
        { L"All", L"*.*" },
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

    const HRESULT showHr = dialog->Show(nullptr);
    if (SUCCEEDED(showHr))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
            {
                outFilePath = path;
                CoTaskMemFree(path);
            }

            item->Release();
        }
    }

    dialog->Release();

    if (shouldUninit)
        CoUninitialize();

    return !outFilePath.empty();
}

std::optional<ParsedBattleAuthoringFile> Try_ParseBattleAuthoringFileName(const fs::path& filePath)
{
    const string stem = filePath.stem().string();
    auto parseWithPrefix = [&stem](std::string_view prefix, BattleAuthoringFileKind kind) -> std::optional<ParsedBattleAuthoringFile>
    {
        if (!StartsWith(stem, prefix))
            return std::nullopt;

        const string remainder = stem.substr(prefix.size());
        if (remainder.empty())
            return std::nullopt;

        for (BattleCameraMode mode : magic_enum::enum_values<BattleCameraMode>())
        {
            const string modeName = string(magic_enum::enum_name(mode));
            if (remainder == modeName)
            {
                ParsedBattleAuthoringFile parsed{};
                parsed.kind = kind;
                parsed.mode = mode;
                return parsed;
            }

            const string modePrefix = modeName + "_";
            if (!StartsWith(remainder, modePrefix))
                continue;

            ParsedBattleAuthoringFile parsed{};
            parsed.kind = kind;
            parsed.mode = mode;
            parsed.skillId = remainder.substr(modePrefix.size());
            return parsed;
        }

        return std::nullopt;
    };

    if (auto parsed = parseWithPrefix("BattlePreset_", BattleAuthoringFileKind::Preset))
        return parsed;

    if (auto parsed = parseWithPrefix("BattleCameraClip_", BattleAuthoringFileKind::Clip))
        return parsed;

    return std::nullopt;
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

    Update_BattlePresetAuthoringInput(timeDelta);
    Update_TransitionPreviewPlayback(timeDelta);

    if (_timelinePlaying)
    {
        const float clipTimeDelta = (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
            ? timeDelta * kBattleAnimationPreviewFps
            : timeDelta;
        const float frameDelta = (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
            ? clipTimeDelta
            : clipTimeDelta * kBattleClipPreviewFps;
        _battleClipPlaybackFrameAccumulator += std::max(0.f, frameDelta);
        const int frameAdvance = static_cast<int>(std::floor(_battleClipPlaybackFrameAccumulator));
        if (frameAdvance > 0)
        {
            _battleClipCurrentFrame += frameAdvance;
            _battleClipPlaybackFrameAccumulator -= static_cast<float>(frameAdvance);
        }

        const int frameMax = Get_BattleClipPreviewFrameMax();
        if (_battleClipCurrentFrame >= frameMax)
        {
            _battleClipCurrentFrame = frameMax;
            _timelinePlaying = false;
            _battleClipPlaybackFrameAccumulator = 0.f;
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
    Ensure_PreviewSkyBackdrop();
    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);

    if (_previewRenderTarget == nullptr || _previewCamera == nullptr)
        return;

    if (_previewEnabled && _previewObjectsDirty)
        Rebuild_PreviewObjects();

    Refresh_PreviewAnimationList();
    if (_previewEnabled && _previewDirty)
    {
        Apply_PreviewContext();
        _previewDirty = false;
    }
    else if (_previewEnabled)
    {
        Position_PreviewObjects();
        Update_PreviewAttackActorMotion(_previewContext.timeSec);
        Apply_BattleCameraPreviewPose(_previewContext.timeSec);
    }

    Position_PreviewObjects();
    if (_previewEnabled)
        Update_PreviewAttackActorMotion(_previewContext.timeSec);

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

    const float dt = ImGui::GetIO().DeltaTime;
    if (_previewCamera != nullptr)
    {
        Shared<TransformCom> previewCameraTransform = _previewCamera->Get_Transform();
        if (previewCameraTransform != nullptr)
        {
            const Vec3 previewCameraPos = previewCameraTransform->Get_WorldPosition();
            for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
            {
                if (skyCloudCard == nullptr)
                    continue;

                Shared<TransformCom> skyCloudTransform = skyCloudCard->Get_Transform();
                if (skyCloudTransform == nullptr)
                    continue;

                skyCloudTransform->Set_WorldPosition(previewCameraPos + kPreviewSkyCloudLocalOffset);
            }
        }
    }

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

    if (_previewSkyBox != nullptr)
    {
        if (!_previewObjectsBegunPlay)
            _previewSkyBox->Try_BeginPlay();

        _previewSkyBox->Set_Visible(true);
        _previewSkyBox->Priority_Update(dt);
        _previewSkyBox->Update(dt);
    }

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard == nullptr)
            continue;

        if (!_previewObjectsBegunPlay)
            skyCloudCard->Try_BeginPlay();

        skyCloudCard->Set_Visible(true);
        skyCloudCard->Priority_Update(dt);
        skyCloudCard->Update(dt);
    }

    _previewObjectsBegunPlay = true;

    GAME->Backup_RenderGroup();

    if (_previewHeroObject != nullptr)
        _previewHeroObject->Late_Update(dt);

    if (_previewEnemyObject != nullptr)
        _previewEnemyObject->Late_Update(dt);

    if (_previewSkyBox != nullptr)
        _previewSkyBox->Late_Update(dt);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard != nullptr)
            skyCloudCard->Late_Update(dt);
    }

    Position_PreviewObjects();
    if (_previewEnabled)
        Update_PreviewAttackActorMotion(_previewContext.timeSec);

    GAME->Draw_RendererOnly();
    GAME->Restore_RenderGroup();
    Ensure_PreviewGridResources();
    Draw_PreviewGrid();
    EDITOR->Capture_BackBuffer_ToRenderTarget(_previewRenderTarget);

    GAME->Bind_BackBuffer();
    if (savedActiveCamera != nullptr)
        GAME->Change_ActiveCamera(savedActiveCamera);
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
    GAME->UpdateFrustum(savedView, savedProj);
    EDITOR->Apply_PreviewViewportSizeImmediate(savedViewportWidth, savedViewportHeight);
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

        Handle_BattleClipPlaybackShortcut();

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
        Render_LoadAuthoringFilePopup();
        Render_UnsavedSkillChangePopup();
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

    if (!_previewContext.skillId.empty())
    {
        auto iter = std::find(_skillIds.begin(), _skillIds.end(), _previewContext.skillId);
        _selectedSkillIndex = (iter != _skillIds.end())
            ? static_cast<int>(std::distance(_skillIds.begin(), iter))
            : -1;
    }
    else if (!_skillIds.empty() && _selectedSkillIndex < 0)
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
    desc.farPlane = 500.f;
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

void BattleCameraPreview_View::Ensure_PreviewGridResources()
{
    if (_previewGridBatch && _previewGridEffect && _previewGridInputLayout && _previewGridDepthState && _previewGridBlendState)
        return;

    static constexpr D3D11_INPUT_ELEMENT_DESC kGridInputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    _previewGridBatch = make_shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(GAME->Get_Context().Get());
    _previewGridEffect = make_shared<DirectX::BasicEffect>(GAME->Get_Device().Get());

    if (_previewGridBatch == nullptr || _previewGridEffect == nullptr)
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        return;
    }

    _previewGridEffect->SetVertexColorEnabled(true);

    const void* shaderByteCode = nullptr;
    size_t shaderByteCodeLength = 0;
    _previewGridEffect->GetVertexShaderBytecode(&shaderByteCode, &shaderByteCodeLength);

    if (FAILED(GAME->Get_Device()->CreateInputLayout(
        kGridInputElements,
        static_cast<UINT>(_countof(kGridInputElements)),
        shaderByteCode,
        shaderByteCodeLength,
        _previewGridInputLayout.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    if (FAILED(GAME->Get_Device()->CreateDepthStencilState(&depthDesc, _previewGridDepthState.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        _previewGridDepthState.Reset();
        _previewGridBlendState.Reset();
        return;
    }

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (FAILED(GAME->Get_Device()->CreateBlendState(&blendDesc, _previewGridBlendState.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        _previewGridDepthState.Reset();
        _previewGridBlendState.Reset();
    }
}

void BattleCameraPreview_View::Ensure_PreviewSkyBackdrop()
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

void BattleCameraPreview_View::Draw_PreviewGrid() const
{
    if (!_showPreviewGrid)
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewGridDepthState || !_previewGridBlendState)
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewGridDepthState.Get(), 0);
    context->OMSetBlendState(_previewGridBlendState.Get(), nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    _previewGridBatch->Begin();

    const Vec3 gridCenter = Vec3(_previewFormation.origin.x, _previewFormation.origin.y, _previewFormation.origin.z + 2.5f);
    constexpr int gridDivs = 48;
    constexpr float gridHalfExtent = 24.f;
    const float step = (gridHalfExtent * 2.f) / static_cast<float>(gridDivs);
    const float y = gridCenter.y + 0.02f;

    const XMVECTOR minorColor = XMVectorSet(0.70f, 0.88f, 0.96f, 0.12f);
    const XMVECTOR majorColor = XMVectorSet(0.82f, 0.94f, 0.98f, 0.24f);
    const XMVECTOR xAxisColor = XMVectorSet(0.98f, 0.72f, 0.80f, 0.45f);
    const XMVECTOR zAxisColor = XMVectorSet(0.62f, 0.94f, 1.f, 0.5f);

    for (int i = 0; i <= gridDivs; ++i)
    {
        const float offset = -gridHalfExtent + step * static_cast<float>(i);
        const bool isCenter = fabsf(offset) < 0.0001f;
        const bool isMajor = (i % 4) == 0;

        const XMVECTOR colorX = isCenter ? zAxisColor : (isMajor ? majorColor : minorColor);
        const XMVECTOR colorZ = isCenter ? xAxisColor : (isMajor ? majorColor : minorColor);

        DirectX::VertexPositionColor lineXStart(
            Vec3(gridCenter.x - gridHalfExtent, y, gridCenter.z + offset),
            colorX);
        DirectX::VertexPositionColor lineXEnd(
            Vec3(gridCenter.x + gridHalfExtent, y, gridCenter.z + offset),
            colorX);
        _previewGridBatch->DrawLine(lineXStart, lineXEnd);

        DirectX::VertexPositionColor lineZStart(
            Vec3(gridCenter.x + offset, y, gridCenter.z - gridHalfExtent),
            colorZ);
        DirectX::VertexPositionColor lineZEnd(
            Vec3(gridCenter.x + offset, y, gridCenter.z + gridHalfExtent),
            colorZ);
        _previewGridBatch->DrawLine(lineZStart, lineZEnd);
    }

    _previewGridBatch->End();
}

void BattleCameraPreview_View::Reset_DefaultFormation()
{
    _previewFormation.name = "Default Battle Layout";
    _previewFormation.origin = Vec3(22.f, 2.5f, -10.24f);
    Apply_DefaultHeroFormation();
    Apply_DefaultEnemyFormation();
}

void BattleCameraPreview_View::Apply_DefaultHeroFormation()
{
    for (int i = 0; i < static_cast<int>(_previewFormation.heroes.size()); ++i)
    {
        _previewFormation.heroes[i] =
        {
            "Hero " + std::to_string(i),
            kDefaultHeroSlotOffsets[i],
            Vec3::Zero,
        };
    }
}

void BattleCameraPreview_View::Apply_DefaultEnemyFormation()
{
    for (int i = 0; i < static_cast<int>(_previewFormation.enemies.size()); ++i)
    {
        _previewFormation.enemies[i] =
        {
            "Enemy " + std::to_string(i),
            kDefaultEnemySlotOffsets[i],
            kDefaultEnemySlotRotation,
        };
    }

    vector<Client::BattleMonsterSpawnDesc> spawnDescs = Resolve_SelectedEnemySpawnDescs();
    if (spawnDescs.empty())
        return;

    std::sort(
        spawnDescs.begin(),
        spawnDescs.end(),
        [](const Client::BattleMonsterSpawnDesc& a, const Client::BattleMonsterSpawnDesc& b)
        {
            if (a.offset.x != b.offset.x)
                return a.offset.x < b.offset.x;

            return a.offset.z < b.offset.z;
        });

    const size_t appliedCount = std::min(spawnDescs.size(), _previewFormation.enemies.size());
    for (size_t i = 0; i < appliedCount; ++i)
    {
        const int slotIndex = Resolve_EnemyPreviewSlotIndex(appliedCount, i);
        PreviewSlot& slot = _previewFormation.enemies[slotIndex];
        const Client::BattleMonsterSpawnDesc& spawnDesc = spawnDescs[i];

        slot.label = spawnDesc.prefabName.empty()
            ? "Enemy " + std::to_string(slotIndex)
            : spawnDesc.prefabName;
        slot.offset = Vec3(spawnDesc.offset.x, spawnDesc.offset.y, 6.f + spawnDesc.offset.z);
        slot.rotation = kDefaultEnemySlotRotation + spawnDesc.rotation;
    }
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
    _previewSkillPhaseBlocksDirty = true;
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

    ImGui::SameLine(ImGui::GetWindowWidth() - 350.f);
    if (ImGui::Button("Load Camera Json##Header"))
    {
        Refresh_AuthoringFileList();
        _authoringFileSearchText.clear();
        _selectedAuthoringFileIndex = -1;
        ImGui::OpenPopup("Load Battle Camera Json");
    }

    ImGui::SameLine();
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

    if (Select_EnumCombo("Mode", _previewContext.mode, kLeftPanelComboWidth))
    {
        _transitionPreviewEditTargetPinned = false;
        Apply_DefaultPreviewSlotsForMode(_previewContext.mode);
        Try_AutoSelectPreviewActorAndAnimation();
        _hasLoadedBattlePreset = false;
        _hasLoadedBattleClip = false;
        _previewDirty = true;
    }

    if (ImGui::Checkbox("Use Skill Override", &_useSkillOverride))
    {
        Request_SkillOverrideChange(_useSkillOverride, _selectedSkillIndex);
    }

    if (_useSkillOverride)
    {
        const string currentSkillId = Get_SelectedSkillId();
        const char* currentSkillLabel = currentSkillId.empty() ? "None" : currentSkillId.c_str();

        ImGui::SetNextItemWidth(kLeftPanelWideComboWidth);
        if (ImGui::BeginCombo("Skill", currentSkillLabel))
        {
            char skillSearchBuffer[128] = {};
            strncpy_s(skillSearchBuffer, _skillSearchText.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##SkillSearch", skillSearchBuffer, IM_ARRAYSIZE(skillSearchBuffer)))
                _skillSearchText = skillSearchBuffer;

            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(_skillIds.size()); ++i)
            {
                if (!Contains_CaseInsensitive(_skillIds[i], _skillSearchText))
                    continue;

                const bool isSelected = (_selectedSkillIndex == i);
                if (ImGui::Selectable(_skillIds[i].c_str(), isSelected))
                {
                    Request_SkillOverrideChange(true, i);
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
    }

    if (ImGui::Checkbox("Use Manual Authoring Target", &_useManualBattleAuthoringTarget))
    {
        RetargetLoadedBattleAuthoringDataToCurrentTarget();
    }

    char manualPresetBuffer[128] = {};
    const size_t manualPresetLength = std::min(_manualBattleAuthoringPresetId.size(), static_cast<size_t>(127));
    memcpy(manualPresetBuffer, _manualBattleAuthoringPresetId.data(), manualPresetLength);
    manualPresetBuffer[manualPresetLength] = '\0';
    char manualClipBuffer[128] = {};
    const size_t manualClipLength = std::min(_manualBattleAuthoringClipId.size(), static_cast<size_t>(127));
    memcpy(manualClipBuffer, _manualBattleAuthoringClipId.data(), manualClipLength);
    manualClipBuffer[manualClipLength] = '\0';
    ImGui::BeginDisabled(!_useManualBattleAuthoringTarget);
    ImGui::SetNextItemWidth(kLeftPanelWideComboWidth);
    if (ImGui::InputText("Authoring Preset Id", manualPresetBuffer, 128))
    {
        _manualBattleAuthoringPresetId = manualPresetBuffer;
        RetargetLoadedBattleAuthoringDataToCurrentTarget();
    }
    ImGui::SetNextItemWidth(kLeftPanelWideComboWidth);
    if (ImGui::InputText("Authoring Clip Id", manualClipBuffer, 128))
    {
        _manualBattleAuthoringClipId = manualClipBuffer;
        RetargetLoadedBattleAuthoringDataToCurrentTarget();
    }
    ImGui::EndDisabled();

    if (_useManualBattleAuthoringTarget)
        ImGui::TextDisabled("Manual authoring preset/clip ids override selected shot block ids for load/save.");

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

void BattleCameraPreview_View::Render_UnsavedSkillChangePopup()
{
    if (_requestOpenUnsavedSkillChangePopup)
    {
        ImGui::OpenPopup("Unsaved Battle Camera Changes");
        _requestOpenUnsavedSkillChangePopup = false;
    }

    if (!ImGui::BeginPopupModal("Unsaved Battle Camera Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextWrapped("Current preset or clip changes are not saved. Change the skill target and discard those edits?");
    ImGui::Separator();

    if (ImGui::Button("Discard Changes", ImVec2(150.f, 0.f)))
    {
        Apply_SkillOverrideChange(_pendingUseSkillOverride, _pendingSelectedSkillIndex);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void BattleCameraPreview_View::Render_PreviewActorSelector()
{
    ImGui::Separator();
    ImGui::TextDisabled("Preview Actors");

    if (Select_EnumCombo("Hero", _selectedHeroId, kLeftPanelComboWidth))
    {
        _previewObjectsDirty = true;
        _animationListDirty = true;
    }

    const char* currentEnemyLabel = Get_SelectedEnemyPrefabName().empty() ? "None" : Get_SelectedEnemyPrefabName().c_str();
    ImGui::SetNextItemWidth(kLeftPanelComboWidth);
    if (ImGui::BeginCombo("Enemy", currentEnemyLabel))
    {
        for (int i = 0; i < static_cast<int>(_enemyPrefabNames.size()); ++i)
        {
            const bool isSelected = _selectedEnemyPrefabIndex == i;
            if (ImGui::Selectable(_enemyPrefabNames[i].c_str(), isSelected))
            {
                _selectedEnemyPrefabIndex = i;
                Apply_DefaultEnemyFormation();
                _previewObjectsDirty = true;
                _animationListDirty = true;
                _previewDirty = true;
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

    ImGui::SetNextItemWidth(kSlotSideComboWidth);
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
                _animationListDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(kSlotComboWidth);
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
                _animationListDirty = true;
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
    ImGui::SetNextItemWidth(kFormationVec3InputWidth);
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
            ImGui::SetNextItemWidth(kFormationVec3InputWidth);
            if (ImGui::DragFloat3("Offset", offset, 0.05f))
            {
                slot.offset = Vec3(offset[0], offset[1], offset[2]);
                _previewDirty = true;
            }

            float rotation[3] = { slot.rotation.x, slot.rotation.y, slot.rotation.z };
            ImGui::SetNextItemWidth(kFormationVec3InputWidth);
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
            "Runtime preview capture");

        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewMax.y - 46.f),
            IM_COL32(190, 198, 210, 255),
            "Wheel/W/S: Distance | A/D: Aim Offset(좌/우) | Q/E: Pivot Height(상/하)");
        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewMax.y - 28.f),
            IM_COL32(190, 198, 210, 255),
            "RMB Drag: Yaw/Pitch | Ctrl + RMB Drag: Roll/Look Height");

    }
    else
    {
        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewPos.y + 12.f),
            IM_COL32(210, 216, 224, 255),
            "프리뷰 타겟이 준비되지 않았습니다.");
    }

    ImGui::EndChild();
}

void BattleCameraPreview_View::Render_PreviewPanel()
{
    ImGui::BeginChild("BattleCameraDetailsPanel", ImVec2(0.f, 0.f), true);

    Ensure_BattlePresetLoaded();

    ImGui::TextDisabled("Preview Context");
    ImGui::Separator();

    ImGui::Text("Mode : %s", magic_enum::enum_name(_previewContext.mode).data());
    ImGui::Text("Skill : %s", Get_SelectedSkillId().empty() ? "(default)" : Get_SelectedSkillId().c_str());
    ImGui::Text("Preset Id : %s", Get_CurrentBattlePresetAuthoringOverrideId().empty() ? "(default)" : Get_CurrentBattlePresetAuthoringOverrideId().c_str());
    ImGui::Text("Clip Id : %s", Get_CurrentBattleClipAuthoringOverrideId().empty() ? "(default)" : Get_CurrentBattleClipAuthoringOverrideId().c_str());
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
    if (ImGui::CollapsingHeader("Preview Shortcuts", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("편집하려면 프리뷰 위에 마우스");
        ImGui::BulletText("Mouse Wheel : adjust Distance");
        ImGui::BulletText("W / S : move Distance in / out");
        ImGui::BulletText("A / D : adjust Aim Offset");
        ImGui::BulletText("Q / E : adjust Pivot Height");
        ImGui::BulletText("Right Mouse Drag : adjust Yaw / Pitch");
        ImGui::BulletText("Ctrl + Right Mouse Drag Vertical : adjust Look Height");
        ImGui::BulletText("Ctrl + Right Mouse Drag Horizontal : adjust Roll Offset");
    }

    Render_BattlePresetEditor();

    ImGui::EndChild();
}

void BattleCameraPreview_View::Render_TransitionPreviewPanel()
{
    ImGui::Separator();
    ImGui::TextDisabled("Preset Transition Preview");

    if (ImGui::Button("Load Camera Json##TransitionPreview"))
    {
        Refresh_AuthoringFileList();
        _selectedAuthoringFileIndex = -1;
        ImGui::OpenPopup("Load Battle Camera Json");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Loop", &_transitionPreviewLoop);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Follow Active Step", &_transitionPreviewAutoFollowActiveStep);

    const float totalSequenceDuration = Get_TransitionSequenceDuration();
    if (totalSequenceDuration > 0.f)
    {
        const int totalFrame = std::max(1, static_cast<int>(std::round(totalSequenceDuration * kBattleClipPreviewFps)));
        int sequenceFrame = std::clamp(
            static_cast<int>(std::round(_transitionPreviewSequenceTimeSec * kBattleClipPreviewFps)),
            0,
            totalFrame);

        if (ImGui::Button("Prev Frame"))
        {
            Stop_TransitionPreviewPlayback();
            sequenceFrame = std::max(0, sequenceFrame - 1);
            _transitionPreviewSequenceTimeSec = static_cast<float>(sequenceFrame) / kBattleClipPreviewFps;
            Scrub_TransitionSequence(_transitionPreviewSequenceTimeSec);
        }

        ImGui::SameLine();
        if (ImGui::Button("Next Frame"))
        {
            Stop_TransitionPreviewPlayback();
            sequenceFrame = std::min(totalFrame, sequenceFrame + 1);
            _transitionPreviewSequenceTimeSec = static_cast<float>(sequenceFrame) / kBattleClipPreviewFps;
            Scrub_TransitionSequence(_transitionPreviewSequenceTimeSec);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Frame %d / %d", sequenceFrame, totalFrame);

        ImGui::SetNextItemWidth(320.f);
        if (ImGui::SliderFloat("Sequence Time", &_transitionPreviewSequenceTimeSec, 0.f, totalSequenceDuration, "%.2f s"))
        {
            Stop_TransitionPreviewPlayback();
            Scrub_TransitionSequence(_transitionPreviewSequenceTimeSec);
        }
    }

    if (ImGui::Button(_transitionPreviewPlaying ? "Pause Transition" : "Play Transition"))
    {
        if (_transitionPreviewPlaying)
            Stop_TransitionPreviewPlayback();
        else
            Start_TransitionPreviewPlayback();
    }

    ImGui::SameLine();
    if (ImGui::Button("Restart Sequence"))
    {
        Stop_TransitionPreviewPlayback();
        Start_TransitionPreviewPlayback();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset To Step 0") && !_transitionPreviewSteps.empty())
    {
        Stop_TransitionPreviewPlayback();
        _transitionPreviewSequenceTimeSec = 0.f;
        _transitionPreviewActiveStepIndex = 0;
        Rebuild_TransitionPreviewStepAtTime(0, 0.f);
    }

    ImGui::TextDisabled(
        "Playback : %s | Step %d / %d | Elapsed %.2f s | Edit Target %d",
        _transitionPreviewPlaying ? "Playing" : "Stopped",
        _transitionPreviewActiveStepIndex >= 0 ? (_transitionPreviewActiveStepIndex + 1) : 0,
        static_cast<int>(_transitionPreviewSteps.size()),
        _transitionPreviewElapsed,
        _transitionPreviewEditTargetStepIndex + 1);

    ImGui::Spacing();

    for (int i = 0; i < static_cast<int>(_transitionPreviewSteps.size()); ++i)
    {
        PreviewTransitionStep& step = _transitionPreviewSteps[i];
        const bool isActiveStep = (_transitionPreviewActiveStepIndex == i);
        ImGui::PushID(i);
        ImGui::Separator();
        if (isActiveStep)
            ImGui::TextColored(ImVec4(0.96f, 0.78f, 0.30f, 1.f), "Step %d  [Active]", i + 1);
        else
            ImGui::Text("Step %d", i + 1);

        ImGui::SetNextItemWidth(180.f);
        Select_EnumCombo("Mode", step.mode, 180.f);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.f);
        ImGui::DragFloat("Duration", &step.durationSec, 0.01f, 0.05f, 10.f, "%.2f s");
        step.durationSec = std::max(0.05f, step.durationSec);

        ImGui::TextDisabled(
            "Skill : %s | Animation : %s | Easing : follows loaded preset",
            step.skillId.empty() ? "(default)" : step.skillId.c_str(),
            step.animationName.empty() ? "(current)" : step.animationName.c_str());

        if (ImGui::Button("Apply Step"))
        {
            Stop_TransitionPreviewPlayback();
            _transitionPreviewActiveStepIndex = i;
            _transitionPreviewElapsed = 0.f;

            float sequenceTime = 0.f;
            for (int stepIndex = 0; stepIndex < i; ++stepIndex)
                sequenceTime += std::max(0.05f, _transitionPreviewSteps[stepIndex].durationSec);

            _transitionPreviewSequenceTimeSec = sequenceTime;
            Rebuild_TransitionPreviewStepAtTime(i, 0.f);
        }

        ImGui::SameLine();
        if (ImGui::Button("Set Edit Target"))
            Set_TransitionEditTargetStep(i);

        ImGui::SameLine();
        if (ImGui::Button("Use Current Preview"))
        {
            Capture_CurrentPreviewIntoTransitionStep(step);
            _previewDirty = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Jump Here"))
        {
            Stop_TransitionPreviewPlayback();

            float sequenceTime = 0.f;
            for (int stepIndex = 0; stepIndex < i; ++stepIndex)
                sequenceTime += std::max(0.05f, _transitionPreviewSteps[stepIndex].durationSec);

            Scrub_TransitionSequence(sequenceTime);
        }

        ImGui::SameLine();
        if (ImGui::Button("Pause Here"))
        {
            Stop_TransitionPreviewPlayback();

            float sequenceTime = 0.f;
            for (int stepIndex = 0; stepIndex < i; ++stepIndex)
                sequenceTime += std::max(0.05f, _transitionPreviewSteps[stepIndex].durationSec);

            Scrub_TransitionSequence(sequenceTime);
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove") && _transitionPreviewSteps.size() > 1)
        {
            _transitionPreviewSteps.erase(_transitionPreviewSteps.begin() + i);
            ImGui::PopID();
            break;
        }

        if (_transitionPreviewEditTargetStepIndex == i)
            ImGui::TextDisabled("Preset editor is pinned to this step.");
        else if (isActiveStep)
            ImGui::TextDisabled("Editing follows this active step via current preview mode.");

        ImGui::PopID();
    }

    if (ImGui::Button("Add Step"))
    {
        PreviewTransitionStep step{};
        if (!_transitionPreviewSteps.empty())
            step.mode = _transitionPreviewSteps.back().mode;

        _transitionPreviewSteps.push_back(step);
    }
}

void BattleCameraPreview_View::Render_BattlePresetEditor()
{
    const BattleCameraMode editorMode = Get_BattlePresetEditorMode();
    const string selectedPresetId = Get_CurrentBattlePresetAuthoringOverrideId();
    if (!_hasLoadedBattlePreset ||
        _loadedBattlePresetMode != editorMode ||
        _loadedBattlePresetSkillId != selectedPresetId)
    {
        Load_BattlePresetForMode(editorMode);
    }

    const bool useBossEnemyAttackDrift = editorMode == BattleCameraMode::ENEMYATTACK && _editingEnemyAttackBossDrift;
    if (!_hasLoadedBattleDrift ||
        _loadedBattleDriftMode != editorMode ||
        _loadedBattleDriftBossVariant != useBossEnemyAttackDrift)
        Load_BattleDriftForMode(editorMode);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Battle Preset Editor");
    ImGui::Text("Editing Mode : %s", magic_enum::enum_name(editorMode).data());
    if (_activeTimelineEditorTab != TimelineEditorTab::TransitionPreview)
        ImGui::TextDisabled("Editor follows preview context mode.");
    else if (_transitionPreviewAutoFollowActiveStep)
        ImGui::TextDisabled("Editor follows active transition step.");
    else if (_transitionPreviewEditTargetPinned)
        ImGui::TextDisabled("Editor is pinned to transition step %d.", _transitionPreviewEditTargetStepIndex + 1);
    else
        ImGui::TextDisabled("Editor follows preview context mode.");

    if (ImGui::Button("Load Preset"))
    {
        Load_BattlePresetForMode(editorMode);
        if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
            battleTurnCamera->Set_Preset(editorMode, _editingBattlePreset);
        _battlePresetDirty = false;
        _previewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Base Preset"))
    {
        Load_BattlePresetForMode(editorMode, true);
        if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
            battleTurnCamera->Set_Preset(editorMode, _editingBattlePreset);
        _battlePresetDirty = false;
        _previewDirty = true;
    }

    ImGui::TextDisabled("Load Preset starts from fallback and only applies the current shot/preset id override. Load Base Preset explicitly includes the mode default first.");

    if (ImGui::Button("Save Preset"))
    {
        Client::CameraPresetIO::SaveBattlePresetToFile(editorMode, _editingBattlePreset, selectedPresetId);
        if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
            battleTurnCamera->Set_Preset(editorMode, _editingBattlePreset);
        _battlePresetDirty = false;
    }

    bool changed = false;
    auto dragFloat = [&changed](const char* label, float& value, float speed, float minValue, float maxValue, const char* format = "%.3f")
    {
        ImGui::SetNextItemWidth(kPresetInputWidth);
        if (ImGui::DragFloat(label, &value, speed, minValue, maxValue, format))
            changed = true;
    };

    float fovDeg = XMConvertToDegrees(_editingBattlePreset.camera.fovy);
    ImGui::SetNextItemWidth(kPresetInputWidth);
    if (ImGui::DragFloat("FOV Deg", &fovDeg, 0.1f, 10.f, 120.f, "%.2f"))
    {
        _editingBattlePreset.camera.fovy = XMConvertToRadians(fovDeg);
        changed = true;
    }

    dragFloat("Distance", _editingBattlePreset.distance, 0.01f, 0.1f, 50.f);
    dragFloat("Side Offset", _editingBattlePreset.sideOffset, 0.01f, -20.f, 20.f);
    dragFloat("Height Offset", _editingBattlePreset.heightOffset, 0.01f, -20.f, 20.f);
    dragFloat("Aim Offset", _editingBattlePreset.aimOffset, 0.01f, -20.f, 20.f);
    dragFloat("Look Height", _editingBattlePreset.lookHeight, 0.01f, -10.f, 20.f);
    dragFloat("Pivot Height", _editingBattlePreset.pivotHeight, 0.01f, -10.f, 20.f);

    ImGui::Separator();
    dragFloat("Yaw Offset", _editingBattlePreset.yawOffset, 0.05f, -360.f, 360.f);
    dragFloat("Pitch Offset", _editingBattlePreset.pitchOffset, 0.05f, -89.f, 89.f);
    dragFloat("Roll Offset", _editingBattlePreset.rollOffset, 0.001f, -XM_PI, XM_PI);

    ImGui::Separator();
    dragFloat("Follow Speed", _editingBattlePreset.followSpeed, 0.01f, 0.f, 30.f);
    ImGui::SetNextItemWidth(kPresetInputWidth);
    if (Select_EnumCombo("Transition Easing", _editingBattlePreset.transitionEasing, kPresetInputWidth))
        changed = true;
    dragFloat("Transition Duration", _editingBattlePreset.transitionDuration, 0.01f, 0.01f, 5.f);
    dragFloat("Near Plane", _editingBattlePreset.camera.nearPlane, 0.001f, 0.001f, 10.f);
    dragFloat("Far Plane", _editingBattlePreset.camera.farPlane, 1.f, 10.f, 5000.f, "%.1f");
    if (ImGui::Checkbox("Snap On Enter", &_editingBattlePreset.snapOnEnter))
        changed = true;

    ImGui::Separator();
    if (ImGui::Checkbox("Enable Procedural Drift", &_editingBattleDrift.enabled))
        changed = true;

    if (editorMode == BattleCameraMode::ENEMYATTACK)
    {
        bool bossVariantChanged = false;
        if (ImGui::Checkbox("Boss EnemyAttack Drift", &_editingEnemyAttackBossDrift))
            bossVariantChanged = true;

        ImGui::SameLine();
        ImGui::TextDisabled(_editingEnemyAttackBossDrift ? "Saving to BattleDrift_ENEMYATTACK_Boss.json" : "Saving to BattleDrift_ENEMYATTACK.json");

        if (bossVariantChanged)
        {
            Load_BattleDriftForMode(editorMode);
            if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
                battleTurnCamera->Set_DriftProfile(editorMode, _editingBattleDrift, _editingEnemyAttackBossDrift);
            _previewDirty = true;
        }
    }

    if (ImGui::Button("Load Drift"))
    {
        Load_BattleDriftForMode(editorMode);
        if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
            battleTurnCamera->Set_DriftProfile(editorMode, _editingBattleDrift, useBossEnemyAttackDrift);
        _battleDriftDirty = false;
        _previewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Drift"))
    {
        Client::CameraPresetIO::SaveBattleDriftToFile(editorMode, _editingBattleDrift, useBossEnemyAttackDrift);
        if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
            battleTurnCamera->Set_DriftProfile(editorMode, _editingBattleDrift, useBossEnemyAttackDrift);
        _battleDriftDirty = false;
    }

    if (_editingBattleDrift.enabled)
    {
        dragFloat("Drift Lateral Amp", _editingBattleDrift.lateralAmplitude, 0.01f, 0.f, 20.f);
        dragFloat("Drift Vertical Amp", _editingBattleDrift.verticalAmplitude, 0.01f, 0.f, 20.f);
        dragFloat("Drift Depth Amp", _editingBattleDrift.depthAmplitude, 0.01f, 0.f, 20.f);
        dragFloat("Drift Roll Amp", _editingBattleDrift.rollAmplitude, 0.001f, 0.f, XM_PI);
        dragFloat("Drift Lateral Freq", _editingBattleDrift.lateralFrequency, 0.01f, 0.f, 10.f);
        dragFloat("Drift Vertical Freq", _editingBattleDrift.verticalFrequency, 0.01f, 0.f, 10.f);
        dragFloat("Drift Depth Freq", _editingBattleDrift.depthFrequency, 0.01f, 0.f, 10.f);
        dragFloat("Drift Roll Freq", _editingBattleDrift.rollFrequency, 0.01f, 0.f, 10.f);
    }

    if (changed)
    {
        if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
        {
            battleTurnCamera->Set_Preset(editorMode, _editingBattlePreset);
            battleTurnCamera->Set_DriftProfile(editorMode, _editingBattleDrift, useBossEnemyAttackDrift);
        }

        _battlePresetDirty = true;
        _battleDriftDirty = true;
        _previewDirty = true;
    }
}

void BattleCameraPreview_View::Render_TimelinePanel()
{
    ImGui::BeginChild("BattleCameraTimelinePanel", ImVec2(0.f, 0.f), true);

    if (ImGui::BeginTabBar("BattleCameraBottomTabs"))
    {
        if (ImGui::BeginTabItem("Clip Timeline"))
        {
            _activeTimelineEditorTab = TimelineEditorTab::ClipTimeline;
            ImGui::TextDisabled("Battle Clip Timeline");
            ImGui::Separator();

            const BattleCameraMode editorMode = Get_CurrentBattleAuthoringMode();
            const string selectedClipId = Get_CurrentBattleClipAuthoringOverrideId();
            ImGui::TextDisabled(
                "Clip Target : %s / %s",
                magic_enum::enum_name(editorMode).data(),
                selectedClipId.empty() ? "(default)" : selectedClipId.c_str());
            if (!_hasLoadedBattleClip ||
                _loadedBattleClipMode != editorMode ||
                _loadedBattleClipSkillId != selectedClipId)
            {
                Load_BattleClipForMode(editorMode);
                _previewDirty = true;
            }

            ImGui::SetNextItemWidth(140.f);
            if (ImGui::DragFloat("Duration", &_editingBattleClip.duration, 0.01f, 0.f, 30.f))
            {
                _editingBattleClip.duration = std::max(0.f, _editingBattleClip.duration);
                _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipFrameMax(_editingBattleClip));
                Sync_PreviewTimeFromTimeline();
                _battleClipDirty = true;
                _previewDirty = true;
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(170.f);
            const char* currentTimeModeLabel = Get_BattleClipTimeModeLabel(_editingBattleClip.timeMode);
            if (ImGui::BeginCombo("Time Mode", currentTimeModeLabel))
            {
                const BattleCameraClipTimeMode timeModes[] =
                {
                    BattleCameraClipTimeMode::AnimationSync,
                    BattleCameraClipTimeMode::Independent
                };

                for (BattleCameraClipTimeMode timeMode : timeModes)
                {
                    const bool isSelected = _editingBattleClip.timeMode == timeMode;
                    if (ImGui::Selectable(Get_BattleClipTimeModeLabel(timeMode), isSelected))
                    {
                        _editingBattleClip.timeMode = timeMode;
                        if (timeMode == BattleCameraClipTimeMode::AnimationSync)
                            _editingBattleClip.duration = 1.f;
                        _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipPreviewFrameMax());
                        Sync_PreviewTimeFromTimeline();
                        _battleClipDirty = true;
                        _previewDirty = true;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (ImGui::Button("Load Clip"))
            {
                Load_BattleClipForMode(editorMode);
                _battleClipDirty = false;
                _previewDirty = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Load Base Clip"))
            {
                Load_BattleClipForMode(editorMode, true);
                _battleClipDirty = false;
                _previewDirty = true;
            }

            ImGui::TextDisabled("Load Clip starts empty and only applies the current shot/clip id override. Load Base Clip explicitly pulls in the mode default first.");

            if (ImGui::Button("Save Clip"))
            {
                Client::BattleCameraClipIO::SaveBattleCameraClipToFile(editorMode, _editingBattleClip, selectedClipId);
                if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
                    battleTurnCamera->Set_Clip(editorMode, _editingBattleClip, selectedClipId);
                _battleClipDirty = false;
            }

            ImGui::SameLine();
            if (ImGui::Button(_timelinePlaying ? "Pause" : "Play"))
                Toggle_BattleClipPlayback();

            ImGui::SameLine();
            if (ImGui::Button("Go To Start"))
            {
                _timelinePlaying = false;
                _battleClipCurrentFrame = 0;
                Sync_PreviewTimeFromTimeline();
                _previewDirty = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear All Keys"))
            {
                _timelinePlaying = false;
                _battleClipCurrentFrame = 0;
                for (int trackIndex = 0; trackIndex < kBattleClipTimelineTrackCount; ++trackIndex)
                {
                    CameraTrack* track = Get_BattleClipTrackByIndex(_editingBattleClip, trackIndex);
                    if (track != nullptr)
                        track->keyframes.clear();
                }

                _battleClipSelectedTrackIndex = 0;
                _battleClipSelectedKeyIndex = -1;
                Sync_PreviewTimeFromTimeline();
                _battleClipDirty = true;
                _previewDirty = true;
            }

            ImGui::SameLine();
            ImGui::TextDisabled("Frame %d | Time %.3f s", _battleClipCurrentFrame, Get_CurrentBattleClipPreviewTimeSec());

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
                char animationSearchBuffer[128] = {};
                strncpy_s(animationSearchBuffer, _previewAnimationSearchText.c_str(), _TRUNCATE);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputText("##AnimationSearch", animationSearchBuffer, IM_ARRAYSIZE(animationSearchBuffer)))
                    _previewAnimationSearchText = animationSearchBuffer;

                ImGui::Separator();
                for (int i = 0; i < static_cast<int>(_previewAnimationNames.size()); ++i)
                {
                    if (!Contains_CaseInsensitive(_previewAnimationNames[i], _previewAnimationSearchText))
                        continue;

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
            Render_SkillShotTimeline();
            ImGui::Spacing();
            Render_BattleClipSequencer();
            Render_SelectedBattleClipKeyframe();
            Render_BattleClipDebugPanel();
            Render_BattleClipInsertKeyframePopup();
            Render_BattleClipRemoveKeyframePopup();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Transition Preview"))
        {
            _activeTimelineEditorTab = TimelineEditorTab::TransitionPreview;
            Render_TransitionPreviewPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

void BattleCameraPreview_View::Render_SkillShotTimeline()
{
    Refresh_PreviewSkillPhaseBlocks();

    const string selectedSkillId = Get_SelectedSkillId();
    if (selectedSkillId.empty())
    {
        ImGui::TextDisabled("Skill shot timeline is available when a skill override is selected.");
        return;
    }

    ImGui::Separator();
    ImGui::TextDisabled("Skill Shot Timeline");
    ImGui::Checkbox("Use Skill Timeline Preview", &_useSkillShotTimeline);
    ImGui::SameLine();
    ImGui::Checkbox("Preview Attack Movement", &_previewAttackMovementEnabled);
    bool shotTimingChanged = false;

    ImGui::SameLine();
    if (ImGui::Button("Add Block From Phase"))
        Add_PreviewShotBlockFromSelectedPhase();

    ImGui::SameLine();
    if (ImGui::Button("Add Block At Time"))
        Add_PreviewShotBlockAtCurrentTime();

    ImGui::SameLine();
    if (ImGui::Button("Remove Block"))
        Remove_SelectedPreviewShotBlock();

    ImGui::SameLine();
    if (ImGui::Button("Duplicate Block"))
        Duplicate_SelectedPreviewShotBlock();

    ImGui::SameLine();
    if (ImGui::Button("Fit To Phase"))
        Fit_SelectedPreviewShotBlockToActivePhase();

    const float timelineDurationSec = std::max(Get_PreviewSkillSequenceDuration(), Get_BattleClipAuthoringDurationSec());
    const float drawWidth = std::max(ImGui::GetContentRegionAvail().x, 320.f);
    const float phaseHeight = 28.f;
    const float shotHeight = 34.f;
    const float timelineHeight = phaseHeight + shotHeight + 36.f;
    const ImVec2 canvasSize(drawWidth, timelineHeight);

    ImGui::BeginChild("BattleSkillShotTimelineCanvas", canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasMax(canvasMin.x + drawWidth, canvasMin.y + timelineHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float innerMinX = canvasMin.x + 8.f;
    const float innerWidth = std::max(drawWidth - 16.f, 1.f);

    drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(24, 26, 31, 255), 6.f);

    auto timeToX = [&](float timeSec)
    {
        if (timelineDurationSec <= 0.0001f)
            return innerMinX;

        const float ratio = std::clamp(timeSec / timelineDurationSec, 0.f, 1.f);
        return innerMinX + ratio * innerWidth;
    };

    auto xToTime = [&](float xPos)
    {
        const float ratio = std::clamp((xPos - innerMinX) / innerWidth, 0.f, 1.f);
        return ratio * timelineDurationSec;
    };

    const float phaseTop = canvasMin.y + 10.f;
    bool interactedWithPhase = false;
    for (int i = 0; i < static_cast<int>(_previewSkillPhaseBlocks.size()); ++i)
    {
        const PreviewSkillPhaseBlock& block = _previewSkillPhaseBlocks[i];
        const float x0 = timeToX(block.startTimeSec);
        const float x1 = timeToX(block.startTimeSec + block.durationSec);
        const bool isSelectedPhase = _selectedPreviewSkillPhaseIndex == i;
        const PreviewSkillPhaseBlock* activePhase = Resolve_ActivePreviewSkillPhase(_previewContext.timeSec);
        const bool isActivePhase = activePhase == &block;
        ImU32 fillColor = IM_COL32(58, 64, 74, 255);
        ImU32 outlineColor = IM_COL32(110, 118, 134, 255);
        if (isSelectedPhase && isActivePhase)
        {
            fillColor = IM_COL32(92, 126, 175, 255);
            outlineColor = IM_COL32(194, 225, 255, 255);
        }
        else if (isSelectedPhase)
        {
            fillColor = IM_COL32(76, 98, 134, 255);
            outlineColor = IM_COL32(194, 225, 255, 255);
        }
        else if (isActivePhase)
        {
            fillColor = IM_COL32(58, 64, 74, 255);
            outlineColor = IM_COL32(140, 208, 255, 255);
        }

        const ImRect phaseRect(ImVec2(x0, phaseTop), ImVec2(x1, phaseTop + phaseHeight));
        drawList->AddRectFilled(phaseRect.Min, phaseRect.Max, fillColor, 4.f);
        drawList->AddRect(phaseRect.Min, phaseRect.Max, outlineColor, 4.f, 0, isActivePhase ? 2.f : 1.f);

        const string label = "P" + std::to_string(block.phaseIndex) + " " + block.animationKey;
        drawList->AddText(ImVec2(x0 + 6.f, phaseTop + 6.f), IM_COL32(225, 229, 236, 255), label.c_str());

        if (ImGui::IsMouseHoveringRect(phaseRect.Min, phaseRect.Max, false) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            _selectedPreviewSkillPhaseIndex = i;
            _previewContext.timeSec = block.startTimeSec;
            Sync_BattleClipFrameFromPreviewTime();
            _previewDirty = true;
            interactedWithPhase = true;
        }
    }

    const float shotTop = phaseTop + phaseHeight + 18.f;
    const float shotBottom = shotTop + shotHeight;
    const float handleWidth = 16.f;
    bool interactedWithBlock = false;
    for (int i = 0; i < static_cast<int>(_previewShotBlocks.size()); ++i)
    {
        PreviewCameraShotBlock& block = _previewShotBlocks[i];
        const float x0 = timeToX(block.startTimeSec);
        const float x1 = timeToX(block.endTimeSec);
        const bool isSelected = _selectedPreviewShotBlockIndex == i;
        const bool isActive = Resolve_ActivePreviewShotBlock(_previewContext.timeSec) == &block;
        const ImRect rect(ImVec2(x0, shotTop), ImVec2(std::max(x1, x0 + 8.f), shotBottom));
        const ImRect leftHandleRect(rect.Min, ImVec2(std::min(rect.Min.x + handleWidth, rect.Max.x), rect.Max.y));
        const ImRect rightHandleRect(ImVec2(std::max(rect.Max.x - handleWidth, rect.Min.x), rect.Min.y), rect.Max);
        ImU32 fillColor = IM_COL32(110, 82, 34, 255);
        ImU32 outlineColor = IM_COL32(190, 180, 150, 255);
        if (isSelected && isActive)
        {
            fillColor = IM_COL32(226, 167, 63, 255);
            outlineColor = IM_COL32(245, 229, 182, 255);
        }
        else if (isSelected)
        {
            fillColor = IM_COL32(201, 144, 46, 255);
            outlineColor = IM_COL32(245, 229, 182, 255);
        }
        else if (isActive)
        {
            fillColor = IM_COL32(110, 82, 34, 255);
            outlineColor = IM_COL32(140, 208, 255, 255);
        }

        drawList->AddRectFilled(rect.Min, rect.Max, fillColor, 4.f);
        drawList->AddRect(rect.Min, rect.Max, outlineColor, 4.f, 0, isActive ? 2.f : 1.f);
        drawList->AddRectFilled(leftHandleRect.Min, leftHandleRect.Max, IM_COL32(238, 220, 175, 180), 3.f);
        drawList->AddRectFilled(rightHandleRect.Min, rightHandleRect.Max, IM_COL32(238, 220, 175, 180), 3.f);

        string label = string(magic_enum::enum_name(block.mode));
        if (!block.presetId.empty() || !block.clipId.empty())
        {
            label += " / P:";
            label += block.presetId.empty() ? "-" : block.presetId;
            label += " / C:";
            label += block.clipId.empty() ? "-" : block.clipId;
        }
        drawList->AddText(ImVec2(rect.Min.x + 6.f, rect.Min.y + 8.f), IM_COL32(18, 18, 18, 255), label.c_str());

        const bool hoverLeftHandle = ImGui::IsMouseHoveringRect(leftHandleRect.Min, leftHandleRect.Max, false);
        const bool hoverRightHandle = ImGui::IsMouseHoveringRect(rightHandleRect.Min, rightHandleRect.Max, false);
        const bool hoverBlock = ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false);

        if ((hoverLeftHandle || hoverRightHandle || hoverBlock) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            Select_PreviewShotBlock(i, true);
            _previewShotDragBlockIndex = i;
            _previewShotDragOriginalStartSec = block.startTimeSec;
            _previewShotDragOriginalEndSec = block.endTimeSec;
            _previewShotDragGrabOffsetSec = xToTime(ImGui::GetIO().MousePos.x) - block.startTimeSec;
            _previewShotDragMode = hoverLeftHandle
                ? PreviewShotDragMode::ResizeStart
                : (hoverRightHandle ? PreviewShotDragMode::ResizeEnd : PreviewShotDragMode::Move);
            interactedWithBlock = true;
        }

        if (hoverBlock && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            Select_PreviewShotBlock(i, true);
            interactedWithBlock = true;
        }
    }

    if (_previewShotDragBlockIndex >= 0 &&
        _previewShotDragBlockIndex < static_cast<int>(_previewShotBlocks.size()) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        PreviewCameraShotBlock& draggedBlock = _previewShotBlocks[_previewShotDragBlockIndex];
        const float mouseTimeSec = xToTime(ImGui::GetIO().MousePos.x);
        const float minDurationSec = 1.f / kBattleClipPreviewFps;

        switch (_previewShotDragMode)
        {
        case PreviewShotDragMode::Move:
        {
            const float durationSec = std::max(minDurationSec, _previewShotDragOriginalEndSec - _previewShotDragOriginalStartSec);
            float newStartSec = Snap_PreviewShotTime(mouseTimeSec - _previewShotDragGrabOffsetSec, timelineDurationSec);
            newStartSec = std::clamp(newStartSec, 0.f, std::max(0.f, timelineDurationSec - durationSec));
            draggedBlock.startTimeSec = newStartSec;
            draggedBlock.endTimeSec = newStartSec + durationSec;
            shotTimingChanged = true;
            break;
        }
        case PreviewShotDragMode::ResizeStart:
        {
            float newStartSec = Snap_PreviewShotTime(mouseTimeSec, timelineDurationSec);
            draggedBlock.startTimeSec = std::clamp(newStartSec, 0.f, draggedBlock.endTimeSec - minDurationSec);
            shotTimingChanged = true;
            break;
        }
        case PreviewShotDragMode::ResizeEnd:
        {
            float newEndSec = Snap_PreviewShotTime(mouseTimeSec, timelineDurationSec);
            draggedBlock.endTimeSec = std::clamp(newEndSec, draggedBlock.startTimeSec + minDurationSec, std::max(timelineDurationSec, draggedBlock.startTimeSec + minDurationSec));
            shotTimingChanged = true;
            break;
        }
        case PreviewShotDragMode::None:
        default:
            break;
        }

        _previewDirty = true;
        interactedWithBlock = true;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        _previewShotDragMode = PreviewShotDragMode::None;
        _previewShotDragBlockIndex = -1;
    }

    const float playheadX = timeToX(_previewContext.timeSec);
    drawList->AddLine(ImVec2(playheadX, phaseTop - 4.f), ImVec2(playheadX, shotBottom + 4.f), IM_COL32(140, 208, 255, 255), 2.f);

    const ImRect timelineRect(ImVec2(innerMinX, phaseTop), ImVec2(innerMinX + innerWidth, shotBottom));
    if (!interactedWithPhase &&
        !interactedWithBlock &&
        ImGui::IsMouseHoveringRect(timelineRect.Min, timelineRect.Max, false) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        _previewContext.timeSec = xToTime(ImGui::GetIO().MousePos.x);
        const PreviewSkillPhaseBlock* activePhase = Resolve_ActivePreviewSkillPhase(_previewContext.timeSec);
        if (activePhase != nullptr)
        {
            for (int i = 0; i < static_cast<int>(_previewSkillPhaseBlocks.size()); ++i)
            {
                if (&_previewSkillPhaseBlocks[i] == activePhase)
                {
                    _selectedPreviewSkillPhaseIndex = i;
                    break;
                }
            }
        }
        Sync_BattleClipFrameFromPreviewTime();
        _previewDirty = true;
    }

    ImGui::Dummy(canvasSize);
    ImGui::EndChild();

    if (const PreviewSkillPhaseBlock* selectedPhase = Get_SelectedPreviewSkillPhase())
    {
        ImGui::TextDisabled(
            "Selected Phase: P%d %s | Start %.3f s | Duration %.3f s",
            selectedPhase->phaseIndex,
            selectedPhase->animationKey.c_str(),
            selectedPhase->startTimeSec,
            selectedPhase->durationSec);
    }

    if (PreviewCameraShotBlock* selectedBlock = Get_SelectedPreviewShotBlock())
    {
        selectedBlock->startTimeSec = std::max(0.f, selectedBlock->startTimeSec);
        selectedBlock->endTimeSec = std::max(selectedBlock->startTimeSec + 0.01f, selectedBlock->endTimeSec);

        ImGui::TextDisabled("Selected Shot Block");
        ImGui::SetNextItemWidth(130.f);
        if (ImGui::DragFloat("Shot Start", &selectedBlock->startTimeSec, 0.01f, 0.f, timelineDurationSec + 10.f))
        {
            _previewDirty = true;
            shotTimingChanged = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(130.f);
        if (ImGui::DragFloat("Shot End", &selectedBlock->endTimeSec, 0.01f, 0.f, timelineDurationSec + 10.f))
        {
            _previewDirty = true;
            shotTimingChanged = true;
        }

        if (Select_EnumCombo("Shot Mode", selectedBlock->mode, 180.f))
        {
            const bool wasActiveBlock = Resolve_ActivePreviewShotBlock(_previewContext.timeSec) == selectedBlock;
            const BattleCameraMode previousMode = _previewContext.mode;
            if (wasActiveBlock)
            {
                _previewContext.mode = selectedBlock->mode;
                _activePreviewShotMode = selectedBlock->mode;
                if (previousMode != selectedBlock->mode)
                    Apply_DefaultPreviewSlotsForMode(_previewContext.mode);
            }
            RetargetLoadedBattleAuthoringDataToCurrentTarget();
            _previewDirty = true;
        }
        char presetBuffer[128] = {};
        const size_t presetCopyLength = std::min(selectedBlock->presetId.size(), static_cast<size_t>(127));
        memcpy(presetBuffer, selectedBlock->presetId.data(), presetCopyLength);
        presetBuffer[presetCopyLength] = '\0';
        if (ImGui::InputText("Shot Preset Id", presetBuffer, 128))
        {
            selectedBlock->presetId = presetBuffer;
            const bool wasActiveBlock = Resolve_ActivePreviewShotBlock(_previewContext.timeSec) == selectedBlock;
            if (wasActiveBlock)
                _activePreviewShotPresetId = selectedBlock->presetId;
            RetargetLoadedBattleAuthoringDataToCurrentTarget();
            _previewDirty = true;
        }
        char clipBuffer[128] = {};
        const size_t clipCopyLength = std::min(selectedBlock->clipId.size(), static_cast<size_t>(127));
        memcpy(clipBuffer, selectedBlock->clipId.data(), clipCopyLength);
        clipBuffer[clipCopyLength] = '\0';
        if (ImGui::InputText("Shot Clip Id", clipBuffer, 128))
        {
            selectedBlock->clipId = clipBuffer;
            const bool wasActiveBlock = Resolve_ActivePreviewShotBlock(_previewContext.timeSec) == selectedBlock;
            if (wasActiveBlock)
                _activePreviewShotClipId = selectedBlock->clipId;
            RetargetLoadedBattleAuthoringDataToCurrentTarget();
            _previewDirty = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Jump To Shot"))
            Select_PreviewShotBlock(_selectedPreviewShotBlockIndex, true);

        ImGui::SameLine();
        if (ImGui::Button("Snap To Playhead"))
        {
            const float durationSec = std::max(1.f / kBattleClipPreviewFps, selectedBlock->endTimeSec - selectedBlock->startTimeSec);
            selectedBlock->startTimeSec = Snap_PreviewShotTime(_previewContext.timeSec, timelineDurationSec);
            selectedBlock->endTimeSec = selectedBlock->startTimeSec + durationSec;
            _previewDirty = true;
            shotTimingChanged = true;
        }

        ImGui::TextDisabled("Selected shot drives the preset/clip editor target and clicking a shot moves the preview to that shot.");
    }

    if (shotTimingChanged && _selectedPreviewShotBlockIndex >= 0 && _selectedPreviewShotBlockIndex < static_cast<int>(_previewShotBlocks.size()))
    {
        const PreviewCameraShotBlock selectedSnapshot = _previewShotBlocks[_selectedPreviewShotBlockIndex];
        std::sort(_previewShotBlocks.begin(), _previewShotBlocks.end(),
            [](const PreviewCameraShotBlock& lhs, const PreviewCameraShotBlock& rhs)
            {
                return lhs.startTimeSec < rhs.startTimeSec;
            });

        for (int i = 0; i < static_cast<int>(_previewShotBlocks.size()); ++i)
        {
            const PreviewCameraShotBlock& candidate = _previewShotBlocks[i];
            if (fabsf(candidate.startTimeSec - selectedSnapshot.startTimeSec) <= 0.0001f &&
                fabsf(candidate.endTimeSec - selectedSnapshot.endTimeSec) <= 0.0001f &&
                candidate.mode == selectedSnapshot.mode &&
                candidate.presetId == selectedSnapshot.presetId &&
                candidate.clipId == selectedSnapshot.clipId)
            {
                _selectedPreviewShotBlockIndex = i;
                break;
            }
        }
    }
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
    int previewFrameMax = Get_BattleClipPreviewFrameMax();

    BattleClipSequencerAdapter adapter(
        &_editingBattleClip,
        &_battleClipSelectedTrackIndex,
        &_battleClipSelectedKeyIndex,
        &_battleClipCurrentFrame,
        &consumedKeyClick,
        &requestOpenInsertPopup,
        &requestRemoveKeyPopup,
        &_battleClipContextTrackIndex,
        &_battleClipContextFrame,
        &previewFrameMax,
        _editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync,
        &_previewDirty,
        &_previewDistanceOffsetEnabled,
        &_previewFovOffsetEnabled,
        &_previewShakeEnabled,
        &_previewSideOffsetEnabled,
        &_previewPivotOffsetEnabled,
        &_previewHeightOffsetEnabled,
        &_previewAimOffsetEnabled,
        &_previewLookHeightOffsetEnabled,
        &_previewYawOffsetEnabled,
        &_previewOrbitYawEnabled,
        &_previewOrbitRadiusEnabled,
        &_previewOrbitHeightEnabled,
        &_previewOrbitLookHeightEnabled,
        &_previewShakeFrequencyXEnabled,
        &_previewShakeFrequencyYEnabled,
        &_previewShakeDecayRateXEnabled,
        &_previewShakeDecayRateEnabled,
        &_previewShakeVerticalRatioEnabled,
        &_previewShakeStrengthScaleEnabled);

    static bool expanded = true;
    static int firstFrame = 0;
    ImSequencer::Sequencer(&adapter, &_battleClipCurrentFrame, &expanded, &sequencerSelectedTrackIndex, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);

    if (!consumedKeyClick)
        _battleClipSelectedTrackIndex = sequencerSelectedTrackIndex;

    Refresh_SelectedBattleClipKey();

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
    return;
}

void BattleCameraPreview_View::Render_BattleClipDebugPanel()
{
    const int debugTrackIndex = Get_BattleClipTrackByIndex(_editingBattleClip, _battleClipSelectedTrackIndex) != nullptr
        ? _battleClipSelectedTrackIndex
        : kBattleClipDebugFallbackTrackIndex;
    const CameraTrack* debugTrack = Get_BattleClipTrackByIndex(_editingBattleClip, debugTrackIndex);
    if (debugTrack == nullptr)
        return;

    const float previewTimeSec = _previewContext.timeSec;
    const float previewClipTimeSec = Get_CurrentBattleClipPreviewTimeSec();
    const float animationDurationSec = Get_CurrentPreviewAnimationDurationSec();
    const int previewFrameMax = Get_BattleClipPreviewFrameMax();
    const BattleCameraClipSample clipSample = Client::Sample_BattleCameraClip(_editingBattleClip, previewClipTimeSec);
    const float trackSample = Client::Sample_BattleCameraTrack(*debugTrack, previewClipTimeSec);
    const bool isSorted = Is_BattleClipTrackSorted(*debugTrack);

    ImGui::Separator();
    ImGui::TextDisabled("Clip Debug");
    ImGui::Text("Frame: %d / %d", _battleClipCurrentFrame, previewFrameMax);
    ImGui::Text("Preview Time: %.3f s", previewTimeSec);
    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
    {
        ImGui::Text("Clip Time: %.3f", previewClipTimeSec);
        ImGui::Text("Clip Duration: %.3f", 1.f);
    }
    else
    {
        ImGui::Text("Clip Time: %.3f s", previewClipTimeSec);
        ImGui::Text("Clip Duration: %.3f s", _editingBattleClip.duration);
    }
    ImGui::Text("Skill Sequence Duration: %.3f s", Get_PreviewSkillSequenceDuration());
    ImGui::Text("Time Mode: %s", Get_BattleClipTimeModeLabel(_editingBattleClip.timeMode));
    ImGui::Text("Animation Duration: %.3f s", animationDurationSec);
    ImGui::Text("Debug Track: %s (%d)", Get_BattleClipTrackLabel(debugTrackIndex), debugTrackIndex);
    ImGui::Text("Track Key Count: %d", static_cast<int>(debugTrack->keyframes.size()));
    ImGui::Text("Track Sorted: %s", isSorted ? "Yes" : "No");

    if (!debugTrack->keyframes.empty())
    {
        const CameraKeyframe& firstKey = debugTrack->keyframes.front();
        const CameraKeyframe& lastKey = debugTrack->keyframes.back();
        ImGui::Text("First Key: time=%.3f value=%.3f", firstKey.time, firstKey.value);
        ImGui::Text("Last Key:  time=%.3f value=%.3f", lastKey.time, lastKey.value);
    }
    else
    {
        ImGui::TextDisabled("First Key: (none)");
        ImGui::TextDisabled("Last Key: (none)");
    }

    ImGui::Text("Track Sample @ %.3f: %.3f", previewTimeSec, trackSample);
    ImGui::Text("Clip Yaw Sample @ %.3f: %.3f", previewTimeSec, clipSample.yawOffset);
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
        _battleClipDirty = true;
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

    CameraTrack* track = Get_SelectedBattleClipTrack();
    CameraKeyframe* keyframe = Get_SelectedBattleClipKeyframe();

    if (track != nullptr && keyframe != nullptr)
    {
        ImGui::TextDisabled("Track %d | Key %d", _battleClipSelectedTrackIndex, _battleClipSelectedKeyIndex);

        const float authoringDurationSec = Get_BattleClipAuthoringDurationSec();
        const char* timeLabel = _editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync
            ? "Key Time (Norm)"
            : "Key Time";

        ImGui::SetNextItemWidth(180.f);
        if (ImGui::DragFloat(timeLabel, &keyframe->time, 0.01f, 0.f, authoringDurationSec))
        {
            keyframe->time = std::clamp(keyframe->time, 0.f, authoringDurationSec);
            if (keyframe->time > _editingBattleClip.duration)
                _editingBattleClip.duration = keyframe->time;

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

            _battleClipCurrentFrame = Get_BattleClipFrameFromTime(keyframe->time);
            Sync_PreviewTimeFromTimeline();
            _battleClipDirty = true;
            _previewDirty = true;
            keyframe = Get_SelectedBattleClipKeyframe();
        }

        if (keyframe != nullptr)
        {
            ImGui::SetNextItemWidth(180.f);
            if (ImGui::DragFloat("Key Value", &keyframe->value, 0.01f))
            {
                _battleClipDirty = true;
                _previewDirty = true;
            }

            ImGui::SetNextItemWidth(180.f);
            const char* interpolationLabel = magic_enum::enum_name(keyframe->interpolation).data();
            if (ImGui::BeginCombo("Interpolation", interpolationLabel))
            {
                for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
                {
                    const bool isSelected = keyframe->interpolation == mode;
                    if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
                    {
                        keyframe->interpolation = mode;
                        _battleClipDirty = true;
                        _previewDirty = true;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
        }

        ImGui::Separator();
    }

    if (ImGui::Button("Remove Selected Key"))
    {
        if (track != nullptr &&
            _battleClipSelectedKeyIndex >= 0 &&
            _battleClipSelectedKeyIndex < static_cast<int>(track->keyframes.size()))
        {
            track->keyframes.erase(track->keyframes.begin() + _battleClipSelectedKeyIndex);
            _battleClipSelectedKeyIndex = std::clamp(_battleClipSelectedKeyIndex, -1, static_cast<int>(track->keyframes.size()) - 1);
            _battleClipDirty = true;
            _previewDirty = true;
        }

        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void BattleCameraPreview_View::Render_LoadAuthoringFilePopup()
{
    if (!ImGui::BeginPopupModal("Load Battle Camera Json", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (_authoringFilePaths.empty())
    {
        ImGui::TextDisabled("No battle camera json files found.");
    }
    else
    {
        char authoringSearchBuffer[128] = {};
        strncpy_s(authoringSearchBuffer, _authoringFileSearchText.c_str(), _TRUNCATE);
        ImGui::SetNextItemWidth(460.f);
        if (ImGui::InputText("Search", authoringSearchBuffer, IM_ARRAYSIZE(authoringSearchBuffer)))
            _authoringFileSearchText = authoringSearchBuffer;

        if (ImGui::Button("Refresh List"))
        {
            Refresh_AuthoringFileList();
            _selectedAuthoringFileIndex = -1;
        }

        if (ImGui::BeginListBox("##BattleAuthoringFiles", ImVec2(460.f, 260.f)))
        {
            for (int i = 0; i < static_cast<int>(_authoringFilePaths.size()); ++i)
            {
                const string label = _authoringFilePaths[i].filename().string();
                if (!Contains_CaseInsensitive(label, _authoringFileSearchText))
                    continue;

                const bool isSelected = (_selectedAuthoringFileIndex == i);

                if (ImGui::Selectable(label.c_str(), isSelected))
                    _selectedAuthoringFileIndex = i;

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    _selectedAuthoringFileIndex = i;
                    Load_SelectedAuthoringFile();
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndListBox();
        }
    }

    ImGui::Separator();

    if (_selectedAuthoringFileIndex >= 0 &&
        _selectedAuthoringFileIndex < static_cast<int>(_authoringFilePaths.size()))
    {
        ImGui::TextDisabled("Selected : %s", _authoringFilePaths[_selectedAuthoringFileIndex].filename().string().c_str());
    }
    else
    {
        ImGui::TextDisabled("No file selected.");
    }

    if (_selectedAuthoringFileIndex < 0)
        ImGui::BeginDisabled();

    if (ImGui::Button("Load", ImVec2(120.f, 0.f)))
    {
        Load_SelectedAuthoringFile();
        ImGui::CloseCurrentPopup();
    }

    if (_selectedAuthoringFileIndex < 0)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void BattleCameraPreview_View::Apply_PreviewContext()
{
    _previewEnabled = true;

    if (_previewObjectsDirty)
        Rebuild_PreviewObjects();

    Apply_ActivePreviewShotBlock(_previewContext.timeSec);
    Sync_BattleClipFrameFromPreviewTime();
    Apply_PreviewAnimation(_previewContext.timeSec);
    Update_PreviewAttackActorMotion(_previewContext.timeSec);
    Apply_BattleCameraPreviewPose(_previewContext.timeSec);
    _previewDirty = false;
}

void BattleCameraPreview_View::Apply_PreviewAnimation(float previewTimeSec) const
{
    Shared<ModelCom> model = Resolve_PreviewSubjectModel();
    if (model == nullptr)
        return;

    string animationName{};
    if (_useSkillShotTimeline)
    {
        if (const PreviewSkillPhaseBlock* currentPhase = Resolve_CurrentPreviewSkillPhase(previewTimeSec))
            animationName = currentPhase->animationKey;
    }

    if (animationName.empty() &&
        _selectedPreviewAnimationIndex >= 0 &&
        _selectedPreviewAnimationIndex < static_cast<int>(_previewAnimationNames.size()))
    {
        animationName = _previewAnimationNames[_selectedPreviewAnimationIndex];
    }

    if (!animationName.empty())
    {
        const char* currentAnimationName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
        if (currentAnimationName == nullptr || animationName != currentAnimationName)
            model->Set_Animation(animationName, false, 0.f);
    }

    model->Set_AnimationPaused(true);
    float animationTimeSec = previewTimeSec;
    if (_useSkillShotTimeline)
    {
        if (const PreviewSkillPhaseBlock* currentPhase = Resolve_CurrentPreviewSkillPhase(previewTimeSec))
            animationTimeSec = std::max(0.f, previewTimeSec - currentPhase->startTimeSec);
    }

    const BattleCameraClipTimeMode previewTimeMode = _editingBattleClip.timeMode;

    if (previewTimeMode == BattleCameraClipTimeMode::Independent)
        animationTimeSec = std::clamp(animationTimeSec, 0.f, std::max(0.f, model->Get_CurrentAnimationDurationSec()));

    model->Set_CurrentAnimationTimeSec(animationTimeSec);
    model->Refresh_CurrentAnimationPose();
}

void BattleCameraPreview_View::Apply_BattleCameraPreviewPose(float previewTimeSec)
{
    if (_previewCamera == nullptr)
        return;

    Ensure_BattlePresetLoaded();
    const BattleCameraMode previewMode = _previewContext.mode;
    const BattleCameraPreset& preset = _editingBattlePreset;
    const float previewClipTimeSec = Get_CurrentBattleClipPreviewTimeSec();
    const BattleCameraClipSample clipSample = Uses_RuntimeBattleClip(previewMode)
        ? Client::Sample_BattleCameraClip(_editingBattleClip, previewClipTimeSec)
        : BattleCameraClipSample{};
    BattleCameraClipSample previewClipSample = clipSample;
    if (!_previewFovOffsetEnabled)
        previewClipSample.fovOffset = 0.f;
    if (!_previewDistanceOffsetEnabled)
        previewClipSample.distanceOffset = 0.f;
    if (!_previewShakeEnabled)
        previewClipSample.shakeAmplitude = 0.f;
    if (!_previewShakeFrequencyXEnabled)
        previewClipSample.shakeFrequencyX = BattleCameraClipSample{}.shakeFrequencyX;
    if (!_previewShakeFrequencyYEnabled)
        previewClipSample.shakeFrequencyY = BattleCameraClipSample{}.shakeFrequencyY;
    if (!_previewShakeDecayRateXEnabled)
        previewClipSample.shakeDecayRateX = BattleCameraClipSample{}.shakeDecayRateX;
    if (!_previewShakeDecayRateEnabled)
        previewClipSample.shakeDecayRate = BattleCameraClipSample{}.shakeDecayRate;
    if (!_previewShakeVerticalRatioEnabled)
        previewClipSample.shakeVerticalRatio = BattleCameraClipSample{}.shakeVerticalRatio;
    if (!_previewShakeStrengthScaleEnabled)
        previewClipSample.shakeStrengthScale = BattleCameraClipSample{}.shakeStrengthScale;
    if (!_previewYawOffsetEnabled)
        previewClipSample.yawOffset = 0.f;
    if (!_previewSideOffsetEnabled)
        previewClipSample.sideOffset = 0.f;
    if (!_previewPivotOffsetEnabled)
        previewClipSample.pivotOffset = 0.f;
    if (!_previewHeightOffsetEnabled)
        previewClipSample.heightOffset = 0.f;
    if (!_previewAimOffsetEnabled)
        previewClipSample.aimOffset = 0.f;
    if (!_previewLookHeightOffsetEnabled)
        previewClipSample.lookHeightOffset = 0.f;
    if (!_previewOrbitYawEnabled)
        previewClipSample.orbitYaw = 0.f;
    if (!_previewOrbitRadiusEnabled)
        previewClipSample.orbitRadius = 0.f;
    if (!_previewOrbitHeightEnabled)
        previewClipSample.orbitHeight = 0.f;
    if (!_previewOrbitLookHeightEnabled)
        previewClipSample.orbitLookHeight = 0.f;

    const float finalFovY = std::clamp(preset.camera.fovy + previewClipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));

    Vec3 subjectPos = Get_PreviewSlotWorldPosition(_previewContext.subjectSide, _previewContext.subjectSlot);
    Vec3 opponentPos = Get_PreviewSlotWorldPosition(_previewContext.opponentSide, _previewContext.opponentSlot);

    if (previewMode != BattleCameraMode::WIDE)
    {
        if (Shared<GameObject> subjectObject = Resolve_PreviewBattleActor(_previewContext.subjectSide))
        {
            if (Shared<TransformCom> transform = subjectObject->Get_Transform())
                subjectPos = transform->Get_WorldPosition();
        }

        if (Shared<GameObject> opponentObject = Resolve_PreviewBattleActor(_previewContext.opponentSide))
        {
            if (Shared<TransformCom> transform = opponentObject->Get_Transform())
                opponentPos = transform->Get_WorldPosition();
        }
    }

    const bool isPoseStateChanged =
        !_previewHasSmoothedTarget ||
        _previewPoseMode != previewMode ||
        _previewPoseSubjectSide != _previewContext.subjectSide ||
        _previewPoseSubjectSlot != _previewContext.subjectSlot ||
        previewTimeSec < _previewPoseLastTimeSec;

    if (isPoseStateChanged || preset.snapOnEnter)
    {
        _previewSmoothedTargetPos = subjectPos;
        _previewHasSmoothedTarget = true;
    }

    if (previewMode == BattleCameraMode::WIDE)
    {
        const float poseTimeDelta = isPoseStateChanged ? 0.f : std::max(0.f, previewTimeSec - _previewPoseLastTimeSec);
        _previewSmoothedTargetPos = Vec3::Lerp(
            _previewSmoothedTargetPos,
            subjectPos,
            poseTimeDelta * preset.followSpeed);
    }
    else
    {
        // Match current runtime behavior in BattleTurnCamera::Update_FromCurrentPreset():
        // subject focus modes use the actor's current world position directly.
        _previewSmoothedTargetPos = subjectPos;
    }

    _previewPoseMode = previewMode;
    _previewPoseSubjectSide = _previewContext.subjectSide;
    _previewPoseSubjectSlot = _previewContext.subjectSlot;
    _previewPoseLastTimeSec = previewTimeSec;

    Vec3 desiredPos = Vec3::Zero;
    Vec3 lookTarget = Vec3::Zero;
    float evaluatedFovY = finalFovY;
    float evaluatedRollOffset = preset.rollOffset;

    if (previewMode == BattleCameraMode::WIDE)
    {
        const PreviewSlot* yawBaseSlot = Get_PreviewSlot(PreviewSlotSide::Hero, 1);
        if (yawBaseSlot == nullptr)
            yawBaseSlot = Get_PreviewSlot(PreviewSlotSide::Hero, 0);

        if (yawBaseSlot == nullptr)
            return;

        Vec3 heroCenter = Vec3::Zero;
        uint32 heroCount = 0;
        for (int i = 0; i < static_cast<int>(_previewFormation.heroes.size()); ++i)
        {
            heroCenter += Get_PreviewSlotWorldPosition(PreviewSlotSide::Hero, i);
            ++heroCount;
        }

        if (heroCount == 0)
            return;

        heroCenter /= static_cast<float>(heroCount);

        Client::BattleTurnCameraEvaluatedPose pose{};
        if (Client::Evaluate_BattleTurnWidePose(
            heroCenter,
            yawBaseSlot->rotation.y,
            preset.camera.fovy,
            0.f,
            0.f,
            0.f,
            pose))
        {
            desiredPos = pose.cameraPos;
            lookTarget = pose.lookTarget;
            evaluatedFovY = pose.fovY;
            evaluatedRollOffset = pose.rollOffset;
        }
    }
    else if (previewMode == BattleCameraMode::ENEMYATTACK && _previewContext.opponentSide != _previewContext.subjectSide)
    {
        Client::BattleTurnCameraEvaluatedPose pose{};
        if (Client::Evaluate_BattleTurnEnemyAttackPose(subjectPos, opponentPos, preset, previewClipSample, previewClipTimeSec, pose))
        {
            desiredPos = pose.cameraPos;
            lookTarget = pose.lookTarget;
            evaluatedFovY = pose.fovY;
            evaluatedRollOffset = pose.rollOffset;
        }
    }
    else
    {
        Client::BattleTurnCameraEvaluatedPose pose{};
        if (Client::Evaluate_BattleTurnPresetPose(
            subjectPos,
            preset,
            previewClipSample,
            0.f,
            0.f,
            &_previewSmoothedTargetPos,
            previewClipTimeSec,
            pose))
        {
            desiredPos = pose.cameraPos;
            lookTarget = pose.lookTarget;
            evaluatedFovY = pose.fovY;
            evaluatedRollOffset = pose.rollOffset;
        }
    }

    Client::BattleTurnCameraEvaluatedPose driftPose{};
    driftPose.cameraPos = desiredPos;
    driftPose.lookTarget = lookTarget;
    driftPose.fovY = evaluatedFovY;
    driftPose.rollOffset = evaluatedRollOffset;
    Apply_PreviewProceduralDrift(_editingBattleDrift, previewTimeSec, driftPose);
    desiredPos = driftPose.cameraPos;
    lookTarget = driftPose.lookTarget;
    evaluatedFovY = driftPose.fovY;
    evaluatedRollOffset = driftPose.rollOffset;

    _previewCamera->Set_FovY(evaluatedFovY);
    if (Shared<TransformCom> transform = _previewCamera->Get_Transform())
    {
        transform->Set_WorldPosition(desiredPos);
        transform->LookAt(lookTarget);
        transform->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, evaluatedRollOffset));
    }
}

float BattleCameraPreview_View::Get_CurrentPreviewAnimationDurationSec() const
{
    if (_useSkillShotTimeline)
    {
        if (const PreviewSkillPhaseBlock* currentPhase = Resolve_CurrentPreviewSkillPhase(_previewContext.timeSec))
            return std::max(0.f, currentPhase->durationSec);
    }

    Shared<ModelCom> model = Resolve_PreviewSubjectModel();
    if (model == nullptr)
        return 0.f;

    return std::max(0.f, model->Get_CurrentAnimationDurationSec());
}

float BattleCameraPreview_View::Get_BattleClipAuthoringDurationSec() const
{
    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
        return 1.f;

    float referenceDurationSec = Get_CurrentPreviewAnimationDurationSec();

    if (const PreviewCameraShotBlock* selectedBlock = Get_SelectedPreviewShotBlock())
    {
        referenceDurationSec = std::max(
            referenceDurationSec,
            std::max(0.01f, selectedBlock->endTimeSec - selectedBlock->startTimeSec));
    }
    else if (const PreviewCameraShotBlock* activeBlock = Resolve_ActivePreviewShotBlock(_previewContext.timeSec))
    {
        referenceDurationSec = std::max(
            referenceDurationSec,
            std::max(0.01f, activeBlock->endTimeSec - activeBlock->startTimeSec));
    }
    else if (_useSkillShotTimeline)
    {
        referenceDurationSec = std::max(referenceDurationSec, Get_PreviewSkillSequenceDuration());
    }

    return std::max(_editingBattleClip.duration, referenceDurationSec);
}

int BattleCameraPreview_View::Get_BattleClipPreviewFrameMax() const
{
    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
        return std::max(1, static_cast<int>(std::lround(Get_CurrentBattleClipTimeRangeSec() * kBattleAnimationPreviewFps)));

    const float previewDurationSec = Get_BattleClipAuthoringDurationSec();
    return std::max(0, static_cast<int>(std::lround(previewDurationSec * kBattleClipPreviewFps)));
}

float BattleCameraPreview_View::Get_CurrentBattleClipTimeOriginSec() const
{
    if (const PreviewSkillPhaseBlock* selectedPhase = Get_SelectedPreviewSkillPhase())
        return selectedPhase->startTimeSec;

    if (const PreviewCameraShotBlock* selectedBlock = Get_SelectedPreviewShotBlock())
        return selectedBlock->startTimeSec;

    if (const PreviewCameraShotBlock* activeBlock = Resolve_ActivePreviewShotBlock(_previewContext.timeSec))
        return activeBlock->startTimeSec;

    return 0.f;
}

float BattleCameraPreview_View::Get_CurrentBattleClipTimeRangeSec() const
{
    if (const PreviewSkillPhaseBlock* selectedPhase = Get_SelectedPreviewSkillPhase())
        return std::max(0.0001f, selectedPhase->durationSec);

    if (const PreviewSkillPhaseBlock* activePhase = Resolve_ActivePreviewSkillPhase(_previewContext.timeSec))
        return std::max(0.0001f, activePhase->durationSec);

    return std::max(0.0001f, Get_CurrentPreviewAnimationDurationSec());
}

float BattleCameraPreview_View::Get_CurrentBattleClipPreviewTimeSec() const
{
    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
    {
        const float localTimeSec = std::max(0.f, _previewContext.timeSec - Get_CurrentBattleClipTimeOriginSec());
        return std::clamp(localTimeSec / Get_CurrentBattleClipTimeRangeSec(), 0.f, 1.f);
    }

    return std::max(0.f, _previewContext.timeSec - Get_CurrentBattleClipTimeOriginSec());
}

float BattleCameraPreview_View::Get_BattleClipTimeFromFrame(int frame) const
{
    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
    {
        const int frameMax = std::max(1, Get_BattleClipPreviewFrameMax());
        return std::clamp(static_cast<float>(frame) / static_cast<float>(frameMax), 0.f, 1.f);
    }

    return std::max(0.f, static_cast<float>(frame) / kBattleClipPreviewFps);
}

int BattleCameraPreview_View::Get_BattleClipFrameFromTime(float clipTime) const
{
    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
    {
        const int frameMax = std::max(1, Get_BattleClipPreviewFrameMax());
        return std::clamp(static_cast<int>(std::lround(std::clamp(clipTime, 0.f, 1.f) * static_cast<float>(frameMax))), 0, frameMax);
    }

    return std::max(0, static_cast<int>(std::lround(std::max(0.f, clipTime) * kBattleClipPreviewFps)));
}

void BattleCameraPreview_View::Sync_BattleClipFrameFromPreviewTime()
{
    const float previewClipTimeSec = Get_CurrentBattleClipPreviewTimeSec();
    _battleClipCurrentFrame = std::clamp(
        Get_BattleClipFrameFromTime(previewClipTimeSec),
        0,
        Get_BattleClipPreviewFrameMax());
}

void BattleCameraPreview_View::Clear_PreviewContext()
{
    _previewEnabled = false;
    _activePreviewShotPresetId.clear();
    _activePreviewShotClipId.clear();
    Reset_PreviewPoseState();
    Clear_PreviewObjects();
    _previewObjectsDirty = true;
    _previewDirty = false;
    _timelinePlaying = false;
}

void BattleCameraPreview_View::Ensure_BattlePresetLoaded()
{
    const string selectedPresetId = Get_CurrentBattlePresetAuthoringOverrideId();

    if (_hasLoadedBattlePreset &&
        _loadedBattlePresetMode == _previewContext.mode &&
        _loadedBattlePresetSkillId == selectedPresetId)
        return;

    Load_BattlePresetForCurrentMode();
}

void BattleCameraPreview_View::Load_BattlePresetForMode(BattleCameraMode mode, bool loadBase)
{
    _editingBattlePreset = Make_FallbackBattleCameraPreset();
    const string selectedPresetId = Get_CurrentBattlePresetAuthoringOverrideId();
    const bool shouldLoadBasePreset = loadBase || selectedPresetId.empty();

    if (shouldLoadBasePreset)
        Client::CameraPresetIO::LoadBattlePresetFromFile(mode, _editingBattlePreset);

    if (!selectedPresetId.empty())
        Client::CameraPresetIO::LoadBattlePresetFromFile(mode, _editingBattlePreset, selectedPresetId);

    _loadedBattlePresetMode = mode;
    _loadedBattlePresetSkillId = selectedPresetId;
    _hasLoadedBattlePreset = true;
    _battlePresetDirty = false;
}

void BattleCameraPreview_View::Load_BattlePresetForCurrentMode(bool loadBase)
{
    Load_BattlePresetForMode(_previewContext.mode, loadBase);
}

void BattleCameraPreview_View::Load_BattleDriftForMode(BattleCameraMode mode)
{
    _editingBattleDrift = BattleCameraDriftProfile{};
    const bool useBossEnemyAttackDrift = mode == BattleCameraMode::ENEMYATTACK && _editingEnemyAttackBossDrift;
    Client::CameraPresetIO::LoadBattleDriftFromFile(mode, _editingBattleDrift, useBossEnemyAttackDrift);
    _loadedBattleDriftMode = mode;
    _loadedBattleDriftBossVariant = useBossEnemyAttackDrift;
    _hasLoadedBattleDrift = true;
    _battleDriftDirty = false;
}

void BattleCameraPreview_View::Load_BattleDriftForCurrentMode()
{
    Load_BattleDriftForMode(_previewContext.mode);
}

void BattleCameraPreview_View::Load_BattleClipForMode(BattleCameraMode mode, bool loadBase, bool resetTimeline)
{
    const float preservedTimeSec = _previewContext.timeSec;
    const string selectedClipId = Get_CurrentBattleClipAuthoringOverrideId();
    _editingBattleClip = BattleCameraClip{ 1.f };
    _editingBattleClip.timeMode = Get_DefaultBattleCameraClipTimeMode(mode);
    const bool shouldLoadBaseClip = loadBase || selectedClipId.empty();
    if (shouldLoadBaseClip)
        Client::BattleCameraClipIO::LoadBattleCameraClipFromFile(mode, _editingBattleClip);

    if (!selectedClipId.empty())
        Client::BattleCameraClipIO::LoadBattleCameraClipFromFile(mode, _editingBattleClip, selectedClipId);

    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
        _editingBattleClip.duration = 1.f;

    _loadedBattleClipMode = mode;
    _loadedBattleClipSkillId = selectedClipId;
    _hasLoadedBattleClip = true;
    _battleClipDirty = false;
    _battleClipSelectedKeyIndex = -1;

    if (resetTimeline)
    {
        _battleClipCurrentFrame = 0;
        Sync_PreviewTimeFromTimeline();
        return;
    }

    _previewContext.timeSec = std::max(0.f, preservedTimeSec);
    Sync_BattleClipFrameFromPreviewTime();
}

void BattleCameraPreview_View::Load_BattleClipForCurrentMode(bool loadBase, bool resetTimeline)
{
    Load_BattleClipForMode(_previewContext.mode, loadBase, resetTimeline);
}

void BattleCameraPreview_View::Refresh_AuthoringFileList()
{
    _authoringFilePaths.clear();

    const fs::path presetFolder = fs::absolute(fs::path(kBattleCameraPresetFolder)).lexically_normal();
    if (!fs::exists(presetFolder))
        return;

    for (const auto& entry : fs::directory_iterator(presetFolder))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".json")
            continue;

        if (!Try_ParseBattleAuthoringFileName(entry.path()).has_value())
            continue;

        _authoringFilePaths.push_back(entry.path());
    }

    std::sort(_authoringFilePaths.begin(), _authoringFilePaths.end());
}

void BattleCameraPreview_View::Load_SelectedAuthoringFile()
{
    if (_selectedAuthoringFileIndex < 0 ||
        _selectedAuthoringFileIndex >= static_cast<int>(_authoringFilePaths.size()))
    {
        return;
    }

    Apply_AuthoringFileSelection(_authoringFilePaths[_selectedAuthoringFileIndex]);
}

bool BattleCameraPreview_View::Apply_AuthoringFileSelection(const fs::path& filePath)
{
    const auto parsed = Try_ParseBattleAuthoringFileName(filePath);
    if (!parsed.has_value())
        return false;

    Apply_AuthoringFileContext(parsed->mode, parsed->kind == BattleAuthoringFileKind::Preset, parsed->skillId);
    Try_AutoSelectPreviewActorAndAnimation();
    Load_BattlePresetForCurrentMode();
    Load_BattleDriftForCurrentMode();
    Load_BattleClipForCurrentMode();

    if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
    {
        battleTurnCamera->Set_Preset(_previewContext.mode, _editingBattlePreset);
        battleTurnCamera->Set_DriftProfile(_previewContext.mode, _editingBattleDrift, _previewContext.mode == BattleCameraMode::ENEMYATTACK && _editingEnemyAttackBossDrift);
        battleTurnCamera->Set_Clip(_previewContext.mode, _editingBattleClip, Get_CurrentBattleClipAuthoringOverrideId());
    }

    Apply_PreviewContext();
    _previewDirty = false;
    return true;
}

void BattleCameraPreview_View::Apply_AuthoringFileContext(BattleCameraMode mode, bool isPresetFile, const string& authoringId)
{
    const string previousSkillId = Get_SelectedSkillId();
    _previewContext.mode = mode;
    _selectedSkillIndex = -1;

    auto iter = std::find(_skillIds.begin(), _skillIds.end(), authoringId);
    const bool isKnownSkillId = (iter != _skillIds.end());

    if (isKnownSkillId)
    {
        _previewContext.skillId = authoringId;
        _useSkillOverride = true;
        _selectedSkillIndex = static_cast<int>(std::distance(_skillIds.begin(), iter));
        _useManualBattleAuthoringTarget = false;
        _manualBattleAuthoringPresetId.clear();
        _manualBattleAuthoringClipId.clear();
    }
    else
    {
        if (authoringId.empty())
        {
            _previewContext.skillId.clear();
            _useSkillOverride = false;
            _useManualBattleAuthoringTarget = false;
            _manualBattleAuthoringPresetId.clear();
            _manualBattleAuthoringClipId.clear();
        }
        else
        {
            _previewContext.skillId = previousSkillId;
            _useSkillOverride = !previousSkillId.empty();
            if (_useSkillOverride)
            {
                auto previousIter = std::find(_skillIds.begin(), _skillIds.end(), previousSkillId);
                if (previousIter != _skillIds.end())
                    _selectedSkillIndex = static_cast<int>(std::distance(_skillIds.begin(), previousIter));
            }

            _useManualBattleAuthoringTarget = true;
            if (isPresetFile)
                _manualBattleAuthoringPresetId = authoringId;
            else
                _manualBattleAuthoringClipId = authoringId;
        }
    }

    _hasLoadedBattlePreset = false;
    _hasLoadedBattleClip = false;
    _selectedPreviewSkillPhaseIndex = -1;
    Reset_PreviewPoseState();
}

void BattleCameraPreview_View::Update_BattlePresetAuthoringInput(float timeDelta)
{
    if (_previewCamera == nullptr || !_isPreviewHovered)
        return;

    Ensure_BattlePresetLoaded();

    bool changed = false;
    ImGuiIO& io = ImGui::GetIO();

    if (io.MouseWheel != 0.f)
    {
        const float wheelStep = 0.4f;
        _editingBattlePreset.distance += (io.MouseWheel > 0.f) ? -wheelStep : wheelStep;
        changed = true;
    }

    const float moveStep = 3.f * timeDelta;

    if (ImGui::IsKeyDown(ImGuiKey_W))
    {
        _editingBattlePreset.distance -= moveStep;
        changed = true;
    }

    if (ImGui::IsKeyDown(ImGuiKey_S))
    {
        _editingBattlePreset.distance += moveStep;
        changed = true;
    }

    if (ImGui::IsKeyDown(ImGuiKey_A))
    {
        _editingBattlePreset.aimOffset -= moveStep;
        changed = true;
    }

    if (ImGui::IsKeyDown(ImGuiKey_D))
    {
        _editingBattlePreset.aimOffset += moveStep;
        changed = true;
    }

    if (ImGui::IsKeyDown(ImGuiKey_Q))
    {
        _editingBattlePreset.pivotHeight -= moveStep;
        changed = true;
    }

    if (ImGui::IsKeyDown(ImGuiKey_E))
    {
        _editingBattlePreset.pivotHeight += moveStep;
        changed = true;
    }

    const bool isRightMouseDragging = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool isCtrlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);

    // Preview authoring uses target-relative preset values, not free-camera world transforms.
    // Keep the shortcut mapping here explicit so designers can retune the controls without
    // hunting through the pose-calculation code.
    if (isRightMouseDragging)
    {
        if (isCtrlDown)
        {
            _editingBattlePreset.lookHeight += io.MouseDelta.y * _editingBattlePreset.camera.mouseSensor;
            _editingBattlePreset.rollOffset += io.MouseDelta.x * _editingBattlePreset.camera.mouseSensor * _editingBattlePreset.camera.mouseSensitivity;
        }
        else
        {
            _editingBattlePreset.yawOffset += io.MouseDelta.x * _editingBattlePreset.camera.mouseSensor;
            _editingBattlePreset.pitchOffset += io.MouseDelta.y * _editingBattlePreset.camera.mouseSensor * _editingBattlePreset.camera.mouseSensitivity;
        }

        changed = true;
    }

    if (!changed)
        return;

    _editingBattlePreset.distance = std::max(0.1f, _editingBattlePreset.distance);
    _editingBattlePreset.pitchOffset = std::clamp(
        _editingBattlePreset.pitchOffset,
        _editingBattlePreset.pitchMin,
        _editingBattlePreset.pitchMax);

    if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
        battleTurnCamera->Set_Preset(_previewContext.mode, _editingBattlePreset);

    _previewDirty = true;
}

void BattleCameraPreview_View::Refresh_PreviewAnimationList()
{
    if (!_animationListDirty)
        return;

    _previewAnimationNames.clear();
    _previewSkillPhaseBlocksDirty = true;
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

void BattleCameraPreview_View::Try_AutoSelectPreviewActorAndAnimation()
{
    const string selectedSkillId = Get_SelectedSkillId();
    if (selectedSkillId.empty())
        return;

    const Client::Skill_Data* skill = CLIENT->Get_Skill(selectedSkillId);
    if (skill == nullptr)
        return;

    auto equalsIgnoreCase = [](const string& lhs, const char* rhs)
    {
        string normalizedLhs = lhs;
        string normalizedRhs = rhs ? string(rhs) : string{};
        std::transform(normalizedLhs.begin(), normalizedLhs.end(), normalizedLhs.begin(), ::tolower);
        std::transform(normalizedRhs.begin(), normalizedRhs.end(), normalizedRhs.begin(), ::tolower);
        return normalizedLhs == normalizedRhs;
    };

    Client::HeroId resolvedHeroId = Client::HeroId::END;
    if (equalsIgnoreCase(skill->characterId, "gustave"))
        resolvedHeroId = Client::HeroId::Gustave;
    else if (equalsIgnoreCase(skill->characterId, "lune"))
        resolvedHeroId = Client::HeroId::Lune;
    else if (equalsIgnoreCase(skill->characterId, "maelle"))
        resolvedHeroId = Client::HeroId::Maelle;

    if (resolvedHeroId != Client::HeroId::END && resolvedHeroId != _selectedHeroId)
    {
        _selectedHeroId = resolvedHeroId;
        _previewObjectsDirty = true;
        _animationListDirty = true;
        Rebuild_PreviewObjects();
        Refresh_PreviewAnimationList();
    }

    string targetAnimationName{};
    if ((_previewContext.mode == BattleCameraMode::PLAYERATTACK ||
         _previewContext.mode == BattleCameraMode::ENEMYATTACK) &&
        !skill->AnimPhases.empty())
    {
        targetAnimationName = skill->AnimPhases.front().animationKey;
    }
   /* else
    {
        targetAnimationName = skill->IdleAnimationKey;
    }*/

    if (targetAnimationName.empty() || _previewAnimationNames.empty())
        return;

    auto iter = std::find(_previewAnimationNames.begin(), _previewAnimationNames.end(), targetAnimationName);
    if (iter == _previewAnimationNames.end())
        return;

    _selectedPreviewAnimationIndex = static_cast<int>(std::distance(_previewAnimationNames.begin(), iter));
}

void BattleCameraPreview_View::Refresh_PreviewSkillPhaseBlocks()
{
    if (!_previewSkillPhaseBlocksDirty)
        return;

    const int previousSelectedPhaseIndex = _selectedPreviewSkillPhaseIndex;
    _previewSkillPhaseBlocks.clear();

    const string selectedSkillId = Get_SelectedSkillId();
    if (selectedSkillId.empty())
    {
        _previewSkillPhaseBlocksDirty = false;
        return;
    }

    const Client::Skill_Data* skill = CLIENT->Get_Skill(selectedSkillId);
    if (skill == nullptr)
    {
        _previewSkillPhaseBlocksDirty = false;
        return;
    }

    float accumulatedTimeSec = 0.f;
    _previewSkillPhaseBlocks.reserve(skill->AnimPhases.size());

    for (const Client::BattleActionPhase& phase : skill->AnimPhases)
    {
        PreviewSkillPhaseBlock block{};
        block.phaseIndex = phase.phaseIndex;
        block.animationKey = phase.animationKey;
        block.startTimeSec = accumulatedTimeSec;
        block.durationSec = std::max(0.01f, Resolve_PreviewAnimationDurationByName(phase.animationKey));
        accumulatedTimeSec += block.durationSec;
        _previewSkillPhaseBlocks.push_back(block);
    }

    if (_previewSkillPhaseBlocks.empty())
        _selectedPreviewSkillPhaseIndex = -1;
    else if (previousSelectedPhaseIndex >= 0 &&
        previousSelectedPhaseIndex < static_cast<int>(_previewSkillPhaseBlocks.size()))
    {
        _selectedPreviewSkillPhaseIndex = previousSelectedPhaseIndex;
    }
    else
    {
        const PreviewSkillPhaseBlock* activePhase = Resolve_ActivePreviewSkillPhase(_previewContext.timeSec);
        if (activePhase != nullptr)
        {
            _selectedPreviewSkillPhaseIndex = static_cast<int>(activePhase - _previewSkillPhaseBlocks.data());
        }
        else
        {
            _selectedPreviewSkillPhaseIndex = 0;
        }
    }

    _previewSkillPhaseBlocksDirty = false;
}

float BattleCameraPreview_View::Resolve_PreviewAnimationDurationByName(const string& animationName) const
{
    if (animationName.empty())
        return 0.f;

    Shared<ModelCom> model = Resolve_PreviewSubjectModel();
    if (model == nullptr)
        return 0.f;

    const char* previousAnimationName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
    const string previousAnimation = previousAnimationName ? string(previousAnimationName) : string{};
    const float previousTimeSec = model->Get_CurrentAnimationTimeSec();

    model->Set_Animation(animationName, false, 0.f);
    const float durationSec = std::max(0.f, model->Get_CurrentAnimationDurationSec());

    if (!previousAnimation.empty())
    {
        model->Set_Animation(previousAnimation, false, 0.f);
        model->Set_CurrentAnimationTimeSec(previousTimeSec);
        model->Refresh_CurrentAnimationPose();
    }

    return durationSec;
}

float BattleCameraPreview_View::Get_PreviewSkillSequenceDuration() const
{
    if (_previewSkillPhaseBlocks.empty())
        return 0.f;

    const PreviewSkillPhaseBlock& lastBlock = _previewSkillPhaseBlocks.back();
    return lastBlock.startTimeSec + lastBlock.durationSec;
}

BattleCameraPreview_View::PreviewSkillPhaseBlock* BattleCameraPreview_View::Get_SelectedPreviewSkillPhase()
{
    if (_selectedPreviewSkillPhaseIndex < 0 ||
        _selectedPreviewSkillPhaseIndex >= static_cast<int>(_previewSkillPhaseBlocks.size()))
    {
        return nullptr;
    }

    return &_previewSkillPhaseBlocks[_selectedPreviewSkillPhaseIndex];
}

const BattleCameraPreview_View::PreviewSkillPhaseBlock* BattleCameraPreview_View::Get_SelectedPreviewSkillPhase() const
{
    if (_selectedPreviewSkillPhaseIndex < 0 ||
        _selectedPreviewSkillPhaseIndex >= static_cast<int>(_previewSkillPhaseBlocks.size()))
    {
        return nullptr;
    }

    return &_previewSkillPhaseBlocks[_selectedPreviewSkillPhaseIndex];
}

const BattleCameraPreview_View::PreviewSkillPhaseBlock* BattleCameraPreview_View::Resolve_CurrentPreviewSkillPhase(float previewTimeSec) const
{
    if (const PreviewSkillPhaseBlock* selectedPhase = Get_SelectedPreviewSkillPhase())
        return selectedPhase;

    return Resolve_ActivePreviewSkillPhase(previewTimeSec);
}

const BattleCameraPreview_View::PreviewSkillPhaseBlock* BattleCameraPreview_View::Resolve_ActivePreviewSkillPhase(float previewTimeSec) const
{
    if (_previewSkillPhaseBlocks.empty())
        return nullptr;

    for (const PreviewSkillPhaseBlock& block : _previewSkillPhaseBlocks)
    {
        const float blockEndTime = block.startTimeSec + block.durationSec;
        if (previewTimeSec >= block.startTimeSec && previewTimeSec <= blockEndTime)
            return &block;
    }

    if (previewTimeSec < _previewSkillPhaseBlocks.front().startTimeSec)
        return &_previewSkillPhaseBlocks.front();

    return &_previewSkillPhaseBlocks.back();
}

BattleCameraPreview_View::PreviewCameraShotBlock* BattleCameraPreview_View::Get_SelectedPreviewShotBlock()
{
    if (_selectedPreviewShotBlockIndex < 0 ||
        _selectedPreviewShotBlockIndex >= static_cast<int>(_previewShotBlocks.size()))
    {
        return nullptr;
    }

    return &_previewShotBlocks[_selectedPreviewShotBlockIndex];
}

const BattleCameraPreview_View::PreviewCameraShotBlock* BattleCameraPreview_View::Get_SelectedPreviewShotBlock() const
{
    if (_selectedPreviewShotBlockIndex < 0 ||
        _selectedPreviewShotBlockIndex >= static_cast<int>(_previewShotBlocks.size()))
    {
        return nullptr;
    }

    return &_previewShotBlocks[_selectedPreviewShotBlockIndex];
}

int BattleCameraPreview_View::Find_ActivePreviewShotBlockIndex(float previewTimeSec) const
{
    for (int i = 0; i < static_cast<int>(_previewShotBlocks.size()); ++i)
    {
        const PreviewCameraShotBlock& block = _previewShotBlocks[i];
        if (previewTimeSec >= block.startTimeSec && previewTimeSec <= block.endTimeSec)
            return i;
    }

    return -1;
}

const BattleCameraPreview_View::PreviewCameraShotBlock* BattleCameraPreview_View::Resolve_ActivePreviewShotBlock(float previewTimeSec) const
{
    const int activeBlockIndex = Find_ActivePreviewShotBlockIndex(previewTimeSec);
    if (activeBlockIndex < 0)
        return nullptr;

    return &_previewShotBlocks[activeBlockIndex];
}

void BattleCameraPreview_View::Apply_ActivePreviewShotBlock(float previewTimeSec)
{
    const PreviewCameraShotBlock* activeBlock = Resolve_ActivePreviewShotBlock(previewTimeSec);
    if (activeBlock == nullptr)
        return;

    const bool modeChanged = _previewContext.mode != activeBlock->mode;
    const bool presetChanged = _activePreviewShotPresetId != activeBlock->presetId;
    const bool clipChanged = _activePreviewShotClipId != activeBlock->clipId;

    _previewContext.mode = activeBlock->mode;
    _activePreviewShotMode = activeBlock->mode;
    _activePreviewShotPresetId = activeBlock->presetId;
    _activePreviewShotClipId = activeBlock->clipId;

    if (modeChanged)
        Apply_DefaultPreviewSlotsForMode(_previewContext.mode);

    if (modeChanged || presetChanged)
        _hasLoadedBattlePreset = false;

    if (modeChanged || clipChanged)
        _hasLoadedBattleClip = false;
}

bool BattleCameraPreview_View::Does_BattleAuthoringTargetMatchPreview() const
{
    return true;
}

void BattleCameraPreview_View::Sync_BattleAuthoringTargetToPreview()
{
    const int activeBlockIndex = Find_ActivePreviewShotBlockIndex(_previewContext.timeSec);
    _useManualBattleAuthoringTarget = false;

    if (activeBlockIndex >= 0)
    {
        Select_PreviewShotBlock(activeBlockIndex, false);
    }
    else
    {
        _hasLoadedBattlePreset = false;
        _hasLoadedBattleClip = false;
    }

    if (activeBlockIndex < 0)
        _selectedPreviewShotBlockIndex = -1;

    _hasLoadedBattlePreset = false;
    _hasLoadedBattleClip = false;
}

void BattleCameraPreview_View::RetargetLoadedBattleAuthoringDataToCurrentTarget()
{
    const BattleCameraMode authoringMode = Get_CurrentBattleAuthoringMode();
    const string authoringPresetId = Get_CurrentBattlePresetAuthoringOverrideId();
    const string authoringClipId = Get_CurrentBattleClipAuthoringOverrideId();

    if (_hasLoadedBattlePreset)
    {
        _loadedBattlePresetMode = authoringMode;
        _loadedBattlePresetSkillId = authoringPresetId;
    }

    if (_hasLoadedBattleClip)
    {
        _loadedBattleClipMode = authoringMode;
        _loadedBattleClipSkillId = authoringClipId;
    }
}

void BattleCameraPreview_View::Toggle_BattleClipPlayback()
{
    _timelinePlaying = !_timelinePlaying;
}

void BattleCameraPreview_View::Handle_BattleClipPlaybackShortcut()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (!_isFocused || io.WantTextInput || io.KeyCtrl || io.KeyAlt)
        return;

    if (_activeTimelineEditorTab != TimelineEditorTab::ClipTimeline)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
        Toggle_BattleClipPlayback();
}

string BattleCameraPreview_View::Get_CurrentBattlePresetAuthoringOverrideId() const
{
    if (_useManualBattleAuthoringTarget && !_manualBattleAuthoringPresetId.empty())
        return _manualBattleAuthoringPresetId;

    if (!_activePreviewShotPresetId.empty())
        return _activePreviewShotPresetId;

    return Get_SelectedSkillId();
}

string BattleCameraPreview_View::Get_CurrentBattleClipAuthoringOverrideId() const
{
    if (_useManualBattleAuthoringTarget && !_manualBattleAuthoringClipId.empty())
        return _manualBattleAuthoringClipId;

    if (!_activePreviewShotClipId.empty())
        return _activePreviewShotClipId;

    return Get_SelectedSkillId();
}

void BattleCameraPreview_View::Add_PreviewShotBlockFromSelectedPhase()
{
    Refresh_PreviewSkillPhaseBlocks();
    const PreviewSkillPhaseBlock* selectedPhase = Resolve_CurrentPreviewSkillPhase(_previewContext.timeSec);
    if (selectedPhase == nullptr)
        return;

    PreviewCameraShotBlock block{};
    block.startTimeSec = selectedPhase->startTimeSec;
    block.endTimeSec = selectedPhase->startTimeSec + selectedPhase->durationSec;
    block.mode = _previewContext.mode;
    _previewShotBlocks.push_back(block);
    std::sort(_previewShotBlocks.begin(), _previewShotBlocks.end(),
        [](const PreviewCameraShotBlock& lhs, const PreviewCameraShotBlock& rhs)
        {
            return lhs.startTimeSec < rhs.startTimeSec;
        });
    for (int i = 0; i < static_cast<int>(_previewShotBlocks.size()); ++i)
    {
        if (fabsf(_previewShotBlocks[i].startTimeSec - block.startTimeSec) <= 0.0001f &&
            fabsf(_previewShotBlocks[i].endTimeSec - block.endTimeSec) <= 0.0001f &&
            _previewShotBlocks[i].mode == block.mode)
        {
            Select_PreviewShotBlock(i, true);
            break;
        }
    }
}

void BattleCameraPreview_View::Add_PreviewShotBlockAtCurrentTime()
{
    PreviewCameraShotBlock block{};
    block.startTimeSec = _previewContext.timeSec;
    block.endTimeSec = _previewContext.timeSec + 0.75f;
    block.mode = _previewContext.mode;
    _previewShotBlocks.push_back(block);
    std::sort(_previewShotBlocks.begin(), _previewShotBlocks.end(),
        [](const PreviewCameraShotBlock& lhs, const PreviewCameraShotBlock& rhs)
        {
            return lhs.startTimeSec < rhs.startTimeSec;
        });
    for (int i = 0; i < static_cast<int>(_previewShotBlocks.size()); ++i)
    {
        if (fabsf(_previewShotBlocks[i].startTimeSec - block.startTimeSec) <= 0.0001f &&
            fabsf(_previewShotBlocks[i].endTimeSec - block.endTimeSec) <= 0.0001f &&
            _previewShotBlocks[i].mode == block.mode)
        {
            Select_PreviewShotBlock(i, true);
            break;
        }
    }
}

void BattleCameraPreview_View::Remove_SelectedPreviewShotBlock()
{
    if (_selectedPreviewShotBlockIndex < 0 ||
        _selectedPreviewShotBlockIndex >= static_cast<int>(_previewShotBlocks.size()))
    {
        return;
    }

    _previewShotBlocks.erase(_previewShotBlocks.begin() + _selectedPreviewShotBlockIndex);
    _selectedPreviewShotBlockIndex = std::clamp(_selectedPreviewShotBlockIndex, 0, static_cast<int>(_previewShotBlocks.size()) - 1);
}

void BattleCameraPreview_View::Duplicate_SelectedPreviewShotBlock()
{
    const PreviewCameraShotBlock* selectedBlock = Get_SelectedPreviewShotBlock();
    if (selectedBlock == nullptr)
        return;

    PreviewCameraShotBlock duplicated = *selectedBlock;
    const float durationSec = std::max(0.01f, duplicated.endTimeSec - duplicated.startTimeSec);
    duplicated.startTimeSec = duplicated.endTimeSec;
    duplicated.endTimeSec = duplicated.startTimeSec + durationSec;
    _previewShotBlocks.push_back(duplicated);
    std::sort(_previewShotBlocks.begin(), _previewShotBlocks.end(),
        [](const PreviewCameraShotBlock& lhs, const PreviewCameraShotBlock& rhs)
        {
            return lhs.startTimeSec < rhs.startTimeSec;
        });

    for (int i = 0; i < static_cast<int>(_previewShotBlocks.size()); ++i)
    {
        if (fabsf(_previewShotBlocks[i].startTimeSec - duplicated.startTimeSec) <= 0.0001f &&
            fabsf(_previewShotBlocks[i].endTimeSec - duplicated.endTimeSec) <= 0.0001f &&
            _previewShotBlocks[i].presetId == duplicated.presetId &&
            _previewShotBlocks[i].clipId == duplicated.clipId &&
            _previewShotBlocks[i].mode == duplicated.mode)
        {
            Select_PreviewShotBlock(i, true);
            break;
        }
    }
}

void BattleCameraPreview_View::Fit_SelectedPreviewShotBlockToActivePhase()
{
    PreviewCameraShotBlock* selectedBlock = Get_SelectedPreviewShotBlock();
    const PreviewSkillPhaseBlock* selectedPhase = Resolve_CurrentPreviewSkillPhase(_previewContext.timeSec);
    if (selectedBlock == nullptr || selectedPhase == nullptr)
        return;

    selectedBlock->startTimeSec = selectedPhase->startTimeSec;
    selectedBlock->endTimeSec = selectedPhase->startTimeSec + selectedPhase->durationSec;
    _previewDirty = true;
}

void BattleCameraPreview_View::Select_PreviewShotBlock(int blockIndex, bool syncPreviewTime)
{
    if (blockIndex < 0 || blockIndex >= static_cast<int>(_previewShotBlocks.size()))
        return;

    _selectedPreviewShotBlockIndex = blockIndex;
    const PreviewCameraShotBlock& block = _previewShotBlocks[blockIndex];
    _previewContext.mode = block.mode;
    _activePreviewShotMode = block.mode;
    _activePreviewShotPresetId = block.presetId;
    _activePreviewShotClipId = block.clipId;
    _hasLoadedBattlePreset = false;
    _hasLoadedBattleClip = false;

    if (syncPreviewTime)
    {
        _previewContext.timeSec = block.startTimeSec;
        _battleClipCurrentFrame = 0;
    }

    _previewDirty = true;
}

float BattleCameraPreview_View::Snap_PreviewShotTime(float timeSec, float timelineDurationSec) const
{
    constexpr float kSnapThresholdSec = 0.08f;

    float snappedTimeSec = std::clamp(timeSec, 0.f, std::max(0.f, timelineDurationSec));
    auto trySnap = [&](float candidateTimeSec)
    {
        if (fabsf(snappedTimeSec - candidateTimeSec) <= kSnapThresholdSec)
            snappedTimeSec = candidateTimeSec;
    };

    trySnap(0.f);
    trySnap(_previewContext.timeSec);

    for (const PreviewSkillPhaseBlock& phase : _previewSkillPhaseBlocks)
    {
        trySnap(phase.startTimeSec);
        trySnap(phase.startTimeSec + phase.durationSec);
    }

    return snappedTimeSec;
}

void BattleCameraPreview_View::Sort_BattleClipTrackKeyframes(CameraTrack& track)
{
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const CameraKeyframe& lhs, const CameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });
}

void BattleCameraPreview_View::Insert_BattleClipKeyAtFrame(int trackIndex, int frame)
{
    CameraTrack* track = nullptr;

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
    case 3:
        track = &_editingBattleClip.sideOffset;
        break;
    case 4:
        track = &_editingBattleClip.pivotOffset;
        break;
    case 5:
        track = &_editingBattleClip.heightOffset;
        break;
    case 6:
        track = &_editingBattleClip.aimOffset;
        break;
    case 7:
        track = &_editingBattleClip.lookHeightOffset;
        break;
    case 8:
        track = &_editingBattleClip.yawOffset;
        break;
    case 9:
        track = &_editingBattleClip.orbitYaw;
        break;
    case 10:
        track = &_editingBattleClip.orbitRadius;
        break;
    case 11:
        track = &_editingBattleClip.orbitHeight;
        break;
    case 12:
        track = &_editingBattleClip.orbitLookHeight;
        break;
    case 13:
        track = &_editingBattleClip.shakeFrequencyX;
        break;
    case 14:
        track = &_editingBattleClip.shakeFrequencyY;
        break;
    case 15:
        track = &_editingBattleClip.shakeDecayRateX;
        break;
    case 16:
        track = &_editingBattleClip.shakeDecayRate;
        break;
    case 17:
        track = &_editingBattleClip.shakeVerticalRatio;
        break;
    case 18:
        track = &_editingBattleClip.shakeStrengthScale;
        break;
    default:
        return;
    }

    const float keyTime = std::clamp(Get_BattleClipTimeFromFrame(frame), 0.f, Get_BattleClipAuthoringDurationSec());
    if (_editingBattleClip.timeMode != BattleCameraClipTimeMode::AnimationSync && keyTime > _editingBattleClip.duration)
        _editingBattleClip.duration = keyTime;

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

    CameraKeyframe keyframe{};
    keyframe.time = keyTime;
    keyframe.value = Get_DefaultBattleClipValue(trackIndex);
    track->keyframes.push_back(keyframe);
    Sort_BattleClipTrackKeyframes(*track);
    _battleClipDirty = true;

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
    _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipPreviewFrameMax());
    const float clipTimelineTime = Get_BattleClipTimeFromFrame(_battleClipCurrentFrame);
    if (_editingBattleClip.timeMode == BattleCameraClipTimeMode::AnimationSync)
        _previewContext.timeSec = Get_CurrentBattleClipTimeOriginSec() + clipTimelineTime * Get_CurrentBattleClipTimeRangeSec();
    else
        _previewContext.timeSec = Get_CurrentBattleClipTimeOriginSec() + clipTimelineTime;
}

void BattleCameraPreview_View::Refresh_SelectedBattleClipKey()
{
    CameraTrack* track = Get_SelectedBattleClipTrack();
    if (track == nullptr)
    {
        _battleClipSelectedKeyIndex = -1;
        return;
    }

    if (_battleClipSelectedKeyIndex >= 0 &&
        _battleClipSelectedKeyIndex < static_cast<int>(track->keyframes.size()))
    {
        return;
    }

    if (track->keyframes.empty())
    {
        _battleClipSelectedKeyIndex = -1;
        return;
    }

    const float currentTime = Get_BattleClipTimeFromFrame(_battleClipCurrentFrame);
    int nearestKeyIndex = 0;
    float nearestTimeDelta = FLT_MAX;

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        const float timeDelta = fabsf(track->keyframes[i].time - currentTime);
        if (timeDelta < nearestTimeDelta)
        {
            nearestTimeDelta = timeDelta;
            nearestKeyIndex = i;
        }
    }

    _battleClipSelectedKeyIndex = nearestKeyIndex;
}

void BattleCameraPreview_View::Reset_PreviewPoseState()
{
    _previewSmoothedTargetPos = Vec3::Zero;
    _previewHasSmoothedTarget = false;
    _previewPoseMode = _previewContext.mode;
    _previewPoseSubjectSide = _previewContext.subjectSide;
    _previewPoseSubjectSlot = _previewContext.subjectSlot;
    _previewPoseLastTimeSec = 0.f;
}

void BattleCameraPreview_View::Apply_DefaultPreviewSlotsForMode(BattleCameraMode mode, bool resetPoseState)
{
    switch (mode)
    {
    case BattleCameraMode::BATTLEINTRO:
        _previewContext.subjectSide = PreviewSlotSide::Hero;
        _previewContext.subjectSlot = 1;
        _previewContext.opponentSide = PreviewSlotSide::Enemy;
        _previewContext.opponentSlot = 1;
        break;

    case BattleCameraMode::ENEMYFOCUS:
    case BattleCameraMode::ENEMYATTACK:
        _previewContext.subjectSide = PreviewSlotSide::Enemy;
        _previewContext.subjectSlot = 1;
        _previewContext.opponentSide = PreviewSlotSide::Hero;
        _previewContext.opponentSlot = 1;
        break;

    case BattleCameraMode::PLAYERFOCUS:
    case BattleCameraMode::PLAYERATTACK:
    case BattleCameraMode::PARRY:
        _previewContext.subjectSide = PreviewSlotSide::Hero;
        _previewContext.subjectSlot = 1;
        _previewContext.opponentSide = PreviewSlotSide::Enemy;
        _previewContext.opponentSlot = 1;
        break;

    case BattleCameraMode::WIDE:
    default:
        _previewContext.subjectSide = PreviewSlotSide::Hero;
        _previewContext.subjectSlot = 1;
        _previewContext.opponentSide = PreviewSlotSide::Enemy;
        _previewContext.opponentSlot = 1;
        break;
    }

    if (resetPoseState)
        Reset_PreviewPoseState();
}

void BattleCameraPreview_View::Apply_PreviewTransitionMode(BattleCameraMode mode)
{
    const Vec3 previousSmoothedTargetPos = _previewSmoothedTargetPos;
    const bool hadSmoothedTarget = _previewHasSmoothedTarget;

    _previewContext.mode = mode;
    Apply_DefaultPreviewSlotsForMode(mode, false);
    _hasLoadedBattlePreset = false;
    _hasLoadedBattleClip = false;
    Load_BattlePresetForCurrentMode();
    Load_BattleClipForCurrentMode();

    _previewContext.timeSec = 0.f;
    _battleClipCurrentFrame = 0;
    Sync_PreviewTimeFromTimeline();

    _previewPoseMode = _previewContext.mode;
    _previewPoseSubjectSide = _previewContext.subjectSide;
    _previewPoseSubjectSlot = _previewContext.subjectSlot;
    _previewPoseLastTimeSec = 0.f;

    if (_editingBattlePreset.snapOnEnter || !hadSmoothedTarget)
    {
        const Vec3 subjectPos = Get_PreviewSlotWorldPosition(_previewContext.subjectSide, _previewContext.subjectSlot);
        _previewSmoothedTargetPos = subjectPos;
        _previewHasSmoothedTarget = true;
    }
    else
    {
        _previewSmoothedTargetPos = previousSmoothedTargetPos;
        _previewHasSmoothedTarget = true;
    }

    _previewEnabled = true;
    _previewDirty = true;
}

void BattleCameraPreview_View::Apply_PreviewSkillContext(const string& skillId)
{
    _previewContext.skillId = skillId;
    _useSkillOverride = !skillId.empty();
    _previewSkillPhaseBlocksDirty = true;
    _previewShotBlocks.clear();
    _selectedPreviewShotBlockIndex = -1;
    _selectedPreviewSkillPhaseIndex = -1;
    _selectedSkillIndex = -1;

    if (_useSkillOverride)
    {
        auto iter = std::find(_skillIds.begin(), _skillIds.end(), skillId);
        if (iter != _skillIds.end())
            _selectedSkillIndex = static_cast<int>(std::distance(_skillIds.begin(), iter));
    }

    _hasLoadedBattlePreset = false;
    _hasLoadedBattleClip = false;
    Try_AutoSelectPreviewActorAndAnimation();
}

bool BattleCameraPreview_View::Has_UnsavedBattleAuthoringChanges() const
{
    return _battlePresetDirty || _battleDriftDirty || _battleClipDirty;
}

void BattleCameraPreview_View::Request_SkillOverrideChange(bool useSkillOverride, int selectedSkillIndex)
{
    const bool currentUseSkillOverride = !_previewContext.skillId.empty();
    const bool skillSelectionChanged =
        useSkillOverride &&
        currentUseSkillOverride &&
        selectedSkillIndex != _selectedSkillIndex;

    if (useSkillOverride == currentUseSkillOverride && !skillSelectionChanged)
        return;

    if (Has_UnsavedBattleAuthoringChanges())
    {
        _pendingUseSkillOverride = useSkillOverride;
        _pendingSelectedSkillIndex = selectedSkillIndex;
        _useSkillOverride = currentUseSkillOverride;
        _requestOpenUnsavedSkillChangePopup = true;
        return;
    }

    Apply_SkillOverrideChange(useSkillOverride, selectedSkillIndex);
}

void BattleCameraPreview_View::Apply_SkillOverrideChange(bool useSkillOverride, int selectedSkillIndex)
{
    _useSkillOverride = useSkillOverride;

    if (!_useSkillOverride)
    {
        _previewContext.skillId.clear();
        _selectedSkillIndex = -1;
        _previewShotBlocks.clear();
        _selectedPreviewShotBlockIndex = -1;
    }
    else if (selectedSkillIndex >= 0 && selectedSkillIndex < static_cast<int>(_skillIds.size()))
    {
        _selectedSkillIndex = selectedSkillIndex;
        _previewContext.skillId = _skillIds[selectedSkillIndex];
        _previewShotBlocks.clear();
        _selectedPreviewShotBlockIndex = -1;
    }

    Try_AutoSelectPreviewActorAndAnimation();
    _previewSkillPhaseBlocksDirty = true;
    _hasLoadedBattlePreset = false;
    _hasLoadedBattleClip = false;
    _previewDirty = true;
}

void BattleCameraPreview_View::Apply_TransitionStepContext(const PreviewTransitionStep& step)
{
    Apply_PreviewTransitionMode(step.mode);
    Apply_PreviewSkillContext(step.skillId);
    Load_BattlePresetForCurrentMode();
    Load_BattleClipForCurrentMode();

    if (!step.animationName.empty())
    {
        auto iter = std::find(_previewAnimationNames.begin(), _previewAnimationNames.end(), step.animationName);
        _selectedPreviewAnimationIndex = (iter != _previewAnimationNames.end())
            ? static_cast<int>(std::distance(_previewAnimationNames.begin(), iter))
            : -1;
    }
}

void BattleCameraPreview_View::Capture_CurrentPreviewIntoTransitionStep(PreviewTransitionStep& step)
{
    step.mode = _previewContext.mode;
    step.skillId = Get_SelectedSkillId();
    step.animationName.clear();

    if (_selectedPreviewAnimationIndex >= 0 &&
        _selectedPreviewAnimationIndex < static_cast<int>(_previewAnimationNames.size()))
    {
        step.animationName = _previewAnimationNames[_selectedPreviewAnimationIndex];
    }
}

void BattleCameraPreview_View::Set_TransitionPreviewLocalTime(float timeSec)
{
    _previewContext.timeSec = std::max(0.f, timeSec);
    _battleClipCurrentFrame = std::max(0, static_cast<int>(std::lround(_previewContext.timeSec * kBattleClipPreviewFps)));
    _previewDirty = true;
}

Client::BattleTurnCameraEvaluatedPose BattleCameraPreview_View::Capture_PreviewCurrentPose() const
{
    Client::BattleTurnCameraEvaluatedPose pose{};
    if (_previewCamera == nullptr)
        return pose;

    pose.fovY = _previewCamera->Get_FovY();

    Shared<TransformCom> transform = _previewCamera->Get_Transform();
    if (transform == nullptr)
        return pose;

    const Vec3 cameraPos = transform->Get_WorldPosition();
    Vec3 forward = transform->Get_WorldForward();
    if (forward.LengthSquared() <= 0.0001f)
        forward = Vec3::Look;
    else
        forward.Normalize();

    pose.cameraPos = cameraPos;
    pose.lookTarget = cameraPos + forward * 10.f;
    pose.rollOffset = 0.f;
    return pose;
}

bool BattleCameraPreview_View::Evaluate_PreviewModePose(BattleCameraMode mode, float previewTimeSec, bool includeClip, Client::BattleTurnCameraEvaluatedPose& outPose) const
{
    BattleCameraPreset preset = Make_FallbackBattleCameraPreset();
    Client::CameraPresetIO::LoadBattlePresetFromFile(mode, preset);

    const string selectedPresetId = Get_CurrentBattlePresetAuthoringOverrideId();
    if (!selectedPresetId.empty())
        Client::CameraPresetIO::LoadBattlePresetFromFile(mode, preset, selectedPresetId);

    if (mode == BattleCameraMode::WIDE)
    {
        Vec3 heroCenter = Vec3::Zero;
        uint32 heroCount = 0;
        for (const PreviewSlot& hero : _previewFormation.heroes)
        {
            heroCenter += _previewFormation.origin + hero.offset;
            ++heroCount;
        }

        if (heroCount == 0)
            return false;

        heroCenter /= static_cast<float>(heroCount);

        const PreviewSlot* yawBaseSlot = &_previewFormation.heroes[std::min(1, static_cast<int>(_previewFormation.heroes.size()) - 1)];
        if (yawBaseSlot == nullptr)
            return false;

        return Client::Evaluate_BattleTurnWidePose(
            heroCenter,
            yawBaseSlot->rotation.y,
            preset.camera.fovy,
            0.f,
            0.f,
            0.f,
            outPose);
    }

    BattleCameraClipSample clipSample{};
    if (includeClip && Uses_RuntimeBattleClip(mode))
    {
        clipSample = Client::Sample_BattleCameraClip(_editingBattleClip, previewTimeSec);

        if (!_previewFovOffsetEnabled)
            clipSample.fovOffset = 0.f;
        if (!_previewDistanceOffsetEnabled)
            clipSample.distanceOffset = 0.f;
        if (!_previewShakeEnabled)
            clipSample.shakeAmplitude = 0.f;
        if (!_previewShakeFrequencyXEnabled)
            clipSample.shakeFrequencyX = BattleCameraClipSample{}.shakeFrequencyX;
        if (!_previewShakeFrequencyYEnabled)
            clipSample.shakeFrequencyY = BattleCameraClipSample{}.shakeFrequencyY;
        if (!_previewShakeDecayRateXEnabled)
            clipSample.shakeDecayRateX = BattleCameraClipSample{}.shakeDecayRateX;
        if (!_previewShakeDecayRateEnabled)
            clipSample.shakeDecayRate = BattleCameraClipSample{}.shakeDecayRate;
        if (!_previewShakeVerticalRatioEnabled)
            clipSample.shakeVerticalRatio = BattleCameraClipSample{}.shakeVerticalRatio;
        if (!_previewShakeStrengthScaleEnabled)
            clipSample.shakeStrengthScale = BattleCameraClipSample{}.shakeStrengthScale;
        if (!_previewYawOffsetEnabled)
            clipSample.yawOffset = 0.f;
        if (!_previewSideOffsetEnabled)
            clipSample.sideOffset = 0.f;
        if (!_previewPivotOffsetEnabled)
            clipSample.pivotOffset = 0.f;
        if (!_previewHeightOffsetEnabled)
            clipSample.heightOffset = 0.f;
        if (!_previewAimOffsetEnabled)
            clipSample.aimOffset = 0.f;
        if (!_previewLookHeightOffsetEnabled)
            clipSample.lookHeightOffset = 0.f;
        if (!_previewOrbitYawEnabled)
            clipSample.orbitYaw = 0.f;
        if (!_previewOrbitRadiusEnabled)
            clipSample.orbitRadius = 0.f;
        if (!_previewOrbitHeightEnabled)
            clipSample.orbitHeight = 0.f;
        if (!_previewOrbitLookHeightEnabled)
            clipSample.orbitLookHeight = 0.f;
    }

    PreviewSlotSide subjectSide = PreviewSlotSide::Hero;
    int subjectSlot = 1;
    PreviewSlotSide opponentSide = PreviewSlotSide::Enemy;
    int opponentSlot = 1;

    switch (mode)
    {
    case BattleCameraMode::BATTLEINTRO:
        break;

    case BattleCameraMode::ENEMYFOCUS:
    case BattleCameraMode::ENEMYATTACK:
        subjectSide = PreviewSlotSide::Enemy;
        opponentSide = PreviewSlotSide::Hero;
        break;

    case BattleCameraMode::PLAYERFOCUS:
    case BattleCameraMode::PLAYERATTACK:
    case BattleCameraMode::PARRY:
    default:
        break;
    }

    const Vec3 subjectPos = Get_PreviewSlotWorldPosition(subjectSide, subjectSlot);
    const Vec3 opponentPos = Get_PreviewSlotWorldPosition(opponentSide, opponentSlot);

    if (mode == BattleCameraMode::ENEMYATTACK && opponentSide != subjectSide)
    {
        return Client::Evaluate_BattleTurnEnemyAttackPose(subjectPos, opponentPos, preset, clipSample, previewTimeSec, outPose);
    }

    return Client::Evaluate_BattleTurnPresetPose(subjectPos, preset, clipSample, 0.f, 0.f, nullptr, previewTimeSec, outPose);
}

void BattleCameraPreview_View::Apply_PreviewEvaluatedPose(const Client::BattleTurnCameraEvaluatedPose& pose)
{
    if (_previewCamera == nullptr)
        return;

    _previewCamera->Set_FovY(pose.fovY);
    if (Shared<TransformCom> transform = _previewCamera->Get_Transform())
    {
        transform->Set_WorldPosition(pose.cameraPos);
        transform->LookAt(pose.lookTarget);
        transform->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, pose.rollOffset));
    }
}

void BattleCameraPreview_View::Rebuild_TransitionPreviewStepAtTime(int stepIndex, float timeSec)
{
    if (stepIndex < 0 || stepIndex >= static_cast<int>(_transitionPreviewSteps.size()))
        return;

    const PreviewTransitionStep& step = _transitionPreviewSteps[stepIndex];
    const float clampedTime = std::clamp(timeSec, 0.f, std::max(0.f, step.durationSec));
    Apply_TransitionStepContext(step);
    _previewContext.mode = step.mode;
    Apply_DefaultPreviewSlotsForMode(step.mode, false);
    _previewEnabled = true;
    _previewContext.timeSec = clampedTime;
    _battleClipCurrentFrame = std::max(0, static_cast<int>(std::lround(clampedTime * kBattleClipPreviewFps)));
    Apply_PreviewAnimation(clampedTime);

    Client::BattleTurnCameraEvaluatedPose toPose{};
    if (!Evaluate_PreviewModePose(step.mode, clampedTime, false, toPose))
        return;

    Client::BattleTurnCameraEvaluatedPose fromPose = toPose;
    if (stepIndex == 0)
    {
        fromPose = Capture_PreviewCurrentPose();
        if (fromPose.lookTarget.LengthSquared() <= 0.0001f)
            fromPose = toPose;
    }
    else
    {
        const PreviewTransitionStep& previousStep = _transitionPreviewSteps[stepIndex - 1];
        Apply_TransitionStepContext(previousStep);
        if (!Evaluate_PreviewModePose(previousStep.mode, std::max(0.f, previousStep.durationSec), false, fromPose))
        {
            Apply_TransitionStepContext(previousStep);
            fromPose = Capture_PreviewCurrentPose();
        }
        Apply_TransitionStepContext(step);
    }

    const float duration = std::max(0.0001f, step.durationSec);
    const float ratio = (stepIndex == 0) ? 1.f : std::clamp(clampedTime / duration, 0.f, 1.f);
    const float easedRatio = (stepIndex == 0)
        ? 1.f
        : Client::BattleTurnCamera::Evaluate_CameraTransitionT(_editingBattlePreset.transitionEasing, ratio);

    Client::BattleTurnCameraEvaluatedPose blendedBase{};
    blendedBase.cameraPos = Vec3::Lerp(fromPose.cameraPos, toPose.cameraPos, easedRatio);
    blendedBase.lookTarget = Vec3::Lerp(fromPose.lookTarget, toPose.lookTarget, easedRatio);
    blendedBase.fovY = fromPose.fovY + (toPose.fovY - fromPose.fovY) * easedRatio;
    blendedBase.rollOffset = fromPose.rollOffset + (toPose.rollOffset - fromPose.rollOffset) * easedRatio;

    Client::BattleTurnCameraEvaluatedPose finalPose = blendedBase;
    Client::BattleTurnCameraEvaluatedPose currentStepBasePose{};
    Client::BattleTurnCameraEvaluatedPose currentStepClipPose{};
    if (Evaluate_PreviewModePose(step.mode, clampedTime, false, currentStepBasePose) &&
        Evaluate_PreviewModePose(step.mode, clampedTime, true, currentStepClipPose))
    {
        finalPose.cameraPos += (currentStepClipPose.cameraPos - currentStepBasePose.cameraPos);
        finalPose.lookTarget += (currentStepClipPose.lookTarget - currentStepBasePose.lookTarget);
        finalPose.fovY += (currentStepClipPose.fovY - currentStepBasePose.fovY);
        finalPose.rollOffset += (currentStepClipPose.rollOffset - currentStepBasePose.rollOffset);
    }

    Apply_PreviewEvaluatedPose(finalPose);
    _previewDirty = false;
}

void BattleCameraPreview_View::Rebuild_TransitionPreviewSequenceAtTime(float timeSec)
{
    int stepIndex = -1;
    float stepLocalTime = 0.f;
    Resolve_TransitionSequenceTime(timeSec, stepIndex, stepLocalTime);

    if (stepIndex < 0 || stepIndex >= static_cast<int>(_transitionPreviewSteps.size()))
        return;

    Rebuild_TransitionPreviewStepAtTime(stepIndex, stepLocalTime);
}

void BattleCameraPreview_View::Start_TransitionPreviewPlayback()
{
    if (_transitionPreviewSteps.empty())
        return;

    _transitionPreviewPlaying = true;
    _transitionPreviewActiveStepIndex = 0;
    _transitionPreviewElapsed = 0.f;
    _transitionPreviewSequenceTimeSec = 0.f;
    Rebuild_TransitionPreviewStepAtTime(0, 0.f);
}

void BattleCameraPreview_View::Stop_TransitionPreviewPlayback()
{
    _transitionPreviewPlaying = false;
    _transitionPreviewActiveStepIndex = -1;
    _transitionPreviewElapsed = 0.f;
}

void BattleCameraPreview_View::Update_TransitionPreviewPlayback(float timeDelta)
{
    if (!_transitionPreviewPlaying || _transitionPreviewSteps.empty())
        return;

    if (_transitionPreviewActiveStepIndex < 0 ||
        _transitionPreviewActiveStepIndex >= static_cast<int>(_transitionPreviewSteps.size()))
    {
        Stop_TransitionPreviewPlayback();
        return;
    }

    const float clampedDelta = std::max(0.f, timeDelta);
    _transitionPreviewElapsed += clampedDelta;
    _transitionPreviewSequenceTimeSec += clampedDelta;
    const PreviewTransitionStep& currentStep = _transitionPreviewSteps[_transitionPreviewActiveStepIndex];

    if (_transitionPreviewAutoFollowActiveStep)
        Set_TransitionEditTargetStep(_transitionPreviewActiveStepIndex);

    if (_transitionPreviewElapsed < currentStep.durationSec)
    {
        Rebuild_TransitionPreviewStepAtTime(_transitionPreviewActiveStepIndex, _transitionPreviewElapsed);
        return;
    }

    _transitionPreviewElapsed = 0.f;
    ++_transitionPreviewActiveStepIndex;

    if (_transitionPreviewActiveStepIndex >= static_cast<int>(_transitionPreviewSteps.size()))
    {
        if (!_transitionPreviewLoop)
        {
            _transitionPreviewSequenceTimeSec = Get_TransitionSequenceDuration();
            Stop_TransitionPreviewPlayback();
            return;
        }

        _transitionPreviewActiveStepIndex = 0;
        _transitionPreviewSequenceTimeSec = 0.f;
    }

    Rebuild_TransitionPreviewStepAtTime(_transitionPreviewActiveStepIndex, 0.f);
}

void BattleCameraPreview_View::Set_TransitionEditTargetStep(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= static_cast<int>(_transitionPreviewSteps.size()))
        return;

    _transitionPreviewEditTargetStepIndex = stepIndex;
    _transitionPreviewEditTargetPinned = true;
    _hasLoadedBattlePreset = false;
    _previewDirty = true;
}

void BattleCameraPreview_View::Update_PreviewAttackActorMotion(float previewTimeSec)
{
    Position_PreviewObjects();

    if (!_previewAttackMovementEnabled)
        return;

    if (_previewContext.mode != BattleCameraMode::ENEMYATTACK &&
        _previewContext.mode != BattleCameraMode::PLAYERATTACK)
        return;

    string skillId = _previewContext.skillId;
    if (skillId.empty())
        skillId = Get_SelectedSkillId();

    if (skillId.empty())
        return;

    const Client::Skill_Data* skill = CLIENT->Get_Skill(skillId);
    if (skill == nullptr || skill->AnimPhases.empty())
        return;

    Shared<GameObject> subjectObject = Resolve_PreviewBattleActor(_previewContext.subjectSide);
    Shared<GameObject> opponentObject = Resolve_PreviewBattleActor(_previewContext.opponentSide);
    if (subjectObject == nullptr || opponentObject == nullptr)
        return;

    Shared<TransformCom> subjectTransform = subjectObject->Get_Transform();
    if (subjectTransform == nullptr)
        return;

    Shared<ModelCom> subjectModel = Resolve_PreviewSubjectModel();
    if (subjectModel == nullptr)
        return;

    _previewContext.subjectOriginPos = Get_PreviewSlotWorldPosition(_previewContext.subjectSide, _previewContext.subjectSlot);

    string currentAnimationName{};
    if (_useSkillShotTimeline)
    {
        if (const PreviewSkillPhaseBlock* activePhaseBlock = Resolve_ActivePreviewSkillPhase(previewTimeSec))
            currentAnimationName = activePhaseBlock->animationKey;
    }

    if (currentAnimationName.empty() &&
        _selectedPreviewAnimationIndex >= 0 &&
        _selectedPreviewAnimationIndex < static_cast<int>(_previewAnimationNames.size()))
    {
        currentAnimationName = _previewAnimationNames[_selectedPreviewAnimationIndex];
    }

    const Client::BattleActionPhase* currentPhase = &skill->AnimPhases.front();
    for (const Client::BattleActionPhase& phase : skill->AnimPhases)
    {
        if (!currentAnimationName.empty() && phase.animationKey == currentAnimationName)
        {
            currentPhase = &phase;
            break;
        }
    }

    if (currentPhase == nullptr)
        return;

    float phaseRatio = 0.f;
    if (_useSkillShotTimeline)
    {
        if (const PreviewSkillPhaseBlock* activePhase = Resolve_ActivePreviewSkillPhase(previewTimeSec))
        {
            const float phaseDuration = std::max(0.0001f, activePhase->durationSec);
            phaseRatio = std::clamp((previewTimeSec - activePhase->startTimeSec) / phaseDuration, 0.f, 1.f);
        }
    }
    else
    {
        const float animationDuration = std::max(0.0001f, subjectModel->Get_CurrentAnimationDurationSec());
        phaseRatio = std::clamp(subjectModel->Get_CurrentAnimationTimeSec() / animationDuration, 0.f, 1.f);
    }

    const Vec3 phaseGoalPos = Resolve_PhaseGoalPosition(_previewContext, *currentPhase);
    Vec3 desiredPos = _previewContext.subjectOriginPos;

    if (currentPhase->faceTarget)
    {
        Vec3 faceDir = phaseGoalPos - desiredPos;
        faceDir.y = 0.f;
        if (faceDir.LengthSquared() > 0.0001f)
        {
            faceDir.Normalize();
            const float yawRad = atan2f(faceDir.x, faceDir.z);
            Vec3 rot = subjectTransform->Get_WorldRotationEuler();
            rot.y = XMConvertToDegrees(yawRad);
            subjectTransform->Set_WorldRotationEuler(rot);
        }
    }

    switch (currentPhase->movementPolicy)
    {
    case Client::BattleMovementPolicy::SnapToTarget:
        desiredPos = phaseGoalPos;
        break;

    case Client::BattleMovementPolicy::MoveToTarget:
    case Client::BattleMovementPolicy::ReturnToOrigin:
    case Client::BattleMovementPolicy::RootMotionToTarget:
        desiredPos = Vec3::Lerp(_previewContext.subjectOriginPos, phaseGoalPos, phaseRatio);
        break;

    case Client::BattleMovementPolicy::None:
    case Client::BattleMovementPolicy::Custom:
    default:
        desiredPos = _previewContext.subjectOriginPos;
        break;
    }

    desiredPos.y = _previewContext.subjectOriginPos.y;
    subjectTransform->Set_WorldPosition(desiredPos);
}

Shared<GameObject> BattleCameraPreview_View::Resolve_PreviewBattleActor(PreviewSlotSide side)
{
    switch (side)
    {
    case PreviewSlotSide::Hero:
        return _previewHeroObject;

    case PreviewSlotSide::Enemy:
        return _previewEnemyObject;

    default:
        return nullptr;
    }
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

    if (_selectedSkillIndex >= 0 && _selectedSkillIndex < static_cast<int>(_skillIds.size()))
        return _skillIds[_selectedSkillIndex];

    return _previewContext.skillId;
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

vector<Client::BattleMonsterSpawnDesc> BattleCameraPreview_View::Resolve_SelectedEnemySpawnDescs() const
{
    const string enemyBattlePrefabName = Get_SelectedEnemyPrefabName();
    if (enemyBattlePrefabName.empty())
        return {};

    const string enemyWorldPrefabName = Resolve_WorldMonsterPrefabName(enemyBattlePrefabName);
    if (enemyWorldPrefabName.empty())
        return {};

    const fs::path prefabPath = Client::Spawn_Prefab::Resolve_PrefabPath(enemyWorldPrefabName);
    Shared<GameObject> worldMonsterObject = GAME->Instantiate_GameObjectJson(prefabPath);
    Shared<Client::Monster_WorldBase> worldMonster =
        std::dynamic_pointer_cast<Client::Monster_WorldBase>(worldMonsterObject);
    if (worldMonster == nullptr)
        return {};

    return worldMonster->Get_BattleMonsterSpawnDescs();
}

BattleCameraMode BattleCameraPreview_View::Get_CurrentBattleAuthoringMode() const
{
    if (_activeTimelineEditorTab != TimelineEditorTab::TransitionPreview)
        return _previewContext.mode;

    return Get_BattlePresetEditorMode();
}

string BattleCameraPreview_View::Get_CurrentPreviewPresetId() const
{
    return Get_CurrentBattlePresetAuthoringOverrideId();
}

string BattleCameraPreview_View::Get_CurrentPreviewClipId() const
{
    return Get_CurrentBattleClipAuthoringOverrideId();
}

BattleCameraMode BattleCameraPreview_View::Get_BattlePresetEditorMode() const
{
    if (_activeTimelineEditorTab != TimelineEditorTab::TransitionPreview)
        return _previewContext.mode;

    if (_transitionPreviewAutoFollowActiveStep &&
        _transitionPreviewActiveStepIndex >= 0 &&
        _transitionPreviewActiveStepIndex < static_cast<int>(_transitionPreviewSteps.size()))
    {
        return _transitionPreviewSteps[_transitionPreviewActiveStepIndex].mode;
    }

    if (_transitionPreviewEditTargetPinned &&
        _transitionPreviewEditTargetStepIndex >= 0 &&
        _transitionPreviewEditTargetStepIndex < static_cast<int>(_transitionPreviewSteps.size()))
    {
        return _transitionPreviewSteps[_transitionPreviewEditTargetStepIndex].mode;
    }

    return _previewContext.mode;
}

CameraTrack* BattleCameraPreview_View::Get_SelectedBattleClipTrack()
{
    switch (_battleClipSelectedTrackIndex)
    {
    case 0:
        return &_editingBattleClip.distanceOffset;
    case 1:
        return &_editingBattleClip.fovOffset;
    case 2:
        return &_editingBattleClip.shakeAmplitude;
    case 3:
        return &_editingBattleClip.sideOffset;
    case 4:
        return &_editingBattleClip.pivotOffset;
    case 5:
        return &_editingBattleClip.heightOffset;
    case 6:
        return &_editingBattleClip.aimOffset;
    case 7:
        return &_editingBattleClip.lookHeightOffset;
    case 8:
        return &_editingBattleClip.yawOffset;
    case 9:
        return &_editingBattleClip.orbitYaw;
    case 10:
        return &_editingBattleClip.orbitRadius;
    case 11:
        return &_editingBattleClip.orbitHeight;
    case 12:
        return &_editingBattleClip.orbitLookHeight;
    case 13:
        return &_editingBattleClip.shakeFrequencyX;
    case 14:
        return &_editingBattleClip.shakeFrequencyY;
    case 15:
        return &_editingBattleClip.shakeDecayRateX;
    case 16:
        return &_editingBattleClip.shakeDecayRate;
    case 17:
        return &_editingBattleClip.shakeVerticalRatio;
    case 18:
        return &_editingBattleClip.shakeStrengthScale;
    default:
        return nullptr;
    }
}

CameraKeyframe* BattleCameraPreview_View::Get_SelectedBattleClipKeyframe()
{
    CameraTrack* track = Get_SelectedBattleClipTrack();
    if (track == nullptr)
        return nullptr;

    if (_battleClipSelectedKeyIndex < 0 || _battleClipSelectedKeyIndex >= static_cast<int>(track->keyframes.size()))
        return nullptr;

    return &track->keyframes[_battleClipSelectedKeyIndex];
}

float BattleCameraPreview_View::Get_TransitionSequenceDuration() const
{
    float total = 0.f;

    for (const PreviewTransitionStep& transitionStep : _transitionPreviewSteps)
    {
        total += std::max(0.05f, transitionStep.durationSec);
    }

    return total;
}

float BattleCameraPreview_View::Get_DefaultBattleClipValue(int trackIndex)
{
    switch (trackIndex)
    {
        case 13: return 32.f;   // shakeFrequencyX
        case 14: return 47.f;   // shakeFrequencyY
        case 15: return 0.f;    // shakeDecayRateX
        case 16: return 8.f;    // shakeDecayRate
        case 17: return 0.8f;   // shakeVerticalRatio
        case 18: return 0.05f;  // shakeStrengthScale
        default: return 0.f;
    }
}

void BattleCameraPreview_View::Resolve_TransitionSequenceTime(float time, int& outStepIndex, float& outStepLocalTime)
{
    outStepIndex = -1;
    outStepLocalTime = 0.f;

    if (_transitionPreviewSteps.empty())
        return;

    const float clampedTime = std::clamp(time, 0.f, Get_TransitionSequenceDuration());
    float accumulated = 0.f;

    for (int i = 0; i < static_cast<int>(_transitionPreviewSteps.size()); ++i)
    {
        const float stepDuration = std::max(0.05f, _transitionPreviewSteps[i].durationSec);
        const float stepEnd = accumulated + stepDuration;
        const bool isLastStep = (i == static_cast<int>(_transitionPreviewSteps.size()) - 1);

        if (clampedTime < stepEnd || (isLastStep && clampedTime <= stepEnd))
        {
            outStepIndex = i;
            outStepLocalTime = std::clamp(clampedTime - accumulated, 0.f, stepDuration);
            return;
        }

        accumulated = stepEnd;
    }
}

void BattleCameraPreview_View::Scrub_TransitionSequence(float timeSec)
{
    int stepIndex = -1;
    float stepLocalTime = 0.f;
    Resolve_TransitionSequenceTime(timeSec, stepIndex, stepLocalTime);

    if (stepIndex < 0 || stepIndex >= static_cast<int>(_transitionPreviewSteps.size()))
        return;

    _transitionPreviewSequenceTimeSec = std::clamp(timeSec, 0.f, Get_TransitionSequenceDuration());
    _transitionPreviewActiveStepIndex = stepIndex;
    _transitionPreviewElapsed = stepLocalTime;
    Rebuild_TransitionPreviewSequenceAtTime(_transitionPreviewSequenceTimeSec);
}

Vec3 BattleCameraPreview_View::Resolve_PhaseGoalPosition(const PreviewContext& context, const Client::BattleActionPhase& phase)
{
    Shared<GameObject> subject = Resolve_PreviewBattleActor(context.subjectSide);

    if (!subject)
        return {};

    Shared<TransformCom> subjectTransform = subject->Get_Transform();

    if (subjectTransform == nullptr)
        return {};

    const Vec3 actorPos = subjectTransform->Get_WorldPosition();

    if (phase.movementPolicy == Client::BattleMovementPolicy::ReturnToOrigin)
        return context.subjectOriginPos;

    const Vec3 anchorPos = Resolve_AnchorPosition(context, phase.targetAnchor);

    if (!phase.useDistance || phase.targetAnchor == Client::BattleTargetAnchor::Origin)
        return anchorPos;

    if (phase.targetAnchor == Client::BattleTargetAnchor::Self)
        return anchorPos;

    Vec3 dir = anchorPos - actorPos;
    dir.y = 0.f;

    if (dir.LengthSquared() <= 0.0001f)
        return anchorPos;

    dir.Normalize();

    return anchorPos - dir * phase.offsetDistance;
}


Vec3 BattleCameraPreview_View::Resolve_AnchorPosition(const PreviewContext& context, Client::BattleTargetAnchor anchor)
{
    Shared<GameObject> subject = Resolve_PreviewBattleActor(context.subjectSide);
    Shared<GameObject> opponent = Resolve_PreviewBattleActor(context.opponentSide);

    switch (anchor)
    {
    case Client::BattleTargetAnchor::Self:
        if (subject && subject->Get_Transform())
            return subject->Get_Transform()->Get_WorldPosition();
        break;

    case Client::BattleTargetAnchor::PrimaryTarget:
        if (opponent && opponent->Get_Transform())
            return opponent->Get_Transform()->Get_WorldPosition();
        break;

    case Client::BattleTargetAnchor::TargetCenter:
        if (opponent && opponent->Get_Transform())
            return opponent->Get_Transform()->Get_WorldPosition();
        break;

    case Client::BattleTargetAnchor::Origin:
        return context.subjectOriginPos;

    default:
        break;
    }

    if (subject && subject->Get_Transform())
        return subject->Get_Transform()->Get_WorldPosition();

    return Vec3::Zero;
}

Shared<BattleCameraPreview_View> BattleCameraPreview_View::Create()
{
    return make_shared<BattleCameraPreview_View>();
}
