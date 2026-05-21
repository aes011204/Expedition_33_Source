#include "Animation_View.h"
#include "Animation_NameUtils.h"
#include "AnimSequencerAdapter.h"
#include "AnimNotify_Parser.h"
#include "ContainerObject.h"
#include "EditorInstance.h"
#include "Editor_Camera_Free.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "DebugDraw.h"
#include "Editor_Macro.h"
#include "Inspector.h"
#include "ModelCom.h"
#include "Prefab_View.h"
#include "PartObject.h"
#include "RenderTarget.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "imgui_internal.h"
#include "Notification_Manager.h"

namespace
{
    string Resolve_NotifyAnimationKey(const Animation_View::AnimationEntry& entry)
    {
        if (!entry.playbackName.empty())
            return entry.playbackName;

        if (!entry.name.empty())
            return entry.name;

        return entry.displayName;
    }

    AnimNotifyData* Find_OrNormalize_NotifyAnimation(
        AnimNotifyAsset& asset,
        const Animation_View::AnimationEntry& entry)
    {
        const string runtimeKey = Resolve_NotifyAnimationKey(entry);

        if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, runtimeKey))
            return animation;

        if (!entry.name.empty())
        {
            if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.name))
            {
                animation->animationName = runtimeKey;
                return animation;
            }
        }

        if (!entry.displayName.empty())
        {
            if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.displayName))
            {
                animation->animationName = runtimeKey;
                return animation;
            }
        }

        return &AnimNotify_Parser::Get_OrAddAnimation(asset, runtimeKey);
    }

    const AnimNotifyData* Find_NotifyAnimation(
        const AnimNotifyAsset& asset,
        const Animation_View::AnimationEntry& entry)
    {
        const string runtimeKey = Resolve_NotifyAnimationKey(entry);

        if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, runtimeKey))
            return animation;

        if (!entry.name.empty())
        {
            if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.name))
                return animation;
        }

        if (!entry.displayName.empty())
        {
            if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(asset, entry.displayName))
                return animation;
        }

        return nullptr;
    }

    void Invalidate_AnimNotifyAssetsForObject(
        const Shared<GameObject>& gameObject,
        const wstring& sourceModelPath)
    {
        if (gameObject == nullptr)
            return;

        if (const Shared<ModelCom> modelCom = gameObject->Get_Component<ModelCom>())
        {
            if (sourceModelPath.empty() || modelCom->Get_SourceModelPath() == sourceModelPath)
                modelCom->Invalidate_AnimNotifyAsset();
        }

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            Invalidate_AnimNotifyAssetsForObject(partObject, sourceModelPath);
        }
    }
}

Animation_View::Animation_View()
    : Editor_Window(L"Animation")
{
    _sequencer = make_unique<AnimSequencerAdapter>(this);
    Set_Open(false);
}

void Animation_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    if (EDITOR->IsPlaying())
        return;

    if (Is_PrefabViewFocused())
        return;

    if (_previewCamera && _isPreviewHovered)
        _previewCamera->Priority_Update(timeDelta);

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    if (!playbackModel->Is_AnimationPaused())
        _sequencerCurrentFrame = static_cast<int32>(std::round(playbackModel->Get_CurrentAnimationTimeSec() * static_cast<float>(Get_CurrentAnimationFps())));
}

void Animation_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (EDITOR->IsPlaying())
        return;

    if (Is_PrefabViewFocused())
        return;

    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    if (previewRenderObject == nullptr || _previewCamera == nullptr)
        return;

    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);
    if (_previewRenderTarget == nullptr)
        return;

    const Matrix savedView = *GAME->Get_Transform(D3DTS::View);
    const Matrix savedProj = *GAME->Get_Transform(D3DTS::Proj);
    vector<LIGHT_DESC> savedLightDescs{};
    GAME->Get_LightDescs(savedLightDescs);
    const bool wasPlayerInputBlocked = GAME->Is_PlayerInputBlocked();

    if (FAILED(_previewRenderTarget->Begin(true, true)))
        return;

    const Matrix previewView = _previewCamera->Get_ViewMatrix();
    const float aspect = static_cast<float>(_previewWidth) / static_cast<float>(max(1u, _previewHeight));
    const Matrix previewProj = XMMatrixPerspectiveFovLH(_previewCamera->Get_FovY(), aspect, _previewCamera->Get_Near(), _previewCamera->Get_Far());

    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(previewView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(previewProj));
    GAME->UpdateFrustum(previewView, previewProj);

    vector<LIGHT_DESC> previewLightDescs{};
    previewLightDescs.reserve(2);

    LIGHT_DESC keyLight{};
    keyLight.type = LightType::Directional;
    keyLight.direction = Vec4(0.35f, -1.f, 0.45f, 0.f);
    keyLight.diffuse = Vec4(0.55f, 0.55f, 0.55f, 1.f);
    keyLight.ambient = Vec4(0.14f, 0.14f, 0.14f, 1.f);
    keyLight.specular = Vec4(0.18f, 0.18f, 0.18f, 1.f);
    previewLightDescs.push_back(keyLight);

    LIGHT_DESC fillLight{};
    fillLight.type = LightType::Directional;
    fillLight.direction = Vec4(-0.4f, -0.25f, -0.8f, 0.f);
    fillLight.diffuse = Vec4(0.18f, 0.18f, 0.2f, 1.f);
    fillLight.ambient = Vec4(0.04f, 0.04f, 0.04f, 1.f);
    fillLight.specular = Vec4(0.05f, 0.05f, 0.05f, 1.f);
    previewLightDescs.push_back(fillLight);

    GAME->Set_Lights(previewLightDescs);
    GAME->Set_PlayerInputBlocked(true);

    const float dt = ImGui::GetIO().DeltaTime;
    if (_sourceMode == SourceMode::LiveModel)
    {
        previewRenderObject->Priority_Update(dt);
        previewRenderObject->Update(dt);
    }

    GAME->Backup_RenderGroup();
    previewRenderObject->Late_Update(dt);
    Lock_PreviewObjectToOrigin(previewRenderObject);

    GAME->Draw_Preview();
    GAME->Restore_RenderGroup();
    Ensure_PreviewGridResources();
    Draw_PreviewGrid();
    GAME->Set_Lights(savedLightDescs);
    GAME->Set_PlayerInputBlocked(wasPlayerInputBlocked);

    GAME->Bind_BackBuffer();
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
}

void Animation_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (_pendingDockId != 0)
    {
        ImGui::DockBuilderDockWindow(windowName.c_str(), _pendingDockId);
        ImGui::SetNextWindowDockID(_pendingDockId, ImGuiCond_Always);
    }
    if (_requestFocusOnOpen)
        ImGui::SetNextWindowFocus();

    _isFocused = false;
    _isHovered = false;

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _pendingDockId = 0;
        _requestFocusOnOpen = false;
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        if (_isFocused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false))
        {
            Shared<ModelCom> playbackModel = Get_PlaybackModel();
            if (playbackModel != nullptr)
                playbackModel->Set_AnimationPaused(!playbackModel->Is_AnimationPaused());
        }

        Handle_ClipboardShortcuts();

        Draw_Header();
        ImGui::Separator();

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float minTopSectionHeight = 240.f;
        const float maxTopSectionHeight = max(minTopSectionHeight, availableHeight - 260.f);
        _topSectionHeight = std::clamp(_topSectionHeight, minTopSectionHeight, maxTopSectionHeight);

        if (ImGui::BeginChild("AnimationTopSection", ImVec2(0.f, _topSectionHeight), false))
        {
            if (ImGui::BeginTable("AnimationViewTopLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("ClipBrowser", ImGuiTableColumnFlags_WidthFixed, 320.f);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("SelectedDetail", ImGuiTableColumnFlags_WidthFixed, 380.f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                Draw_AnimationSelector();

                ImGui::TableSetColumnIndex(1);
                Draw_PreviewPanel();

                ImGui::TableSetColumnIndex(2);
                Draw_SelectedDetail();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        const ImVec2 splitterPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("AnimationTopBottomSplitter", ImVec2(-FLT_MIN, 6.f));
        if (ImGui::IsItemActive())
            _topSectionHeight = std::clamp(_topSectionHeight + ImGui::GetIO().MouseDelta.y, minTopSectionHeight, maxTopSectionHeight);

        const bool splitterHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitterPos,
            ImVec2(splitterPos.x + ImGui::GetContentRegionAvail().x, splitterPos.y + 6.f),
            splitterHovered ? IM_COL32(110, 110, 118, 255) : IM_COL32(72, 72, 78, 255),
            2.f);

        Draw_PlaybackToolbar();
        ImGui::Separator();

        if (ImGui::BeginTable("AnimationViewBottomLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("EventLists", ImGuiTableColumnFlags_WidthFixed, 250.f);
            ImGui::TableSetupColumn("Sequencer", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("TrackPanel", ImGuiTableColumnFlags_WidthFixed, 260.f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Draw_EventListPanel();

            ImGui::TableSetColumnIndex(1);
            Draw_Sequencer();

            ImGui::TableSetColumnIndex(2);
            Draw_TrackSection();

            ImGui::EndTable();
        }

        Draw_CreateNotifyPopup();
        Draw_CreateNotifyStatePopup();
        Draw_SequencerContextMenu();
    }

    Set_Open(isOpen);
    ImGui::End();
}

bool Animation_View::CanSave() const
{
    return !_modelGuid.empty() && !_currentAnimationName.empty();
}

void Animation_View::Save()
{
    Save_NotifyAsset();
}

void Animation_View::Open_FromAsset(const wstring& modelPath, const string& modelGuid, const string& preferredAnimationName, uint32 preferredDockId)
{
    _sourceMode = SourceMode::None;
    _sourceModelPath = modelPath;
    _liveModelCom.reset();
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = true;

    _modelGuid = modelGuid;
    _currentAnimationName = preferredAnimationName;
    _selectedNotifyId = -1;
    _selectedStateId = -1;
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerFirstFrame = 0;
    _isDirty = false;
    _animationSearchBuffer[0] = '\0';

    Refresh_Source();
    Load_NotifyAsset();

    Set_Open(true);
}

void Animation_View::Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName, uint32 preferredDockId, bool focusOnOpen)
{
    const wstring sourceModelPath = modelCom ? modelCom->Get_SourceModelPath() : wstring{};

    _sourceMode = SourceMode::LiveModel;
    _liveModelCom = modelCom;
    _sourceModelPath = sourceModelPath;
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = focusOnOpen;

    _modelGuid = _sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _currentAnimationName = preferredAnimationName;
    _selectedNotifyId = -1;
    _selectedStateId = -1;
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerFirstFrame = 0;
    _isDirty = false;
    _animationSearchBuffer[0] = '\0';

    Refresh_Source();
    Load_NotifyAsset();
    Ensure_PreviewCamera();
    Fit_PreviewCameraToModel();
    Sync_PreviewAnimationSelection(true);

    Set_Open(true);
}

int32 Animation_View::Get_CurrentAnimationFps() const
{
    if (const AnimNotifyData* animationData = Get_CurrentAnimation())
        return max(1, animationData->displayFps);

    return 60;
}

float Animation_View::Get_CurrentAnimationLengthSec() const
{
    if (_liveModelCom != nullptr)
        return _liveModelCom->Get_CurrentAnimationDurationSec();

    return Resolve_AnimNotifyDataLengthSec(Get_CurrentAnimation());
}

int32 Animation_View::Get_CurrentAnimationFrameMax() const
{
    return max(1, static_cast<int32>(std::round(Get_CurrentAnimationLengthSec() * static_cast<float>(Get_CurrentAnimationFps()))));
}

void Animation_View::Refresh_Source()
{
    _availableAnimations.clear();

    if (_sourceMode == SourceMode::LiveModel)
    {
        Refresh_LiveAnimationList();
    }
    else
    {
        Refresh_AssetAnimationList();
    }

    if (_currentAnimationName.empty() && !_availableAnimations.empty())
    {
        _currentAnimationName = _availableAnimations.front().displayName;
    }
    else if (!_currentAnimationName.empty())
    {
        if (const AnimationEntry* foundEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName))
        {
            _currentAnimationName = foundEntry->displayName;
        }
    }
}

void Animation_View::Refresh_AssetAnimationList()
{
    const vector<AnimationNameUtils::AssetAnimationInfo> assetInfos =
        AnimationNameUtils::Collect_AssetAnimationInfos(_sourceModelPath);
    _availableAnimations.reserve(assetInfos.size());

    for (const auto& assetInfo : assetInfos)
    {
        AnimationEntry entry{};
        entry.name = assetInfo.stem;
        entry.displayName = assetInfo.displayName;
        entry.playbackName = assetInfo.stem;
        _availableAnimations.push_back(move(entry));
    }
}

void Animation_View::Refresh_LiveAnimationList()
{
    if (_liveModelCom == nullptr)
        return;

    const uint32 numAnimations = static_cast<uint32>(_liveModelCom->Get_NumAnimations());
    const vector<AnimationNameUtils::AssetAnimationInfo> assetInfos =
        AnimationNameUtils::Collect_AssetAnimationInfos(_liveModelCom->Get_SourceModelPath());
    vector<string> playbackNames{};
    playbackNames.reserve(numAnimations);

    for (uint32 i = 0; i < numAnimations; ++i)
    {
        if (const char* animationName = _liveModelCom->Get_AnimationName(i))
            playbackNames.push_back(animationName);
    }

    if (!assetInfos.empty())
    {
        vector<bool> usedPlayback(playbackNames.size(), false);
        _availableAnimations.reserve(assetInfos.size() + playbackNames.size());

        for (const auto& assetInfo : assetInfos)
        {
            AnimationEntry entry{};
            entry.name = assetInfo.stem;
            entry.displayName = assetInfo.displayName;
            entry.playbackName = assetInfo.stem;

            const int32 matchedPlaybackIndex =
                AnimationNameUtils::Find_BestPlaybackMatchIndex(assetInfo, playbackNames, usedPlayback);
            if (matchedPlaybackIndex >= 0)
            {
                entry.playbackName = playbackNames[matchedPlaybackIndex];
                usedPlayback[matchedPlaybackIndex] = true;
            }

            _availableAnimations.push_back(move(entry));
        }

        for (int32 i = 0; i < static_cast<int32>(playbackNames.size()); ++i)
        {
            if (i < static_cast<int32>(usedPlayback.size()) && usedPlayback[i])
                continue;

            const string& playbackName = playbackNames[i];

            AnimationEntry entry{};
            entry.name = playbackName;
            entry.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackName);
            entry.playbackName = playbackName;
            _availableAnimations.push_back(move(entry));
        }

        return;
    }

    for (const string& playbackName : playbackNames)
    {
        AnimationEntry entry{};
        entry.name = playbackName;
        entry.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackName);
        entry.playbackName = playbackName;
        _availableAnimations.push_back(move(entry));
    }
}

void Animation_View::Load_NotifyAsset()
{
    _asset = {};
    _asset.modelGuid = _modelGuid;

    const fs::path filePath = AnimNotify_Parser::Get_ModelNotifyFilePath(_modelGuid);
    if (!AnimNotify_Parser::Load_FromFile(filePath.wstring(), _asset))
    {
        const fs::path legacyFilePath = AnimNotify_Parser::Get_LegacyModelNotifyFilePath(_modelGuid);
        AnimNotify_Parser::Load_FromFile(legacyFilePath.wstring(), _asset);
    }

    if (_asset.modelGuid.empty())
        _asset.modelGuid = _modelGuid;

    if (_currentAnimationName.empty())
        return;

    if (const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName))
    {
        Find_OrNormalize_NotifyAnimation(_asset, *currentEntry);
        return;
    }

    AnimNotify_Parser::Get_OrAddAnimation(_asset, _currentAnimationName);
}

void Animation_View::Save_NotifyAsset()
{
    if (_modelGuid.empty())
        return;

    if (AnimNotifyData* animationData = Get_CurrentAnimation())
        AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    const fs::path filePath = AnimNotify_Parser::Get_ModelNotifyFilePath(_modelGuid);
    if (AnimNotify_Parser::Save_ToFile(filePath.wstring(), _asset))
    {
        Refresh_OpenModelNotifyAssets();
        _isDirty = false;
        NOTIFY("애니메이션 노티파이 저장: {}", _currentAnimationName);
    }
    else if (EDITOR->Get_Notification())
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(NotifyType::Warning, "애니메이션 노티파이 저장 실패: {}", _currentAnimationName);
    }
}

void Animation_View::Refresh_OpenModelNotifyAssets() const
{
    if (_liveModelCom != nullptr)
        _liveModelCom->Invalidate_AnimNotifyAsset();

    if (_sourceModelPath.empty())
        return;

    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);
            Invalidate_AnimNotifyAssetsForObject(gameObject, _sourceModelPath);
        });
}

void Animation_View::Draw_Header()
{
    const char* sourceLabel = "None";
    if (_sourceMode == SourceMode::LiveModel)
        sourceLabel = "LiveModel";
    else if (!_sourceModelPath.empty())
        sourceLabel = "Asset";

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    const char* previewName = currentEntry ? currentEntry->displayName.c_str() : "(select animation)";

    if (ImGui::Button("Save"))
        Save_NotifyAsset();
    ImGui::SameLine();
    ImGui::Text("Model");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _modelGuid.empty() ? "(none)" : _modelGuid.c_str());
    ImGui::SameLine();
    ImGui::Text("| Source");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", sourceLabel);
    ImGui::SameLine();
    ImGui::Text("| Animation");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", previewName);
}

void Animation_View::Draw_AnimationSelector()
{
    ImGui::Text("Animation Clips");
    ImGui::Separator();
    ImGui::InputTextWithHint("##AnimationClipSearch", "Search animation...", _animationSearchBuffer, IM_ARRAYSIZE(_animationSearchBuffer));
    const string searchTextLower = String::ToLowerCopy(string(_animationSearchBuffer));

    if (!ImGui::BeginChild("AnimationClipBrowserPanel", ImVec2(0.f, 0.f), true))
    {
        ImGui::EndChild();
        return;
    }

    if (_availableAnimations.empty())
    {
        ImGui::TextDisabled("No clips found.");
        ImGui::EndChild();
        return;
    }

    for (const AnimationEntry& entry : _availableAnimations)
    {
        if (!searchTextLower.empty())
        {
            const string displayNameLower = String::ToLowerCopy(entry.displayName);
            const string nameLower = String::ToLowerCopy(entry.name);
            const string playbackNameLower = String::ToLowerCopy(entry.playbackName);

            if (displayNameLower.find(searchTextLower) == string::npos &&
                nameLower.find(searchTextLower) == string::npos &&
                playbackNameLower.find(searchTextLower) == string::npos)
            {
                continue;
            }
        }

        const bool isSelected =
            (_currentAnimationName == entry.displayName ||
             _currentAnimationName == entry.name ||
             _currentAnimationName == entry.playbackName);
        const string label = Make_AnimationComboLabel(entry);

        if (ImGui::Selectable(label.c_str(), isSelected))
        {
            _currentAnimationName = entry.displayName;
            Find_OrNormalize_NotifyAnimation(_asset, entry);
            _selectedNotifyId = -1;
            _selectedStateId = -1;
            _selectedNotifyTrackIndex = 0;
            _selectedNotifyStateTrackIndex = 0;
            _sequencerCurrentFrame = 0;
            _sequencerSelectedRow = -1;
            _sequencerFirstFrame = 0;

            Sync_PreviewAnimationSelection();

            if (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr)
            {
                const int32 animationIndex = Find_AnimationIndex(_liveModelCom, entry.playbackName);
                if (animationIndex >= 0)
                {
                    _liveModelCom->Set_Animation(static_cast<uint32>(animationIndex), true);
                    _liveModelCom->Set_AnimationPaused(false);
                    _liveModelCom->Set_CurrentAnimationTimeSec(0.f);
                    _liveModelCom->Refresh_CurrentAnimationPose();
                }
            }
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndChild();
}

void Animation_View::Draw_PlaybackToolbar()
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const bool hasPlayback = (playbackModel != nullptr);
    const float animationLengthSec = Get_CurrentAnimationLengthSec();
    const float sliderMaxTimeSec = max(0.01f, animationLengthSec);
    const ImVec2 playbackButtonSize(64.f, 0.f);

    if (hasPlayback)
    {
        if (ImGui::Button(playbackModel->Is_AnimationPaused() ? "Play" : "Pause", playbackButtonSize))
            playbackModel->Set_AnimationPaused(!playbackModel->Is_AnimationPaused());

        ImGui::SameLine();

        if (ImGui::Button("Stop", playbackButtonSize))
        {
            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(0.f);
            playbackModel->Refresh_CurrentAnimationPose();
            _sequencerCurrentFrame = 0;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Frame %d", _sequencerCurrentFrame);
        ImGui::SameLine(0.f, 12.f);
        ImGui::TextDisabled("Max Frame %d", Get_CurrentAnimationFrameMax());
        ImGui::SameLine(0.f, 12.f);
        ImGui::Checkbox("Grid", &_showPreviewGrid);

        ImGui::SameLine();

        float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::SliderFloat("##AnimScrub", &currentTimeSec, 0.f, sliderMaxTimeSec, "%.3f s"))
        {
            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
            playbackModel->Refresh_CurrentAnimationPose();
            _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("Play", playbackButtonSize);
        ImGui::SameLine();
        ImGui::Button("Stop", playbackButtonSize);
        ImGui::SameLine();
        ImGui::TextDisabled("Frame %d", _sequencerCurrentFrame);
        ImGui::SameLine(0.f, 12.f);
        ImGui::TextDisabled("Max Frame %d", Get_CurrentAnimationFrameMax());
        ImGui::SameLine(0.f, 12.f);
        ImGui::Checkbox("Grid", &_showPreviewGrid);
        ImGui::SameLine();

        float dummyTimeSec = 0.f;
        ImGui::SetNextItemWidth(220.f);
        ImGui::SliderFloat("##AnimScrub", &dummyTimeSec, 0.f, sliderMaxTimeSec, "%.3f s");
        ImGui::EndDisabled();
    }
}

void Animation_View::Draw_PreviewPanel()
{
    ImGui::BeginChild("AnimationPreviewPanel", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextUnformatted("Preview");
    ImGui::Separator();

    ImVec2 previewSize = ImGui::GetContentRegionAvail();

    if (previewSize.x < 1.f)
        previewSize.x = 1.f;
    if (previewSize.y < 1.f)
        previewSize.y = 1.f;

    _previewWidth = max(1u, static_cast<uint32>(previewSize.x));
    _previewHeight = max(1u, static_cast<uint32>(previewSize.y));

    const ImVec2 previewPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(
        "AnimationPreviewCanvas",
        previewSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    _isPreviewHovered = ImGui::IsItemHovered();
    _isPreviewActive = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    Shared<ModelCom> playbackModel = Get_PlaybackModel();

    if (previewRenderObject != nullptr && playbackModel != nullptr && _previewRenderTarget && _previewRenderTarget->Get_SRV())
    {
        ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(_previewRenderTarget->Get_SRV()));
        drawList->AddImage(
            textureId,
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y));

    }
    else
    {
        drawList->AddRectFilled(
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y),
            IM_COL32(34, 38, 44, 255));

        drawList->AddRect(
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y),
            IM_COL32(85, 95, 110, 255));

        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewPos.y + 12.f),
            IM_COL32(210, 216, 224, 255),
            "Animation preview is not ready.");
    }

    ImGui::EndChild();
}

void Animation_View::Draw_Sequencer()
{
    if (Get_CurrentAnimation() == nullptr)
    {
        ImGui::TextDisabled("No animation selected.");
        return;
    }

    int sequenceOptions =
        ImSequencer::SEQUENCER_EDIT_STARTEND |
        ImSequencer::SEQUENCER_CHANGE_FRAME;

    _sequencerFirstFrame = 0;
    const ImVec2 sequencerRectMin = ImGui::GetCursorScreenPos();
    if (ImSequencer::Sequencer(
        _sequencer.get(),
        &_sequencerCurrentFrame,
        &_sequencerExpanded,
        &_sequencerSelectedRow,
        &_sequencerFirstFrame,
        sequenceOptions))
    {
        Shared<ModelCom> playbackModel = Get_PlaybackModel();
        if (playbackModel != nullptr)
        {
            const float currentTimeSec =
                static_cast<float>(_sequencerCurrentFrame) /
                static_cast<float>(max(1, Get_CurrentAnimationFps()));

            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
            playbackModel->Refresh_CurrentAnimationPose();
        }
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const ImRect sequencerRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        bool isStateTrack = false;
        int32 trackIndex = -1;
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const bool hasTrackContext = Try_GetSequencerTrackContext(sequencerRectMin, mousePos, isStateTrack, trackIndex);
        const float normalized = std::clamp(
            (mousePos.x - (sequencerRect.Min.x + 200.f)) / max(1.f, sequencerRect.Max.x - (sequencerRect.Min.x + 200.f)),
            0.f,
            1.f);
        const int32 clickedFrame = static_cast<int32>(std::round(normalized * static_cast<float>(Get_CurrentAnimationFrameMax())));
        Begin_SequencerContextMenu(clickedFrame, hasTrackContext, isStateTrack, trackIndex);
    }

    if (!_sequencerMarkingState &&
        _sequencerMarkStartFrame >= 0 &&
        _sequencerMarkEndFrame >= 0 &&
        _sequencerMarkStartFrame != _sequencerMarkEndFrame)
    {
        _requestedCreateStateStartFrame = min(_sequencerMarkStartFrame, _sequencerMarkEndFrame);
        _requestedCreateStateEndFrame = max(_sequencerMarkStartFrame, _sequencerMarkEndFrame);
        _openCreateNotifyStatePopup = true;
        _sequencerMarkStartFrame = -1;
        _sequencerMarkEndFrame = -1;
    }
}

void Animation_View::Draw_SelectedDetail()
{
    ImGui::Text("Selected Detail");
    ImGui::Separator();

    if (!ImGui::BeginChild("AnimationSelectedDetailPanel", ImVec2(0.f, 0.f), true))
    {
        ImGui::EndChild();
        return;
    }

    const float detailMaxTimeSec = max(0.01f, Get_CurrentAnimationLengthSec());
    UNREFERENCED_PARAMETER(detailMaxTimeSec);

    if (AnimNotifyEventEntry* notify = Find_SelectedNotify())
    {
        if (notify->notify != nullptr)
            Inspector::Draw_ReflectionSection("SelectedNotify", notify->notify.get(), notify->notify->Get_ReflectionInfo());
    }
    else if (AnimNotifyStateEntry* notifyState = Find_SelectedState())
    {
        if (notifyState->notifyState != nullptr)
            Inspector::Draw_ReflectionSection("SelectedNotifyState", notifyState->notifyState.get(), notifyState->notifyState->Get_ReflectionInfo());
    }
    else
    {
        ImGui::TextDisabled("선택된 애니메이션 없음");
        ImGui::Spacing();
        ImGui::TextWrapped("Notify or NotifyState 선택");
    }

    ImGui::EndChild();
}

void Animation_View::Draw_EventListPanel()
{
    if (Get_CurrentAnimation() == nullptr)
    {
        ImGui::TextDisabled("선택된 애니메이션 없음");
        return;
    }

    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float topHeight = max(120.f, availableHeight * 0.5f - 6.f);

    if (ImGui::BeginChild("NotifyListContainer", ImVec2(0.f, topHeight), false))
        Draw_NotifyList();
    ImGui::EndChild();

    ImGui::Spacing();

    if (ImGui::BeginChild("StateListContainer", ImVec2(0.f, 0.f), false))
        Draw_StateList();
    ImGui::EndChild();
}

void Animation_View::Draw_NotifyList()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    const int32 notifyCount = animationData ? static_cast<int32>(animationData->notifies.size()) : 0;

    ImGui::Text("Notifies (%d)", notifyCount);
    ImGui::Separator();

    if (animationData == nullptr)
    {
        ImGui::TextDisabled("No notify data.");
        return;
    }

    const int32 fps = max(1, Get_CurrentAnimationFps());
    for (int32 trackIndex = 0; trackIndex < static_cast<int32>(animationData->notifyTracks.size()); ++trackIndex)
    {
        const string trackLabel = animationData->notifyTracks[trackIndex].name.empty()
            ? "Notify Track " + to_string(trackIndex)
            : animationData->notifyTracks[trackIndex].name;

        if (!ImGui::TreeNode((trackLabel + "##NotifyTrackTree" + to_string(trackIndex)).c_str()))
            continue;

        if (ImGui::Selectable(("Select Track##NotifyTrackSelect" + to_string(trackIndex)).c_str(), _selectedNotifyTrackIndex == trackIndex))
        {
            _selectedNotifyTrackIndex = trackIndex;
            _selectedNotifyId = -1;
        }

        for (const auto& entry : animationData->notifies)
        {
            if (!entry.notify || entry.trackIndex != trackIndex)
                continue;

            const int32 frame = static_cast<int32>(std::round(entry.timeSec * fps));
            const string label = entry.notify->Get_TypeName() + " (" + to_string(frame) + ")";
            const bool isSelected = (_selectedNotifyId == entry.id);
            if (ImGui::Selectable((label + "##NotifyEntry" + to_string(entry.id)).c_str(), isSelected))
            {
                _selectedNotifyId = entry.id;
                _selectedStateId = -1;
                _selectedNotifyTrackIndex = trackIndex;
            }
        }

        ImGui::TreePop();
    }
}

void Animation_View::Draw_StateList()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    const int32 stateCount = animationData ? static_cast<int32>(animationData->notifyStates.size()) : 0;

    ImGui::Text("Notify States (%d)", stateCount);
    ImGui::Separator();

    if (animationData == nullptr)
    {
        ImGui::TextDisabled("No notify state data.");
        return;
    }

    const int32 fps = max(1, Get_CurrentAnimationFps());
    for (int32 trackIndex = 0; trackIndex < static_cast<int32>(animationData->notifyStateTracks.size()); ++trackIndex)
    {
        const string trackLabel = animationData->notifyStateTracks[trackIndex].name.empty()
            ? "Notify State Track " + to_string(trackIndex)
            : animationData->notifyStateTracks[trackIndex].name;

        if (!ImGui::TreeNode((trackLabel + "##NotifyStateTrackTree" + to_string(trackIndex)).c_str()))
            continue;

        if (ImGui::Selectable(("Select Track##NotifyStateTrackSelect" + to_string(trackIndex)).c_str(), _selectedNotifyStateTrackIndex == trackIndex))
        {
            _selectedNotifyStateTrackIndex = trackIndex;
            _selectedStateId = -1;
        }

        for (const auto& entry : animationData->notifyStates)
        {
            if (!entry.notifyState || entry.trackIndex != trackIndex)
                continue;

            const int32 startFrame = static_cast<int32>(std::round(entry.startSec * fps));
            const int32 endFrame = static_cast<int32>(std::round((entry.startSec + entry.durationSec) * fps));
            const string label = entry.notifyState->Get_TypeName() + " (" + to_string(startFrame) + "~" + to_string(endFrame) + ")";
            const bool isSelected = (_selectedStateId == entry.id);
            if (ImGui::Selectable((label + "##NotifyStateEntry" + to_string(entry.id)).c_str(), isSelected))
            {
                _selectedStateId = entry.id;
                _selectedNotifyId = -1;
                _selectedNotifyStateTrackIndex = trackIndex;
            }
        }

        ImGui::TreePop();
    }
}

void Animation_View::Draw_TrackSection()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (!ImGui::BeginChild("AnimationTrackSectionPanel", ImVec2(0.f, 0.f), false))
    {
        ImGui::EndChild();
        return;
    }

    ImGui::SeparatorText("트랙 생성 / 관리");
    ImGui::TextDisabled("노티파이 트랙");
    char notifyTrackBuffer[128] = {};
    strcpy_s(notifyTrackBuffer, _newNotifyTrackName.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##NotifyTrackInput", "새 노티파이 트랙 이름", notifyTrackBuffer, static_cast<size_t>(std::size(notifyTrackBuffer))))
        _newNotifyTrackName = notifyTrackBuffer;

    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(56, 72, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(74, 92, 62, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(88, 108, 72, 255));
    for (int32 i = 0; i < static_cast<int32>(animationData->notifyTracks.size()); ++i)
    {
        const bool selected = (_selectedNotifyTrackIndex == i);
        const string displayName = Normalize_DefaultTrackName(animationData->notifyTracks[i].name);
        if (ImGui::Selectable((displayName + "##NotifyTrack" + to_string(i)).c_str(), selected))
        {
            _selectedNotifyTrackIndex = i;
            _selectedNotifyId = -1;
        }
    }
    ImGui::PopStyleColor(3);

    if (ImGui::Button("노티파이 트랙 생성", ImVec2(-FLT_MIN, 0.f)))
        Add_NotifyTrack(_newNotifyTrackName);

    if (ImGui::Button("노티파이 트랙 삭제", ImVec2(-FLT_MIN, 0.f)))
        Delete_SelectedNotifyTrack();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("노티파이 스테이트 트랙");
    char notifyStateTrackBuffer[128] = {};
    strcpy_s(notifyStateTrackBuffer, _newNotifyStateTrackName.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##NotifyStateTrackInput", "새 노티파이 스테이트 트랙 이름", notifyStateTrackBuffer, static_cast<size_t>(std::size(notifyStateTrackBuffer))))
        _newNotifyStateTrackName = notifyStateTrackBuffer;

    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(66, 56, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(90, 74, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(108, 88, 56, 255));
    for (int32 i = 0; i < static_cast<int32>(animationData->notifyStateTracks.size()); ++i)
    {
        const bool selected = (_selectedNotifyStateTrackIndex == i);
        const string displayName = Normalize_DefaultTrackName(animationData->notifyStateTracks[i].name);
        if (ImGui::Selectable((displayName + "##NotifyStateTrack" + to_string(i)).c_str(), selected))
        {
            _selectedNotifyStateTrackIndex = i;
            _selectedStateId = -1;
        }
    }
    ImGui::PopStyleColor(3);

    if (ImGui::Button("노티파이 스테이트 트랙 생성", ImVec2(-FLT_MIN, 0.f)))
        Add_NotifyStateTrack(_newNotifyStateTrackName);

    if (ImGui::Button("노티파이 스테이트 트랙 삭제", ImVec2(-FLT_MIN, 0.f)))
        Delete_SelectedNotifyStateTrack();

    ImGui::EndChild();
}

void Animation_View::Add_NotifyTrack(const string& trackName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    AnimNotifyTrackDesc trackDesc{};
    trackDesc.name = trackName.empty()
        ? "Notify"
        : trackName;

    animationData->notifyTracks.push_back(trackDesc);
    _selectedNotifyTrackIndex = static_cast<int32>(animationData->notifyTracks.size()) - 1;
    _isDirty = true;
}

void Animation_View::Add_NotifyStateTrack(const string& trackName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    AnimNotifyTrackDesc trackDesc{};
    trackDesc.name = trackName.empty()
        ? "Notify State"
        : trackName;

    animationData->notifyStateTracks.push_back(trackDesc);
    _selectedNotifyStateTrackIndex = static_cast<int32>(animationData->notifyStateTracks.size()) - 1;
    _isDirty = true;
}

void Animation_View::Add_Notify_AtFrame(int32 frame, const string& typeName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<AnimNotify> notifyInstance = GAME->Create_AnimNotifyPrototype(typeName);
    if (notifyInstance == nullptr)
        return;

    if (animationData->notifyTracks.empty())
        Add_NotifyTrack("Notify");

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 maxNotifyFrame = max(0, Get_CurrentAnimationFrameMax() - 1);
    const int32 clampedFrame = std::clamp(frame, 0, maxNotifyFrame);

    AnimNotifyEventEntry entry{};
    const int32 createdId = animationData->nextNotifyId++;
    entry.id = createdId;
    entry.timeSec = static_cast<float>(clampedFrame) / static_cast<float>(fps);
    entry.trackIndex = std::clamp(_sequencerContextHasTrack && !_sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyTrackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);
    entry.notify = notifyInstance;

    animationData->notifies.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    _selectedNotifyId = createdId;
    _selectedNotifyTrackIndex = std::clamp(_sequencerContextHasTrack && !_sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyTrackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);
    _isDirty = true;
}

void Animation_View::Add_State_ByFrameRange(int32 startFrame, int32 endFrame, const string& typeName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<AnimNotifyState> notifyStateInstance = GAME->Create_AnimNotifyStatePrototype(typeName);
    if (notifyStateInstance == nullptr)
        return;

    if (animationData->notifyStateTracks.empty())
        Add_NotifyStateTrack("Notify State");

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 minFrame = min(startFrame, endFrame);
    const int32 maxFrame = max(startFrame, endFrame);

    AnimNotifyStateEntry entry{};
    const int32 createdId = animationData->nextStateId++;
    entry.id = createdId;
    entry.startSec = static_cast<float>(minFrame) / static_cast<float>(fps);
    entry.durationSec = static_cast<float>(max(1, maxFrame - minFrame)) / static_cast<float>(fps);
    entry.trackIndex = std::clamp(_sequencerContextHasTrack && _sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyStateTrackIndex, 0, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    entry.notifyState = notifyStateInstance;

    animationData->notifyStates.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    _selectedStateId = createdId;
    _selectedNotifyStateTrackIndex = std::clamp(_sequencerContextHasTrack && _sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyStateTrackIndex, 0, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    _isDirty = true;
}

void Animation_View::Delete_SelectedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyId == -1)
        return;

    auto it = std::remove_if(
        animationData->notifies.begin(),
        animationData->notifies.end(),
        [this](const AnimNotifyEventEntry& entry)
        {
            return entry.id == _selectedNotifyId;
        });

    if (it != animationData->notifies.end())
    {
        animationData->notifies.erase(it, animationData->notifies.end());
        _selectedNotifyId = -1;
        _isDirty = true;
    }
}

void Animation_View::Delete_SelectedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedStateId == -1)
        return;

    auto it = std::remove_if(
        animationData->notifyStates.begin(),
        animationData->notifyStates.end(),
        [this](const AnimNotifyStateEntry& entry)
        {
            return entry.id == _selectedStateId;
        });

    if (it != animationData->notifyStates.end())
    {
        animationData->notifyStates.erase(it, animationData->notifyStates.end());
        _selectedStateId = -1;
        _isDirty = true;
    }
}

void Animation_View::Delete_SelectedNotifyTrack()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyTrackIndex < 0 || _selectedNotifyTrackIndex >= static_cast<int32>(animationData->notifyTracks.size()))
        return;

    Remove_NotifyTrackEntries(*animationData, _selectedNotifyTrackIndex);
    animationData->notifyTracks.erase(animationData->notifyTracks.begin() + _selectedNotifyTrackIndex);
    _selectedNotifyTrackIndex = animationData->notifyTracks.empty() ? 0 : min(_selectedNotifyTrackIndex, static_cast<int32>(animationData->notifyTracks.size()) - 1);
    _selectedNotifyId = -1;
    _isDirty = true;
}

void Animation_View::Delete_SelectedNotifyStateTrack()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyStateTrackIndex < 0 || _selectedNotifyStateTrackIndex >= static_cast<int32>(animationData->notifyStateTracks.size()))
        return;

    Remove_NotifyStateTrackEntries(*animationData, _selectedNotifyStateTrackIndex);
    animationData->notifyStateTracks.erase(animationData->notifyStateTracks.begin() + _selectedNotifyStateTrackIndex);
    _selectedNotifyStateTrackIndex = animationData->notifyStateTracks.empty() ? 0 : min(_selectedNotifyStateTrackIndex, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    _selectedStateId = -1;
    _isDirty = true;
}

void Animation_View::Handle_ClipboardShortcuts()
{
    if (!_isFocused)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        if (_selectedNotifyId != -1)
            Delete_SelectedNotify();
        else if (_selectedStateId != -1)
            Delete_SelectedState();
    }

    if (!io.KeyCtrl)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        if (_selectedNotifyId != -1)
            Copy_SelectedNotify();
        else if (_selectedStateId != -1)
            Copy_SelectedState();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        if (_notifyClipboardKind == NotifyClipboardKind::Notify)
            Paste_CopiedNotify();
        else if (_notifyClipboardKind == NotifyClipboardKind::State)
            Paste_CopiedState();
    }
}

void Animation_View::Copy_SelectedNotify()
{
    AnimNotifyEventEntry* selectedNotify = Find_SelectedNotify();
    if (selectedNotify == nullptr || selectedNotify->notify == nullptr)
        return;

    _notifyClipboardKind = NotifyClipboardKind::Notify;
    _copiedNotify = {};
    _copiedNotifyState = {};
    _copiedNotify.typeName = selectedNotify->notify->Get_TypeName();
    _copiedNotify.payload = selectedNotify->notify->To_Json();
    _copiedNotify.timeSec = selectedNotify->timeSec;
    _copiedNotify.trackIndex = selectedNotify->trackIndex;

    NOTIFY("노티파이 복사: {}", _copiedNotify.typeName);
}

void Animation_View::Copy_SelectedState()
{
    AnimNotifyStateEntry* selectedState = Find_SelectedState();
    if (selectedState == nullptr || selectedState->notifyState == nullptr)
        return;

    _notifyClipboardKind = NotifyClipboardKind::State;
    _copiedNotify = {};
    _copiedNotifyState = {};
    _copiedNotifyState.typeName = selectedState->notifyState->Get_TypeName();
    _copiedNotifyState.payload = selectedState->notifyState->To_Json();
    _copiedNotifyState.startSec = selectedState->startSec;
    _copiedNotifyState.durationSec = selectedState->durationSec;
    _copiedNotifyState.trackIndex = selectedState->trackIndex;

    NOTIFY("노티파이 스테이트 복사: {}", _copiedNotifyState.typeName);
}

void Animation_View::Paste_CopiedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _copiedNotify.typeName.empty())
        return;

    Shared<AnimNotify> notifyInstance = GAME->Create_AnimNotifyPrototype(_copiedNotify.typeName);
    if (notifyInstance == nullptr)
        return;

    notifyInstance->From_Json(_copiedNotify.payload);

    if (animationData->notifyTracks.empty())
        Add_NotifyTrack("Notify");

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 trackIndex = std::clamp(_selectedNotifyTrackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);

    AnimNotifyEventEntry entry{};
    const int32 createdId = animationData->nextNotifyId++;
    entry.id = createdId;
    entry.timeSec = static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(fps);
    entry.trackIndex = trackIndex;
    entry.notify = notifyInstance;

    animationData->notifies.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    _selectedNotifyId = createdId;
    _selectedStateId = -1;
    _selectedNotifyTrackIndex = trackIndex;
    _isDirty = true;

    NOTIFY("노티파이 붙여넣기: {}", _copiedNotify.typeName);
}

void Animation_View::Paste_CopiedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _copiedNotifyState.typeName.empty())
        return;

    Shared<AnimNotifyState> notifyStateInstance = GAME->Create_AnimNotifyStatePrototype(_copiedNotifyState.typeName);
    if (notifyStateInstance == nullptr)
        return;

    notifyStateInstance->From_Json(_copiedNotifyState.payload);

    if (animationData->notifyStateTracks.empty())
        Add_NotifyStateTrack("Notify State");

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 trackIndex = std::clamp(_selectedNotifyStateTrackIndex, 0, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);

    AnimNotifyStateEntry entry{};
    const int32 createdId = animationData->nextStateId++;
    entry.id = createdId;
    entry.startSec = static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(fps);
    entry.durationSec = max(_copiedNotifyState.durationSec, 1.f / static_cast<float>(fps));
    entry.trackIndex = trackIndex;
    entry.notifyState = notifyStateInstance;

    animationData->notifyStates.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    _selectedStateId = createdId;
    _selectedNotifyId = -1;
    _selectedNotifyStateTrackIndex = trackIndex;
    _isDirty = true;

    NOTIFY("노티파이 스테이트 붙여넣기: {}", _copiedNotifyState.typeName);
}

void Animation_View::Begin_SequencerContextMenu(int32 frame, bool hasTrackContext, bool isStateTrack, int32 trackIndex)
{
    _sequencerContextFrame = frame;
    _sequencerContextHasTrack = hasTrackContext;
    _sequencerContextIsStateTrack = isStateTrack;
    _sequencerContextTrackIndex = trackIndex;
    _openSequencerContextPopup = true;
}

bool Animation_View::Try_GetSequencerTrackContext(const ImVec2& sequencerRectMin, const ImVec2& mousePos, bool& outIsStateTrack, int32& outTrackIndex) const
{
    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return false;

    constexpr float sequencerHeaderHeight = 20.f;
    constexpr float rowHeight = 30.f;

    const float localY = mousePos.y - (sequencerRectMin.y + sequencerHeaderHeight);
    if (localY < 0.f)
        return false;

    const int32 rowIndex = static_cast<int32>(localY / rowHeight);
    const int32 notifyTrackCount = max(1, static_cast<int32>(animationData->notifyTracks.size()));
    const int32 stateHeaderRow = 1 + notifyTrackCount;
    const int32 stateTrackCount = max(1, static_cast<int32>(animationData->notifyStateTracks.size()));

    if (rowIndex > 0 && rowIndex < stateHeaderRow)
    {
        outIsStateTrack = false;
        outTrackIndex = std::clamp(rowIndex - 1, 0, notifyTrackCount - 1);
        return true;
    }

    if (rowIndex > stateHeaderRow && rowIndex <= stateHeaderRow + stateTrackCount)
    {
        outIsStateTrack = true;
        outTrackIndex = std::clamp(rowIndex - stateHeaderRow - 1, 0, stateTrackCount - 1);
        return true;
    }

    return false;
}

void Animation_View::Draw_CreateNotifyPopup()
{
    if (_openCreateNotifyPopup)
    {
        _openCreateNotifyPopup = false;
        ImGui::OpenPopup("CreateNotifyPopup");
    }

    if (!ImGui::BeginPopup("CreateNotifyPopup"))
        return;

    bool hasAny = false;
    GAME->Visit_AnimNotifyTypes([&](const TypeRegistry::AnimNotifyTypeInfo& info)
    {
        hasAny = true;
        if (ImGui::Selectable(info.className.c_str()))
        {
            Add_Notify_AtFrame(_requestedCreateNotifyFrame, info.className);
            ImGui::CloseCurrentPopup();
        }
    });

    if (!hasAny)
        ImGui::TextDisabled("No AnimNotify types registered.");

    ImGui::EndPopup();
}

void Animation_View::Draw_CreateNotifyStatePopup()
{
    if (_openCreateNotifyStatePopup)
    {
        _openCreateNotifyStatePopup = false;
        ImGui::OpenPopup("CreateNotifyStatePopup");
    }

    if (!ImGui::BeginPopup("CreateNotifyStatePopup"))
        return;

    bool hasAny = false;
    GAME->Visit_AnimNotifyStateTypes([&](const TypeRegistry::AnimNotifyStateTypeInfo& info)
    {
        hasAny = true;
        if (ImGui::Selectable(info.className.c_str()))
        {
            Add_State_ByFrameRange(_requestedCreateStateStartFrame, _requestedCreateStateEndFrame, info.className);
            ImGui::CloseCurrentPopup();
        }
    });

    if (!hasAny)
        ImGui::TextDisabled("No AnimNotifyState types registered.");

    ImGui::EndPopup();
}

void Animation_View::Draw_SequencerContextMenu()
{
    if (_openSequencerContextPopup)
    {
        _openSequencerContextPopup = false;
        ImGui::OpenPopup("AnimationSequencerContextMenu");
    }

    if (!ImGui::BeginPopup("AnimationSequencerContextMenu"))
        return;

    ImGui::TextDisabled("Frame: %d", _sequencerContextFrame);
    ImGui::Separator();

    const bool canAddNotify = _sequencerContextHasTrack ? !_sequencerContextIsStateTrack : true;
    const bool canAddState = _sequencerContextHasTrack ? _sequencerContextIsStateTrack : true;

    if (ImGui::MenuItem("Notify Add", nullptr, false, canAddNotify))
    {
        _requestedCreateNotifyFrame = _sequencerContextFrame;
        _openCreateNotifyPopup = true;
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::MenuItem("Notify State Add", nullptr, false, canAddState))
    {
        _requestedCreateStateStartFrame = _sequencerContextFrame;
        _requestedCreateStateEndFrame = min(Get_CurrentAnimationFrameMax(), _sequencerContextFrame + 12);
        _openCreateNotifyStatePopup = true;
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::BeginMenu("Track Add"))
    {
        if (ImGui::MenuItem("Notify"))
        {
            Add_NotifyTrack(_newNotifyTrackName);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Notify State"))
        {
            Add_NotifyStateTrack(_newNotifyStateTrackName);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}

Shared<ModelCom> Animation_View::Get_PlaybackModel() const
{
    return _liveModelCom;
}

void Animation_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
    width = max(1u, width);
    height = max(1u, height);

    if (_previewRenderTarget == nullptr)
    {
        _previewRenderTarget = RenderTarget::Create(
            L"RenderTarget",
            GAME->Get_Device(),
            GAME->Get_Context(),
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4(0.15f, 0.15f, 0.18f, 1.f),
            true);
        return;
    }

    _previewRenderTarget->Resize(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4(0.15f, 0.15f, 0.18f, 1.f),
        true);
}

void Animation_View::Ensure_PreviewCamera()
{
    if (_previewCamera != nullptr)
        return;

    _previewCamera = Editor_Camera_Free::Create(GAME->Get_Device(), GAME->Get_Context());
    if (_previewCamera == nullptr)
        return;

    Editor_Camera_Free::EDITOR_CAMERA_DESC desc = Build_PreviewCameraDesc(Get_PreviewRenderObject());

    if (FAILED(_previewCamera->Initialize(&desc)))
    {
        _previewCamera = nullptr;
        return;
    }

    _previewCamera->Apply_EditorDesc(desc);
    Apply_PreviewCameraTransform(_previewCamera);
}

Shared<GameObject> Animation_View::Get_PreviewRenderObject() const
{
    if (_liveModelCom != nullptr)
        return Resolve_RootPreviewObject(_liveModelCom->Get_Owner());

    return nullptr;
}

void Animation_View::Fit_PreviewCameraToModel()
{
    if (_previewCamera == nullptr)
        return;

    _previewCamera->Apply_EditorDesc(Build_PreviewCameraDesc(Get_PreviewRenderObject()));
    Apply_PreviewCameraTransform(_previewCamera);
}

void Animation_View::Ensure_PreviewGridResources()
{
    if (_previewGridBatch && _previewGridEffect && _previewGridInputLayout && _previewGridDepthState)
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
    }
}

void Animation_View::Draw_PreviewGrid() const
{
    if (!_showPreviewGrid)
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewGridDepthState)
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewGridDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    _previewGridBatch->Begin();

    const Vec3 gridCenter = Vec3::Zero;

    constexpr int gridDivs = 40;
    constexpr float gridHalfExtent = 20.f;
    const float step = (gridHalfExtent * 2.f) / static_cast<float>(gridDivs);
    const float y = 0.f;

    const XMVECTOR gridColor = XMVectorSet(0.58f, 0.58f, 0.62f, 1.f);
    const XMVECTOR xAxisColor = XMVectorSet(0.95f, 0.32f, 0.32f, 1.f);
    const XMVECTOR zAxisColor = XMVectorSet(0.28f, 0.72f, 1.f, 1.f);

    for (int i = 0; i <= gridDivs; ++i)
    {
        const float offset = -gridHalfExtent + step * static_cast<float>(i);

        const XMVECTOR colorX = (fabsf(offset) < 0.0001f) ? zAxisColor : gridColor;
        const XMVECTOR colorZ = (fabsf(offset) < 0.0001f) ? xAxisColor : gridColor;

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

string Animation_View::Normalize_DefaultTrackName(const string& trackName)
{
    if (trackName == "노티파이")
        return "Notify";
    if (trackName == "노티파이 스테이트")
        return "Notify State";
    if (trackName == "노티파이 트랙")
        return "Notify Track";
    if (trackName == "노티파이 스테이트 트랙")
        return "Notify State Track";

    return trackName;
}

string Animation_View::Make_AnimationComboLabel(const AnimationEntry& entry)
{
    return entry.displayName + "##" + entry.name;
}

const Animation_View::AnimationEntry* Animation_View::Find_AnimationEntryByAnyName(
    const vector<AnimationEntry>& entries,
    const string& name)
{
    for (const auto& entry : entries)
    {
        if (entry.name == name || entry.displayName == name || entry.playbackName == name)
            return &entry;
    }

    return nullptr;
}

int32 Animation_View::Find_AnimationIndex(const Shared<ModelCom>& modelCom, const string& animationName)
{
    if (modelCom == nullptr || animationName.empty())
        return -1;

    const uint32 numAnimations = static_cast<uint32>(modelCom->Get_NumAnimations());
    for (uint32 i = 0; i < numAnimations; ++i)
    {
        const char* currentName = modelCom->Get_AnimationName(i);
        if (currentName != nullptr && animationName == currentName)
            return static_cast<int32>(i);
    }

    return -1;
}

float Animation_View::Resolve_AnimNotifyDataLengthSec(const AnimNotifyData* animationData)
{
    if (animationData == nullptr)
        return 5.f;

    float maxTimeSec = 0.f;

    for (const auto& entry : animationData->notifies)
        maxTimeSec = max(maxTimeSec, entry.timeSec);

    for (const auto& entry : animationData->notifyStates)
        maxTimeSec = max(maxTimeSec, entry.startSec + entry.durationSec);

    return max(5.f, maxTimeSec);
}

Shared<GameObject> Animation_View::Resolve_RootPreviewObject(const Shared<GameObject>& gameObject)
{
    Shared<GameObject> rootObject = gameObject;
    while (rootObject != nullptr)
    {
        Shared<GameObject> parentObject = rootObject->Get_Owner();
        if (parentObject == nullptr)
            break;

        rootObject = parentObject;
    }

    return rootObject;
}

void Animation_View::Lock_PreviewObjectToOrigin(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return;

    Shared<TransformCom> transformCom = gameObject->Get_Transform();
    if (transformCom == nullptr)
        return;

    transformCom->Set_WorldPosition(Vec3::Zero);
}

bool Animation_View::Is_PrefabViewFocused()
{
    Shared<Editor_Window> prefabWindow = EDITOR->Get_Window(L"Prefab");
    Shared<Prefab_View> prefabView = dynamic_pointer_cast<Prefab_View>(prefabWindow);
    if (prefabView != nullptr && prefabView->Is_Open() && prefabView->Is_Focused())
        return true;

    Shared<Editor_Window> animStateWindow = EDITOR->Get_Window(L"AnimState");
    return animStateWindow != nullptr && animStateWindow->Is_Open() && animStateWindow->Is_Focused();
}

Editor_Camera_Free::EDITOR_CAMERA_DESC Animation_View::Build_PreviewCameraDesc(const Shared<GameObject>& gameObject)
{
    UNREFERENCED_PARAMETER(gameObject);

    Editor_Camera_Free::EDITOR_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(1.15f, 1.35f, 5.2f, 1.f);
    desc.at = XMFLOAT4(0.25f, 1.0f, 0.f, 1.f);
    desc.fovy = XM_PIDIV4;
    desc.nearPlane = 0.1f;
    desc.farPlane = 100.f;
    desc.yaw = 0.f;
    desc.pitch = 0.f;
    desc.mouseSensor = 0.15f;
    desc.speedPerSec = 5.f;
    desc.degreePerSec = 180.f;

    return desc;
}

void Animation_View::Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera)
{
    if (camera == nullptr)
        return;

    Shared<TransformCom> cameraTransform = camera->Get_Transform();
    if (cameraTransform == nullptr)
        return;

    cameraTransform->Set_WorldPosition(Vec3(-1.35f, 2.25f, 6.2f));
    cameraTransform->LookAt(Vec3(-1.6f, -1.2f, -2.3f));
}

void Animation_View::Remove_NotifyTrackEntries(AnimNotifyData& animationData, int32 trackIndex)
{
    animationData.notifies.erase(
        remove_if(
            animationData.notifies.begin(),
            animationData.notifies.end(),
            [trackIndex](const AnimNotifyEventEntry& entry)
            {
                return entry.trackIndex == trackIndex;
            }),
        animationData.notifies.end());

    for (auto& entry : animationData.notifies)
    {
        if (entry.trackIndex > trackIndex)
            --entry.trackIndex;
    }
}

void Animation_View::Remove_NotifyStateTrackEntries(AnimNotifyData& animationData, int32 trackIndex)
{
    animationData.notifyStates.erase(
        remove_if(
            animationData.notifyStates.begin(),
            animationData.notifyStates.end(),
            [trackIndex](const AnimNotifyStateEntry& entry)
            {
                return entry.trackIndex == trackIndex;
            }),
        animationData.notifyStates.end());

    for (auto& entry : animationData.notifyStates)
    {
        if (entry.trackIndex > trackIndex)
            --entry.trackIndex;
    }
}

void Animation_View::Sync_PreviewAnimationSelection(bool keepCurrentTime)
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    const string playbackName = currentEntry ? currentEntry->playbackName : _currentAnimationName;
    const int32 animationIndex = Find_AnimationIndex(playbackModel, playbackName);
    if (animationIndex < 0)
        return;

    const float currentTimeSec = keepCurrentTime ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;
    playbackModel->Set_Animation(static_cast<uint32>(animationIndex), true);
    playbackModel->Set_AnimationPaused(false);
    playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
    playbackModel->Refresh_CurrentAnimationPose();
    _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
}

AnimNotifyData* Animation_View::Get_CurrentAnimation()
{
    if (_currentAnimationName.empty())
        return nullptr;

    if (AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName))
        return animation;

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    if (currentEntry == nullptr)
        return nullptr;

    return Find_OrNormalize_NotifyAnimation(_asset, *currentEntry);
}

const AnimNotifyData* Animation_View::Get_CurrentAnimation() const
{
    if (_currentAnimationName.empty())
        return nullptr;

    if (const AnimNotifyData* animation = AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName))
        return animation;

    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    if (currentEntry == nullptr)
        return nullptr;

    return Find_NotifyAnimation(_asset, *currentEntry);
}

AnimNotifyEventEntry* Animation_View::Find_SelectedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyId == -1)
        return nullptr;

    for (auto& entry : animationData->notifies)
    {
        if (entry.id == _selectedNotifyId)
            return &entry;
    }

    return nullptr;
}

AnimNotifyStateEntry* Animation_View::Find_SelectedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedStateId == -1)
        return nullptr;

    for (auto& entry : animationData->notifyStates)
    {
        if (entry.id == _selectedStateId)
            return &entry;
    }

    return nullptr;
}

Shared<Animation_View> Animation_View::Create()
{
    return make_shared<Animation_View>();
}
