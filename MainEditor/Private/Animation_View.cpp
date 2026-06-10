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
#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "Inspector.h"
#include "ModelCom.h"
#include "Prefab_View.h"
#include "PartObject.h"
#include "PreviewObjectHelpers.h"
#include "RenderTarget.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "imgui_internal.h"
#include "Notification_Manager.h"
#include "Weapon.h"

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

    float Resolve_TrailDrainDuration(const Shared<const EffectDefinition>& definition)
    {
        constexpr float fallbackDuration = 0.25f;
        if (definition == nullptr)
            return fallbackDuration;

        float duration = fallbackDuration;
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled || emitter.kind != EffectEmitterKind::Trail)
                continue;

            const ComputeTrailEmitterDesc* trailDesc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc);
            if (trailDesc == nullptr)
                continue;

            duration = max(duration, trailDesc->segmentLifetime);
        }

        return max(0.001f, duration);
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

    bool Is_PreviewNotifyTimeInRange(float targetTime, float previousTime, float currentTime, bool wrapped)
    {
        if (!wrapped)
        {
            if (previousTime <= 1e-4f && targetTime <= 1e-4f)
                return targetTime <= currentTime;

            return previousTime < targetTime && targetTime <= currentTime;
        }

        return targetTime > previousTime || targetTime <= currentTime;
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

    if (!Is_Visible())
        return;

    if (Should_DeferToCompanionPreview())
        return;

    if (_previewCamera && _isPreviewHovered)
        _previewCamera->Priority_Update(timeDelta);

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    Validate_SelectedBoneSelection();

    if (!playbackModel->Is_AnimationPaused())
    {
        if (playbackModel->Is_AnimationFinished())
        {
            // 끝난 클립은 현재 선택된 애니메이션으로 즉시 다시 시작해서 다음 클립으로 넘어가지 않게 한다.
            Sync_PreviewAnimationSelection();
        }

        _sequencerCurrentFrame = static_cast<int32>(std::round(playbackModel->Get_CurrentAnimationTimeSec() * static_cast<float>(Get_CurrentAnimationFps())));
    }
}

void Animation_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (EDITOR->IsPlaying())
        return;

    if (!Is_Visible())
        return;

    if (Should_DeferToCompanionPreview())
        return;

    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    if (previewRenderObject == nullptr || _previewCamera == nullptr)
        return;

    const Matrix savedView = *GAME->Get_Transform(D3DTS::View);
    const Matrix savedProj = *GAME->Get_Transform(D3DTS::Proj);
    const Shared<Camera> savedActiveCamera = GAME->Get_ActiveCamera();
    const uint32 savedViewportWidth = GAME->Get_ViewportWidth();
    const uint32 savedViewportHeight = GAME->Get_ViewportHeight();
    vector<LIGHT_DESC> savedLightDescs{};
    GAME->Get_LightDescs(savedLightDescs);
    const bool wasPlayerInputBlocked = GAME->Is_PlayerInputBlocked();

    EDITOR->Apply_PreviewViewportSizeImmediate(_previewWidth, _previewHeight);

    GAME->Bind_BackBuffer();
    if (FAILED(GAME->Clear_Buffers(Vec4(0.f, 0.f, 0.f, 1.f))))
    {
        EDITOR->Apply_PreviewViewportSizeImmediate(savedViewportWidth, savedViewportHeight);
        return;
    }

    const Matrix previewView = _previewCamera->Get_ViewMatrix();
    const float aspect = static_cast<float>(_previewWidth) / static_cast<float>(max(1u, _previewHeight));
    const Matrix previewProj = XMMatrixPerspectiveFovLH(_previewCamera->Get_FovY(), aspect, _previewCamera->Get_Near(), _previewCamera->Get_Far());

    GAME->Change_ActiveCamera(_previewCamera);
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

    const float dt = min(ImGui::GetIO().DeltaTime, 1.f / 30.f);
    Lock_PreviewObjectToOrigin(previewRenderObject);

    if (_sourceMode == SourceMode::LiveModel)
    {
        const float previousTimeSec = _previewModelCom ? _previewModelCom->Get_CurrentAnimationTimeSec() : 0.f;
        GAME->Backup_RenderGroup();
        PreviewObjectHelpers::Update_PreviewObject(previewRenderObject, dt);
        const float currentTimeSec = _previewModelCom ? _previewModelCom->Get_CurrentAnimationTimeSec() : previousTimeSec;
        Update_PreviewPlayEffectNotifies(previousTimeSec, currentTimeSec, dt);
        PreviewObjectHelpers::Tick_AttachedPreviewEffects(previewRenderObject, dt);
        Update_PreviewWeaponRotateStates(dt); // preview 전용 WeaponRotate를 먼저 반영하고 무기 world를 다시 맞춘 뒤 trail sample을 계산한다.
        Update_PreviewTrailEffect(dt);
    }
    else
    {
        GAME->Backup_RenderGroup();
    }

    for (PreviewTrailRuntime& runtime : _previewTrailRuntimes)
    {
        if (runtime.effect && runtime.isPlaying)
            runtime.effect->Late_Update(dt); // preview에서도 활성 trail runtime마다 Late_Update를 따로 돌린다.
    }

    GAME->Draw_RendererOnly();
    GAME->Restore_RenderGroup();
    Ensure_PreviewGridResources();
    Draw_SelectedBoneMarker();
    Draw_TrailAnchorPreview();
    Draw_PreviewGrid();
    EDITOR->Capture_BackBuffer_ToPreview();
    GAME->Set_Lights(savedLightDescs);
    GAME->Set_PlayerInputBlocked(wasPlayerInputBlocked);

    GAME->Bind_BackBuffer();
    if (savedActiveCamera != nullptr)
        GAME->Change_ActiveCamera(savedActiveCamera);
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
    GAME->UpdateFrustum(savedView, savedProj);
    EDITOR->Apply_PreviewViewportSizeImmediate(savedViewportWidth, savedViewportHeight);
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
    _isVisible = false;

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isVisible = true;
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

    if (!isOpen)
    {
        Reset_PreviewPlayEffectStates();
        Reset_PreviewWeaponRotateStates();
        Reset_PreviewTrailEffect();
    }
}

bool Animation_View::CanSave() const
{
    return !_modelGuid.empty() && !_currentAnimationName.empty();
}

void Animation_View::Save()
{
    Save_NotifyAsset();
    Save_TrailAnchorAsset();
}

void Animation_View::Open_FromAsset(const wstring& modelPath, const string& modelGuid, const string& preferredAnimationName, uint32 preferredDockId)
{
    Reset_PreviewTrailEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewClone();

    _sourceMode = SourceMode::None;
    _sourceModelPath = modelPath;
    _liveModelCom.reset();
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = true;

    _modelGuid = modelGuid;
    _currentAnimationName = preferredAnimationName;
    Clear_SelectedNotifySelection();
    _selectedStateId = -1;
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerFirstFrame = 0;
    _isDirty = false;
    _animationSearchBuffer[0] = '\0';
    _boneSearchBuffer[0] = '\0';
    _selectedBoneIndex = -1;

    Refresh_Source();
    Load_NotifyAsset();
    Load_TrailAnchorAsset();
    Set_Open(true);
}

void Animation_View::Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName, uint32 preferredDockId, bool focusOnOpen)
{
    const wstring sourceModelPath = modelCom ? modelCom->Get_SourceModelPath() : wstring{};

    Reset_PreviewTrailEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewClone();

    _sourceMode = SourceMode::LiveModel;
    _liveModelCom = modelCom;
    _sourceModelPath = sourceModelPath;
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = focusOnOpen;

    _modelGuid = _sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _currentAnimationName = preferredAnimationName;
    Clear_SelectedNotifySelection();
    _selectedStateId = -1;
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerFirstFrame = 0;
    _isDirty = false;
    _animationSearchBuffer[0] = '\0';
    _boneSearchBuffer[0] = '\0';
    _selectedBoneIndex = -1;

    Refresh_Source();
    Rebuild_PreviewClone();
    Load_NotifyAsset();
    Load_TrailAnchorAsset();
    Ensure_PreviewCamera();
    Fit_PreviewCameraToModel();
    Sync_PreviewAnimationSelection(true);

    Set_Open(true);
}

void Animation_View::Clear_LivePreviewBinding(const Shared<GameObject>& previewRoot)
{
    if (_sourceMode != SourceMode::LiveModel)
        return;

    if (previewRoot != nullptr)
    {
        const Shared<GameObject> currentPreviewRoot =
            _liveModelCom ? Resolve_RootPreviewObject(_liveModelCom->Get_Owner()) : nullptr;
        if (currentPreviewRoot != previewRoot)
            return;
    }

    Reset_PreviewTrailEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewClone();

    _sourceMode = SourceMode::None;
    _sourceModelPath.clear();
    _liveModelCom.reset();
    _modelGuid.clear();
    _currentAnimationName.clear();
    _availableAnimations.clear();
    Clear_SelectedNotifySelection();
    _selectedStateId = -1;
    _selectedNotifyTrackIndex = 0;
    _selectedNotifyStateTrackIndex = 0;
    _sequencerCurrentFrame = 0;
    _sequencerSelectedRow = -1;
    _sequencerFirstFrame = 0;
    _selectedBoneIndex = -1;
}

void Animation_View::Rebuild_PreviewClone()
{
    Reset_PreviewClone();

    if (_liveModelCom == nullptr)
        return;

    const Shared<GameObject> liveOwnerObject = _liveModelCom->Get_Owner();
    const Shared<GameObject> liveRootObject = Resolve_RootPreviewObject(liveOwnerObject);
    if (liveOwnerObject == nullptr || liveRootObject == nullptr)
        return;

    if (!PreviewObjectHelpers::Try_BuildGameObjectKey(liveRootObject, liveOwnerObject, _previewOwnerObjectKey))
        return;

    _previewRootObject = GAME->Instantiate_GameObjectJson(liveRootObject->To_Json());
    if (_previewRootObject == nullptr)
        return;

    PreviewObjectHelpers::Disable_PreviewAnimNotifyExecution(_previewRootObject);
    PreviewObjectHelpers::Prime_PreviewObject(_previewRootObject);
    Lock_PreviewObjectToOrigin(_previewRootObject);

    const Shared<GameObject> previewOwnerObject =
        PreviewObjectHelpers::Find_GameObjectByKey(_previewRootObject, _previewOwnerObjectKey);
    _previewModelCom =
        PreviewObjectHelpers::Find_ModelComponentBySourcePath(previewOwnerObject, _sourceModelPath);
}

void Animation_View::Reset_PreviewClone()
{
    Reset_PreviewPlayEffectStates();
    _previewRootObject.reset();
    _previewModelCom.reset();
    _previewOwnerObjectKey.clear();
}

int32 Animation_View::Get_CurrentAnimationFps() const
{
    if (const AnimNotifyData* animationData = Get_CurrentAnimation())
        return max(1, animationData->displayFps);

    return 60;
}

void Animation_View::Refresh_PreviewTrailAnchorAsset()
{
    Reset_PreviewTrailEffect(); // 현재 활성 runtime들을 비우면 다음 preview frame에 각 trail이 자기 partTag 기준으로 다시 생성된다.
}

float Animation_View::Get_CurrentAnimationLengthSec() const
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel != nullptr)
        return playbackModel->Get_CurrentAnimationDurationSec();

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

    AnimNotify_Parser::Load_ModelAsset(_modelGuid, _asset);

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

void Animation_View::Load_TrailAnchorAsset()
{
    _trailAnchorAsset = {};

    if (_modelGuid.empty())
        return;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(_modelGuid);
    (void)Client::TrailAnchor_Parser::Load_FromFile(filePath.wstring(), _trailAnchorAsset);

    if (_trailAnchorAsset.modelGuid.empty())
        _trailAnchorAsset.modelGuid = _modelGuid;
}

bool Animation_View::Load_PreviewTrailAnchorAsset(
    const string& partTag,
    PreviewTrailRuntime& outRuntime)
{
    outRuntime.trailAnchorAsset = {};
    outRuntime.modelGuid.clear();

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewTrailPartTag = String::ToWString(partTag);
    Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(previewTrailPartTag));
    if (weaponPart == nullptr)
        return false;

    const Shared<ModelCom> weaponModel = weaponPart->Get_Model();
    if (weaponModel == nullptr)
        return false;

    const wstring& sourceModelPath = weaponModel->Get_SourceModelPath();
    if (sourceModelPath.empty())
        return false;

    outRuntime.modelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
    if (outRuntime.modelGuid.empty())
        return false;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(outRuntime.modelGuid);
    (void)Client::TrailAnchor_Parser::Load_FromFile(filePath.wstring(), outRuntime.trailAnchorAsset);

    if (outRuntime.trailAnchorAsset.modelGuid.empty())
        outRuntime.trailAnchorAsset.modelGuid = outRuntime.modelGuid;

    return !outRuntime.trailAnchorAsset.modelGuid.empty();
}

void Animation_View::Save_NotifyAsset()
{
    if (_modelGuid.empty())
        return;

    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const float animationLengthSec = Get_CurrentAnimationLengthSec();
    AnimNotify_Parser::Sort_And_Clamp(*animationData, animationLengthSec);

    if (AnimNotify_Parser::Save_AnimationToSplitFile(_modelGuid, *animationData, animationLengthSec))
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

void Animation_View::Save_TrailAnchorAsset() const
{
    if (_modelGuid.empty())
        return;

    Client::TrailAnchorAsset asset = _trailAnchorAsset;
    asset.modelGuid = _modelGuid;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(_modelGuid);
    (void)Client::TrailAnchor_Parser::Save_ToFile(filePath.wstring(), asset);
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
    if (!ImGui::BeginTabBar("AnimationLeftTabs"))
        return;

    if (ImGui::BeginTabItem("Animation Clips"))
    {
        ImGui::Text("Animation Clips");
        ImGui::Separator();
        ImGui::InputTextWithHint("##AnimationClipSearch", "Search animation...", _animationSearchBuffer, IM_ARRAYSIZE(_animationSearchBuffer));
        const string searchTextLower = String::ToLowerCopy(string(_animationSearchBuffer));

        if (ImGui::BeginChild("AnimationClipBrowserPanel", ImVec2(0.f, 0.f), true))
        {
            if (_availableAnimations.empty())
            {
                ImGui::TextDisabled("No clips found.");
            }
            else
            {
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
                        Clear_SelectedNotifySelection();
                        _selectedStateId = -1;
                        _selectedNotifyTrackIndex = 0;
                        _selectedNotifyStateTrackIndex = 0;
                        _sequencerCurrentFrame = 0;
                        _sequencerSelectedRow = -1;
                        _sequencerFirstFrame = 0;

                        Sync_PreviewAnimationSelection();

                        Shared<ModelCom> playbackModel = Get_PlaybackModel();
                        if (_sourceMode == SourceMode::LiveModel && playbackModel != nullptr)
                        {
                            const int32 animationIndex = Find_AnimationIndex(playbackModel, entry.playbackName);
                            if (animationIndex >= 0)
                            {
                                playbackModel->Set_Animation(static_cast<uint32>(animationIndex), true);
                                playbackModel->Set_AnimationPaused(false);
                                playbackModel->Set_CurrentAnimationTimeSec(0.f);
                                playbackModel->Refresh_CurrentAnimationPose();
                            }
                        }
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Bone Tree"))
    {
        Draw_BoneTree();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

void Animation_View::Draw_BoneTree()
{
    ImGui::Text("Bone Tree");
    ImGui::Separator();
    ImGui::InputTextWithHint("##BoneSearch", "Search bone...", _boneSearchBuffer, IM_ARRAYSIZE(_boneSearchBuffer));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 10.f);

    if (!ImGui::BeginChild("AnimationBoneTreePanel", ImVec2(0.f, 0.f), true))
    {
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
    {
        ImGui::TextDisabled("Bone tree is available in live preview mode.");
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    Validate_SelectedBoneSelection();

    const uint32 numBones = static_cast<uint32>(playbackModel->Get_NumBones());
    if (numBones == 0)
    {
        ImGui::TextDisabled("No bones found.");
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    vector<vector<uint32>> children(numBones);
    vector<uint32> rootBones{};
    rootBones.reserve(numBones);

    for (uint32 boneIndex = 0; boneIndex < numBones; ++boneIndex)
    {
        const int32 parentIndex = playbackModel->Get_BoneParentIndex(boneIndex);
        if (parentIndex >= 0 && parentIndex < static_cast<int32>(numBones))
            children[parentIndex].push_back(boneIndex);
        else
            rootBones.push_back(boneIndex);
    }

    const string filterLower = String::ToLowerCopy(string(_boneSearchBuffer));

    const auto branchMatches =
        [&](const auto& self, uint32 boneIndex) -> bool
    {
        const string boneNameLower = String::ToLowerCopy(playbackModel->Get_BoneName(boneIndex));
        if (filterLower.empty() || boneNameLower.find(filterLower) != string::npos)
            return true;

        for (uint32 childIndex : children[boneIndex])
        {
            if (self(self, childIndex))
                return true;
        }

        return false;
    };

    const auto renderNode =
        [&](const auto& self, uint32 boneIndex) -> void
    {
        if (!filterLower.empty() && !branchMatches(branchMatches, boneIndex))
            return;

        const bool isSelected = (_selectedBoneIndex == static_cast<int32>(boneIndex));
        const bool forceOpen = !filterLower.empty() && !children[boneIndex].empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children[boneIndex].empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (forceOpen)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        else if (!children[boneIndex].empty())
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        const string nodeLabel =
            playbackModel->Get_BoneName(boneIndex) + " [" + to_string(boneIndex) + "]";
        const string popupId = "AnimationBoneContextMenu_" + to_string(boneIndex);
        ImGui::PushID(static_cast<int>(boneIndex));
        const bool opened = ImGui::TreeNodeEx("BoneNode", flags, "%s", nodeLabel.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            _selectedBoneIndex = static_cast<int32>(boneIndex);
            Clear_SelectedNotifySelection();
            _selectedStateId = -1;
        }

        if (ImGui::BeginPopupContextItem(popupId.c_str()))
        {
            if (ImGui::MenuItem("뼈 이름 복사"))
            {
                ImGui::SetClipboardText(playbackModel->Get_BoneName(boneIndex).c_str());
                _selectedBoneIndex = static_cast<int32>(boneIndex);
                Clear_SelectedNotifySelection();
                _selectedStateId = -1;
            }

            if (ImGui::MenuItem("Trail Base로 지정"))
            {
                _selectedBoneIndex = static_cast<int32>(boneIndex);
                Clear_SelectedNotifySelection();
                _selectedStateId = -1;
                Assign_SelectedBoneToTrailAnchor(playbackModel, TrailAnchorSlot::Base);
            }

            if (ImGui::MenuItem("Trail Tip으로 지정"))
            {
                _selectedBoneIndex = static_cast<int32>(boneIndex);
                Clear_SelectedNotifySelection();
                _selectedStateId = -1;
                Assign_SelectedBoneToTrailAnchor(playbackModel, TrailAnchorSlot::Tip);
            }

            ImGui::EndPopup();
        }

        if (!opened)
        {
            ImGui::PopID();
            return;
        }

        for (uint32 childIndex : children[boneIndex])
            self(self, childIndex);

        ImGui::TreePop();
        ImGui::PopID();
    };

    for (uint32 rootBoneIndex : rootBones)
        renderNode(renderNode, rootBoneIndex);

    ImGui::PopStyleVar();
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
            playbackModel->Play_Animation(0.f); // preview scrub/stop에서도 ANS state 갱신까지 함께 반영한다.
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
            playbackModel->Play_Animation(0.f); // 시간은 고정한 채 notify/state만 현재 위치 기준으로 다시 평가한다.
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

    ID3D11ShaderResourceView* previewSRV = EDITOR->Get_SharedPreviewSRV();
    if (previewRenderObject != nullptr && playbackModel != nullptr && previewSRV != nullptr)
    {
        ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSRV));
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
            playbackModel->Play_Animation(0.f); // 시퀀서 프레임 점프에서도 ANS_WeaponRotate 같은 state notify를 같이 갱신한다.
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
    Shared<ModelCom> playbackModel = Get_PlaybackModel();

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
    else if (playbackModel != nullptr && _selectedBoneIndex >= 0)
    {
        Draw_SelectedBoneDetail(playbackModel);
    }
    else
    {
        ImGui::TextDisabled("선택된 항목 없음");
        ImGui::Spacing();
        ImGui::TextWrapped("Notify, NotifyState, 또는 Bone을 선택하세요.");
    }

    ImGui::EndChild();
}

void Animation_View::Draw_SelectedBoneDetail(const Shared<ModelCom>& playbackModel)
{
    if (playbackModel == nullptr || _selectedBoneIndex < 0)
    {
        ImGui::TextDisabled("No bone selected.");
        return;
    }

    const uint32 boneIndex = static_cast<uint32>(_selectedBoneIndex);
    if (boneIndex >= playbackModel->Get_NumBones())
    {
        ImGui::TextDisabled("Selected bone is out of range.");
        return;
    }

    const int32 parentIndex = playbackModel->Get_BoneParentIndex(boneIndex);

    ImGui::Text("Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", playbackModel->Get_BoneName(boneIndex).c_str());

    ImGui::Spacing();
    ImGui::Text("Index");
    ImGui::SameLine();
    ImGui::TextDisabled("%d", _selectedBoneIndex);

    ImGui::Text("Parent");
    ImGui::SameLine();
    if (parentIndex >= 0 && parentIndex < static_cast<int32>(playbackModel->Get_NumBones()))
    {
        ImGui::TextDisabled(
            "%s [%d]",
            playbackModel->Get_BoneName(static_cast<uint32>(parentIndex)).c_str(),
            parentIndex);
    }
    else
    {
        ImGui::TextDisabled("(Root)");
    }

    ImGui::Spacing();
    Draw_TrailAnchorPanel();
}

void Animation_View::Draw_TrailAnchorPanel()
{
    ImGui::Separator();
    ImGui::Text("Trail Anchors");

    ImGui::Text("Base Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _trailAnchorAsset.base.boneName.empty() ? "(None)" : _trailAnchorAsset.base.boneName.c_str());

    ImGui::Text("Tip Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _trailAnchorAsset.tip.boneName.empty() ? "(None)" : _trailAnchorAsset.tip.boneName.c_str());

    Vec3 baseLocal = _trailAnchorAsset.base.localPosition;
    if (ImGui::DragFloat3("Base Local", &baseLocal.x, 0.5f))
    {
        _trailAnchorAsset.base.localPosition = baseLocal;
        MarkDirty();
    }

    Vec3 tipLocal = _trailAnchorAsset.tip.localPosition;
    if (ImGui::DragFloat3("Tip Local", &tipLocal.x, 0.5f))
    {
        _trailAnchorAsset.tip.localPosition = tipLocal;
        MarkDirty();
    }
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
            Clear_SelectedNotifySelection();
        }

        for (const auto& entry : animationData->notifies)
        {
            if (!entry.notify || entry.trackIndex != trackIndex)
                continue;

            const int32 frame = static_cast<int32>(std::round(entry.timeSec * fps));
            const string label = entry.notify->Get_TypeName() + " (" + to_string(frame) + ")";
            const bool isSelected = Is_NotifySelected(entry.id);
            if (ImGui::Selectable((label + "##NotifyEntry" + to_string(entry.id)).c_str(), isSelected))
            {
                const bool ctrlPressed = ImGui::GetIO().KeyCtrl;

                if (ctrlPressed)
                {
                    if (Is_NotifySelected(entry.id))
                    {
                        _selectedNotifyIds.erase(entry.id);
                        if (_selectedNotifyId == entry.id)
                            _selectedNotifyId = _selectedNotifyIds.empty() ? -1 : *_selectedNotifyIds.begin();
                    }
                    else
                    {
                        _selectedNotifyIds.insert(entry.id);
                        _selectedNotifyId = entry.id;
                    }
                }
                else
                {
                    _selectedNotifyIds.clear();
                    _selectedNotifyIds.insert(entry.id);
                    _selectedNotifyId = entry.id;
                }

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
            Clear_SelectedNotifySelection();
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
                Clear_SelectedNotifySelection();
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
            Clear_SelectedNotifySelection();
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
            Clear_SelectedNotifySelection();
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
    _selectedNotifyIds.clear();
    _selectedNotifyIds.insert(createdId);
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
    if (animationData == nullptr || _selectedNotifyIds.empty())
        return;

    auto it = std::remove_if(
        animationData->notifies.begin(),
        animationData->notifies.end(),
        [this](const AnimNotifyEventEntry& entry)
        {
            return Is_NotifySelected(entry.id);
        });

    if (it != animationData->notifies.end())
    {
        animationData->notifies.erase(it, animationData->notifies.end());
        Clear_SelectedNotifySelection();
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
    Clear_SelectedNotifySelection();
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
        if (!_selectedNotifyIds.empty())
            Delete_SelectedNotify();
        else if (_selectedStateId != -1)
            Delete_SelectedState();
    }

    if (!_selectedNotifyIds.empty() && !io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
            Move_SelectedNotifiesByFrame(-1);
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
            Move_SelectedNotifiesByFrame(1);
    }

    if (!io.KeyCtrl)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        if (!_selectedNotifyIds.empty())
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

void Animation_View::Move_SelectedNotifiesByFrame(int32 frameDelta)
{
    if (frameDelta == 0)
        return;

    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyIds.empty())
        return;

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 maxNotifyFrame = max(0, Get_CurrentAnimationFrameMax() - 1);

    vector<pair<AnimNotifyEventEntry*, int32>> selectedEntries{};
    selectedEntries.reserve(_selectedNotifyIds.size());

    int32 minFrame = maxNotifyFrame;
    int32 maxFrame = 0;
    for (AnimNotifyEventEntry& entry : animationData->notifies)
    {
        if (!Is_NotifySelected(entry.id))
            continue;

        const int32 frame = std::clamp(
            static_cast<int32>(std::round(entry.timeSec * static_cast<float>(fps))),
            0,
            maxNotifyFrame);

        selectedEntries.emplace_back(&entry, frame);
        minFrame = min(minFrame, frame);
        maxFrame = max(maxFrame, frame);
    }

    if (selectedEntries.empty())
        return;

    const int32 clampedDelta = std::clamp(frameDelta, -minFrame, maxNotifyFrame - maxFrame);
    if (clampedDelta == 0)
        return;

    float primaryTimeSec = -1.f;
    for (const auto& [entry, frame] : selectedEntries)
    {
        const int32 movedFrame = frame + clampedDelta;
        entry->timeSec = static_cast<float>(movedFrame) / static_cast<float>(fps);

        if (entry->id == _selectedNotifyId)
            primaryTimeSec = entry->timeSec;
    }

    if (primaryTimeSec < 0.f)
        primaryTimeSec = selectedEntries.front().first->timeSec;

    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    if (primaryTimeSec >= 0.f)
    {
        _sequencerCurrentFrame = std::clamp(
            static_cast<int32>(std::round(primaryTimeSec * static_cast<float>(fps))),
            0,
            maxNotifyFrame);

        Shared<ModelCom> playbackModel = Get_PlaybackModel();
        if (_sourceMode == SourceMode::LiveModel && playbackModel != nullptr)
        {
            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(primaryTimeSec);
            playbackModel->Refresh_CurrentAnimationPose();
        }
    }

    _isDirty = true;
}

void Animation_View::Copy_SelectedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyIds.empty())
        return;

    _notifyClipboardKind = NotifyClipboardKind::Notify;
    _copiedNotifies.clear();
    _copiedNotifyState = {};

    for (const AnimNotifyEventEntry& entry : animationData->notifies)
    {
        if (!Is_NotifySelected(entry.id) || entry.notify == nullptr)
            continue;

        CopiedNotifyEntry copiedEntry{};
        copiedEntry.typeName = entry.notify->Get_TypeName();
        copiedEntry.payload = entry.notify->To_Json();
        copiedEntry.timeSec = entry.timeSec;
        copiedEntry.trackIndex = entry.trackIndex;
        _copiedNotifies.emplace_back(std::move(copiedEntry));
    }

    if (_copiedNotifies.empty())
        return;

    std::sort(_copiedNotifies.begin(), _copiedNotifies.end(),
        [](const CopiedNotifyEntry& a, const CopiedNotifyEntry& b)
        {
            if (a.timeSec != b.timeSec)
                return a.timeSec < b.timeSec;
            return a.trackIndex < b.trackIndex;
        });

    NOTIFY("노티파이 복사: {}개", static_cast<int32>(_copiedNotifies.size()));
}

void Animation_View::Copy_SelectedState()
{
    AnimNotifyStateEntry* selectedState = Find_SelectedState();
    if (selectedState == nullptr || selectedState->notifyState == nullptr)
        return;

    _notifyClipboardKind = NotifyClipboardKind::State;
    _copiedNotifies.clear();
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
    if (animationData == nullptr || _copiedNotifies.empty())
        return;

    if (animationData->notifyTracks.empty())
        Add_NotifyTrack("Notify");

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const float pasteTimeSec = static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(fps);
    const float baseTimeSec = _copiedNotifies.front().timeSec;

    _selectedNotifyIds.clear();
    int32 lastCreatedId = -1;
    int32 lastTrackIndex = _selectedNotifyTrackIndex;

    for (const CopiedNotifyEntry& copiedEntry : _copiedNotifies)
    {
        Shared<AnimNotify> notifyInstance = GAME->Create_AnimNotifyPrototype(copiedEntry.typeName);
        if (notifyInstance == nullptr)
            continue;

        notifyInstance->From_Json(copiedEntry.payload);

        AnimNotifyEventEntry entry{};
        const int32 createdId = animationData->nextNotifyId++;
        entry.id = createdId;
        entry.timeSec = max(0.f, pasteTimeSec + (copiedEntry.timeSec - baseTimeSec));
        entry.trackIndex = std::clamp(copiedEntry.trackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);
        entry.notify = notifyInstance;

        animationData->notifies.push_back(std::move(entry));
        _selectedNotifyIds.insert(createdId);
        lastCreatedId = createdId;
        lastTrackIndex = entry.trackIndex;
    }

    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    _selectedNotifyId = lastCreatedId;
    _selectedStateId = -1;
    _selectedNotifyTrackIndex = lastTrackIndex;
    _isDirty = true;

    if (!_selectedNotifyIds.empty())
        NOTIFY("노티파이 붙여넣기: {}개", static_cast<int32>(_selectedNotifyIds.size()));
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

    Clear_SelectedNotifySelection();
    _selectedStateId = createdId;
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
    if (_previewModelCom != nullptr)
        return _previewModelCom;

    return _liveModelCom;
}

bool Animation_View::Try_GetTrailSample(EffectTrailSample& outSample) const
{
    for (const PreviewTrailRuntime& runtime : _previewTrailRuntimes)
    {
        if (runtime.stateId != _previewTrailProviderStateId)
            continue;

        return Try_GetTrailSampleForRuntime(runtime, outSample); // 현재 update 중인 runtime의 partTag/anchor로 sample을 계산한다.
    }

    return false;
}

Weapon* Animation_View::Resolve_PreviewWeapon(const wstring& partTag) const
{
    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return nullptr;

    return dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(partTag));
}

bool Animation_View::Try_GetTrailSampleForRuntime(
    const PreviewTrailRuntime& runtime,
    EffectTrailSample& outSample) const
{
    Vec3 baseWorld{};
    Vec3 tipWorld{};
    if (!Try_ResolvePreviewTrailAnchorWorldPosition(runtime.partTag, runtime.trailAnchorAsset.base, baseWorld))
        return false;
    if (!Try_ResolvePreviewTrailAnchorWorldPosition(runtime.partTag, runtime.trailAnchorAsset.tip, tipWorld))
        return false;

    outSample.baseWorldPosition = baseWorld;
    outSample.tipWorldPosition = tipWorld;
    return true;
}

void Animation_View::Update_PreviewWeaponRotateStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _liveModelCom == nullptr)
    {
        Reset_PreviewWeaponRotateStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewWeaponRotateStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    if (playbackModel == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewWeaponRotateStates();
        return;
    }

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
    context.previousTimeSec = max(0.f, context.currentTimeSec - max(0.f, timeDelta));
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    vector<wstring> touchedPartTags{};

    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_WeaponRotate")
            continue;
        if (context.currentTimeSec < entry.startSec)
            continue;
        if (context.currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStateIds.push_back(entry.id);

        const json payload = entry.notifyState->To_Json();
        const string partTagString = payload.value("_partTag", string{ "Part_WeaponL" });
        const wstring partTag = String::ToWString(partTagString);

        AnimNotifyContext stateContext = context;
        stateContext.notifyStateDurationSec = entry.durationSec;
        stateContext.notifyStateProgress =
            entry.durationSec > 0.f
            ? clamp((context.currentTimeSec - entry.startSec) / entry.durationSec, 0.f, 1.f)
            : 1.f;

        PreviewWeaponRotateRuntime* runtime = nullptr;
        for (PreviewWeaponRotateRuntime& entryRuntime : _previewWeaponRotateRuntimes)
        {
            if (entryRuntime.stateId == entry.id)
            {
                runtime = &entryRuntime;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewWeaponRotateRuntime newRuntime{};
            newRuntime.stateId = entry.id;
            newRuntime.notifyState = entry.notifyState;
            newRuntime.partTag = partTag;
            _previewWeaponRotateRuntimes.push_back(std::move(newRuntime));
            runtime = &_previewWeaponRotateRuntimes.back();

            runtime->notifyState->On_Begin(stateContext); // preview는 실제 notify 실행 대신 현재 _asset 기준으로 직접 state를 시작한다.
        }
        else
        {
            runtime->partTag = partTag;
            runtime->notifyState->On_Tick(stateContext);
        }

        touchedPartTags.push_back(partTag);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewWeaponRotateRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewWeaponRotateRuntimes[runtimeIndex].stateId;
        const bool stillActive =
            std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();

        if (stillActive)
            continue;

        AnimNotifyContext endContext = context;
        endContext.notifyStateProgress = 1.f;
        endContext.notifyStateDurationSec = 0.f;

        if (_previewWeaponRotateRuntimes[runtimeIndex].notifyState)
            _previewWeaponRotateRuntimes[runtimeIndex].notifyState->On_End(endContext);

        touchedPartTags.push_back(_previewWeaponRotateRuntimes[runtimeIndex].partTag);
        _previewWeaponRotateRuntimes.erase(_previewWeaponRotateRuntimes.begin() + runtimeIndex);
    }

    for (const wstring& partTag : touchedPartTags)
    {
        if (Weapon* weapon = Resolve_PreviewWeapon(partTag))
            weapon->Update(0.f); // notify가 바꾼 로컬 회전을 현재 프레임 combinedWorldMatrix에 바로 반영한다.
    }
}

void Animation_View::Reset_PreviewWeaponRotateStates()
{
    if (_previewWeaponRotateRuntimes.empty())
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

    AnimNotifyContext endContext{};
    endContext.owner = previewRootObject.get();
    endContext.model = playbackModel.get();
    endContext.modelGuid = _asset.modelGuid;
    endContext.animationName = _currentAnimationName;
    endContext.notifyStateProgress = 1.f;
    endContext.notifyStateDurationSec = 0.f;
    endContext.isPreview = true;

    vector<wstring> touchedPartTags{};
    for (PreviewWeaponRotateRuntime& runtime : _previewWeaponRotateRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(endContext);

        touchedPartTags.push_back(runtime.partTag);
    }

    _previewWeaponRotateRuntimes.clear();

    for (const wstring& partTag : touchedPartTags)
    {
        if (Weapon* weapon = Resolve_PreviewWeapon(partTag))
            weapon->Update(0.f);
    }
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
    if (_previewRootObject != nullptr)
        return _previewRootObject;

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
    if (_previewGridBatch && _previewGridEffect && _previewGridInputLayout && _previewGridDepthState && _previewOverlayDepthState)
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
        _previewOverlayDepthState.Reset();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC overlayDepthDesc{};
    overlayDepthDesc.DepthEnable = FALSE;
    overlayDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    overlayDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    overlayDepthDesc.StencilEnable = FALSE;

    if (FAILED(GAME->Get_Device()->CreateDepthStencilState(&overlayDepthDesc, _previewOverlayDepthState.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        _previewGridDepthState.Reset();
        _previewOverlayDepthState.Reset();
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

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
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

void Animation_View::Draw_SelectedBoneMarker() const
{
    if (_selectedBoneIndex < 0)
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    const uint32 boneIndex = static_cast<uint32>(_selectedBoneIndex);
    if (boneIndex >= playbackModel->Get_NumBones())
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
        return;

    const Matrix* boneMatrix = playbackModel->Get_BoneMatrixPtr(boneIndex);
    if (boneMatrix == nullptr)
        return;

    Shared<GameObject> ownerObject = playbackModel->Get_Owner();
    Shared<TransformCom> ownerTransform = ownerObject ? ownerObject->Get_Transform() : nullptr;
    const Matrix ownerWorldMatrix = ownerTransform ? ownerTransform->Get_WorldMatrix() : Matrix::Identity;
    const Matrix worldBoneMatrix = *boneMatrix * ownerWorldMatrix;

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
    context->OMSetDepthStencilState(_previewOverlayDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    const Vec3 bonePosition(worldBoneMatrix._41, worldBoneMatrix._42, worldBoneMatrix._43);

    Vec3 rightAxis(worldBoneMatrix._11, worldBoneMatrix._12, worldBoneMatrix._13);
    Vec3 upAxis(worldBoneMatrix._21, worldBoneMatrix._22, worldBoneMatrix._23);
    Vec3 forwardAxis(worldBoneMatrix._31, worldBoneMatrix._32, worldBoneMatrix._33);

    if (rightAxis.LengthSquared() <= 0.000001f)
        rightAxis = Vec3::Right;
    else
        rightAxis.Normalize();

    if (upAxis.LengthSquared() <= 0.000001f)
        upAxis = Vec3::Up;
    else
        upAxis.Normalize();

    if (forwardAxis.LengthSquared() <= 0.000001f)
        forwardAxis = Vec3::Forward;
    else
        forwardAxis.Normalize();

    constexpr float markerRadius = 0.09f;
    constexpr float axisLength = 0.28f;
    constexpr int markerSegments = 24;

    _previewGridBatch->Begin();

    const auto drawAxisLine =
        [&](const Vec3& direction, const XMVECTORF32& color) -> void
    {
        DirectX::VertexPositionColor lineStart(bonePosition, color);
        DirectX::VertexPositionColor lineEnd(bonePosition + direction, color);
        _previewGridBatch->DrawLine(lineStart, lineEnd);
    };

    const auto drawRing =
        [&](const Vec3& axisA, const Vec3& axisB, const XMVECTORF32& color) -> void
    {
        for (int segmentIndex = 0; segmentIndex < markerSegments; ++segmentIndex)
        {
            const float t0 = XM_2PI * static_cast<float>(segmentIndex) / static_cast<float>(markerSegments);
            const float t1 = XM_2PI * static_cast<float>(segmentIndex + 1) / static_cast<float>(markerSegments);

            const Vec3 point0 = bonePosition +
                axisA * (cosf(t0) * markerRadius) +
                axisB * (sinf(t0) * markerRadius);
            const Vec3 point1 = bonePosition +
                axisA * (cosf(t1) * markerRadius) +
                axisB * (sinf(t1) * markerRadius);

            DirectX::VertexPositionColor lineStart(point0, color);
            DirectX::VertexPositionColor lineEnd(point1, color);
            _previewGridBatch->DrawLine(lineStart, lineEnd);
        }
    };

    drawRing(rightAxis, upAxis, DirectX::Colors::Yellow);
    drawRing(rightAxis, forwardAxis, DirectX::Colors::Yellow);
    drawRing(upAxis, forwardAxis, DirectX::Colors::Yellow);
    drawAxisLine(rightAxis * axisLength, DirectX::Colors::Red);
    drawAxisLine(upAxis * axisLength, DirectX::Colors::Lime);
    drawAxisLine(forwardAxis * axisLength, DirectX::Colors::DeepSkyBlue);

    _previewGridBatch->End();
}

void Animation_View::Draw_TrailAnchorPreview() const
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
        return;

    Vec3 baseWorld{};
    Vec3 tipWorld{};
    if (!Try_ResolveTrailAnchorWorldPosition(playbackModel, _trailAnchorAsset.base, baseWorld))
        return;
    if (!Try_ResolveTrailAnchorWorldPosition(playbackModel, _trailAnchorAsset.tip, tipWorld))
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
    context->OMSetDepthStencilState(_previewOverlayDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    constexpr float markerSize = 0.08f;
    _previewGridBatch->Begin();

    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld, DirectX::Colors::Orange),
        DirectX::VertexPositionColor(tipWorld, DirectX::Colors::Cyan));

    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld + Vec3(-markerSize, 0.f, 0.f), DirectX::Colors::Orange),
        DirectX::VertexPositionColor(baseWorld + Vec3(markerSize, 0.f, 0.f), DirectX::Colors::Orange));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, -markerSize, 0.f), DirectX::Colors::Orange),
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, markerSize, 0.f), DirectX::Colors::Orange));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, 0.f, -markerSize), DirectX::Colors::Orange),
        DirectX::VertexPositionColor(baseWorld + Vec3(0.f, 0.f, markerSize), DirectX::Colors::Orange));

    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(tipWorld + Vec3(-markerSize, 0.f, 0.f), DirectX::Colors::Cyan),
        DirectX::VertexPositionColor(tipWorld + Vec3(markerSize, 0.f, 0.f), DirectX::Colors::Cyan));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, -markerSize, 0.f), DirectX::Colors::Cyan),
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, markerSize, 0.f), DirectX::Colors::Cyan));
    _previewGridBatch->DrawLine(
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, 0.f, -markerSize), DirectX::Colors::Cyan),
        DirectX::VertexPositionColor(tipWorld + Vec3(0.f, 0.f, markerSize), DirectX::Colors::Cyan));

    _previewGridBatch->End();
}

void Animation_View::Update_PreviewPlayEffectNotifies(float previousTimeSec, float currentTimeSec, float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewPlayEffectStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewPlayEffectStates();
        return;
    }

    const bool wrapped = currentTimeSec + 1e-4f < previousTimeSec;
    const bool timeAdvanced = wrapped || currentTimeSec > previousTimeSec;

    AnimNotifyContext context{};
    context.owner = _previewModelCom->Get_Owner().get();
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.previousTimeSec = previousTimeSec;
    context.currentTimeSec = currentTimeSec;
    context.deltaTime = timeDelta;
    context.isLooping = wrapped;
    context.wrapped = wrapped;
    context.isPreview = true;

    if (timeAdvanced)
    {
        for (const AnimNotifyEventEntry& entry : animationData->notifies)
        {
            if (entry.notify == nullptr)
                continue;
            const string typeName = entry.notify->Get_TypeName();
            if (typeName != "AN_PlayEffect" && typeName != "AN_PlaySound")
                continue;
            if (!Is_PreviewNotifyTimeInRange(entry.timeSec, previousTimeSec, currentTimeSec, wrapped))
                continue;

            entry.notify->Execute(context);
        }
    }

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_PlayEffect")
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStateIds.push_back(entry.id);

        AnimNotifyContext stateContext = context;
        stateContext.notifyStateDurationSec = entry.durationSec;
        stateContext.notifyStateProgress = entry.durationSec > 0.f
            ? std::clamp((currentTimeSec - entry.startSec) / entry.durationSec, 0.f, 1.f)
            : 1.f;
        const string payloadJson = entry.notifyState->To_Json().dump();

        auto runtimeIt = std::find_if(
            _previewPlayEffectStateRuntimes.begin(),
            _previewPlayEffectStateRuntimes.end(),
            [&](const PreviewPlayEffectStateRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewPlayEffectStateRuntimes.end())
        {
            PreviewPlayEffectStateRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewPlayEffectStateRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext);
            continue;
        }

        if (runtimeIt->notifyState != entry.notifyState ||
            runtimeIt->payloadJson != payloadJson)
        {
            if (runtimeIt->notifyState)
            {
                AnimNotifyContext endContext = stateContext;
                endContext.notifyStateProgress = 1.f;
                runtimeIt->notifyState->On_End(endContext);
            }

            runtimeIt->notifyState = entry.notifyState;
            runtimeIt->payloadJson = payloadJson;
            entry.notifyState->On_Begin(stateContext);
            continue;
        }

        if (timeAdvanced)
            entry.notifyState->On_Tick(stateContext);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewPlayEffectStateRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewPlayEffectStateRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewPlayEffectStateRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            _previewPlayEffectStateRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewPlayEffectStateRuntimes.erase(_previewPlayEffectStateRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewPlayEffectStates()
{
    if (_previewPlayEffectStateRuntimes.empty())
        return;

    AnimNotifyContext context{};
    context.owner = _previewModelCom ? _previewModelCom->Get_Owner().get() : nullptr;
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.isPreview = true;

    if (context.owner != nullptr)
    {
        for (PreviewPlayEffectStateRuntime& runtime : _previewPlayEffectStateRuntimes)
        {
            if (runtime.notifyState)
                runtime.notifyState->On_End(context);
        }
    }

    _previewPlayEffectStateRuntimes.clear();
}

void Animation_View::Update_PreviewTrailEffect(float timeDelta)
{
    const vector<const AnimNotifyStateEntry*> activeTrailStates = Find_ActivePreviewTrailStates();
    if (activeTrailStates.empty())
    {
        _previewTrailProviderStateId = -1;

        for (PreviewTrailRuntime& runtime : _previewTrailRuntimes)
            Begin_PreviewTrailDrain(runtime);

        for (int32 runtimeIndex = static_cast<int32>(_previewTrailRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
        {
            if (Tick_PreviewTrailDrain(_previewTrailRuntimes[runtimeIndex], timeDelta))
                _previewTrailRuntimes.erase(_previewTrailRuntimes.begin() + runtimeIndex);
        }

        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const float currentTimeSec = playbackModel ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;
    vector<int32> activeStateIds{};
    activeStateIds.reserve(activeTrailStates.size());

    for (const AnimNotifyStateEntry* activeTrailState : activeTrailStates)
    {
        if (activeTrailState == nullptr || activeTrailState->notifyState == nullptr)
            continue;

        activeStateIds.push_back(activeTrailState->id);

        const json payload = activeTrailState->notifyState->To_Json();
        const string partTag = payload.value("_partTag", string{ "Part_Weapon" });
        const string effectNameValue = payload.value("_effectName", string{});
        if (effectNameValue.empty())
            continue;

        PreviewTrailRuntime* runtime = nullptr;
        for (PreviewTrailRuntime& entry : _previewTrailRuntimes)
        {
            if (entry.stateId == activeTrailState->id)
            {
                runtime = &entry;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewTrailRuntime newRuntime{};
            newRuntime.stateId = activeTrailState->id;
            newRuntime.partTag = partTag; // 각 ANS_Trail이 지정한 partTag를 자기 runtime에 고정한다.
            _previewTrailRuntimes.push_back(std::move(newRuntime));
            runtime = &_previewTrailRuntimes.back();

            if (!Load_PreviewTrailAnchorAsset(runtime->partTag, *runtime))
                continue;
        }

        if (runtime->isDraining)
        {
            if (runtime->effect)
                runtime->effect->Stop();

            runtime->effect.reset();
            runtime->effectName.clear();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
            runtime->lastTimeSec = -1.f;
        }

        if (runtime->partTag != partTag)
        {
            if (runtime->effect)
                runtime->effect->Stop();

            runtime->partTag = partTag;   // 같은 state라도 partTag가 바뀌면 새 무기 기준으로 anchor를 다시 읽는다.
            runtime->effect.reset();
            runtime->effectName.clear();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
            runtime->lastTimeSec = -1.f;

            if (!Load_PreviewTrailAnchorAsset(runtime->partTag, *runtime))
                continue;
        }

        if (runtime->trailAnchorAsset.modelGuid.empty())
            continue;

        const wstring effectName(effectNameValue.begin(), effectNameValue.end());
        if (!Ensure_PreviewTrailEffect(*runtime, effectName))
            continue;

        const bool timeMovedBackward =
            runtime->lastTimeSec >= 0.f &&
            currentTimeSec + 1e-4f < runtime->lastTimeSec;
        if (timeMovedBackward)
        {
            runtime->effect->Stop(); // scrub나 seek으로 시간이 뒤로 가면 해당 runtime history만 버리고 다시 시작한다.
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
        }

        EffectTrailSample trailSample{};
        if (!Try_GetTrailSampleForRuntime(*runtime, trailSample))
        {
            if (runtime->effect && runtime->isPlaying)
            {
                Begin_PreviewTrailDrain(*runtime);
            }
            continue;
        }

        if (!runtime->isPlaying)
        {
            runtime->effect->Bind_TrailSampleProvider(static_pointer_cast<IEffectTrailSampleProvider>(GetSharedPtr<Animation_View>())); // preview도 ComputeTrailEmitter가 sample provider를 통해 base/tip을 받아야 history가 쌓인다.
            if (FAILED(runtime->effect->Reset()))
                continue;

            const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
            if (effectTransform == nullptr)
                continue;

            effectTransform->Set_WorldPosition(trailSample.baseWorldPosition);

            if (FAILED(runtime->effect->Play()))
                continue;

            runtime->isPlaying = true;
        }

        const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
        if (effectTransform == nullptr)
            continue;

        effectTransform->Set_WorldPosition(trailSample.baseWorldPosition);
        _previewTrailProviderStateId = runtime->stateId; // 같은 provider를 공유하되, 현재 effect update 전에 어떤 runtime sample을 줄지 지정한다.
        runtime->effect->Priority_Update(timeDelta);
        runtime->effect->Update(timeDelta);
        runtime->lastTimeSec = currentTimeSec;
    }

    _previewTrailProviderStateId = -1;

    for (int32 runtimeIndex = static_cast<int32>(_previewTrailRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewTrailRuntimes[runtimeIndex].stateId;
        const bool stillActive =
            std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();

        if (stillActive)
            continue;

        Begin_PreviewTrailDrain(_previewTrailRuntimes[runtimeIndex]);
        if (Tick_PreviewTrailDrain(_previewTrailRuntimes[runtimeIndex], timeDelta))
            _previewTrailRuntimes.erase(_previewTrailRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewTrailEffect()
{
    for (PreviewTrailRuntime& runtime : _previewTrailRuntimes)
    {
        if (runtime.effect)
            runtime.effect->Stop();
    }

    _previewTrailRuntimes.clear();
    _previewTrailProviderStateId = -1;
}

void Animation_View::Begin_PreviewTrailDrain(PreviewTrailRuntime& runtime)
{
    if (runtime.effect == nullptr || runtime.isDraining)
        return;

    runtime.isDraining = true;
    runtime.drainElapsed = 0.f;
    runtime.isPlaying = true;
}

bool Animation_View::Tick_PreviewTrailDrain(PreviewTrailRuntime& runtime, float timeDelta)
{
    if (runtime.effect == nullptr)
        return true;

    if (!runtime.isDraining)
        return false;

    runtime.drainElapsed += max(0.f, timeDelta);
    if (runtime.drainElapsed < runtime.drainDuration)
    {
        runtime.effect->Priority_Update(timeDelta);
        runtime.effect->Update(timeDelta);
        runtime.effect->Late_Update(timeDelta);
        return false;
    }

    runtime.effect->Stop();
    return true;
}

vector<const AnimNotifyStateEntry*> Animation_View::Find_ActivePreviewTrailStates() const
{
    vector<const AnimNotifyStateEntry*> activeStates{};

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (_sourceMode != SourceMode::LiveModel || playbackModel == nullptr)
        return activeStates;

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return activeStates;

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_Trail")
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStates.push_back(&entry);
    }

    return activeStates;
}

bool Animation_View::Ensure_PreviewTrailEffect(PreviewTrailRuntime& runtime, const wstring& effectName)
{
    if (runtime.effect && runtime.effectName == effectName)
        return true;

    if (runtime.effect)
        runtime.effect->Stop();

    runtime.effect.reset();
    runtime.effectName.clear();
    runtime.isPlaying = false;
    runtime.isDraining = false;
    runtime.drainElapsed = 0.f;
    runtime.lastTimeSec = -1.f;

    const Shared<const EffectDefinition> definition = EffectAssetRuntimeLoader::Load_DefinitionByName(effectName);
    if (definition == nullptr)
        return false;

    runtime.drainDuration = Resolve_TrailDrainDuration(definition);

    const Shared<EffectInstance> prototype = EffectInstance::Create(GAME->Get_Device(), GAME->Get_Context());
    if (prototype == nullptr)
        return false;

    EffectInstanceDesc effectDesc{};
    effectDesc.definition = definition;

    const Shared<GameObject> previewEffectObject = prototype->Clone(&effectDesc);
    runtime.effect = dynamic_pointer_cast<EffectInstance>(previewEffectObject);
    if (runtime.effect == nullptr)
        return false;

    runtime.effect->Bind_TrailSampleProvider(static_pointer_cast<IEffectTrailSampleProvider>(GetSharedPtr<Animation_View>()));
    CHECK_FAILED(runtime.effect->Try_BeginPlay(), false);
    CHECK_FAILED(runtime.effect->Reset(), false);

    runtime.effectName = effectName;
    return true;
}

void Animation_View::Assign_SelectedBoneToTrailAnchor(const Shared<ModelCom>& playbackModel, TrailAnchorSlot slot)
{
    if (playbackModel == nullptr || _selectedBoneIndex < 0)
        return;

    const uint32 boneIndex = static_cast<uint32>(_selectedBoneIndex);
    if (boneIndex >= playbackModel->Get_NumBones())
        return;

    Client::TrailAnchorPointDesc* target = nullptr;
    if (slot == TrailAnchorSlot::Base)
        target = &_trailAnchorAsset.base;
    else
        target = &_trailAnchorAsset.tip;

    target->boneName = playbackModel->Get_BoneName(boneIndex);
    target->localPosition = Vec3::Zero;

    _trailAnchorAsset.modelGuid = _modelGuid;
    MarkDirty();
}

bool Animation_View::Try_ResolveTrailAnchorWorldPosition(
    const Shared<ModelCom>& playbackModel,
    const Client::TrailAnchorPointDesc& anchor,
    Vec3& outWorldPosition) const
{
    if (playbackModel == nullptr || anchor.boneName.empty())
        return false;

    const int32 boneIndex = playbackModel->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = playbackModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    Shared<GameObject> ownerObject = playbackModel->Get_Owner();
    Shared<TransformCom> ownerTransform = ownerObject ? ownerObject->Get_Transform() : nullptr;
    const Matrix ownerWorldMatrix = ownerTransform ? ownerTransform->Get_WorldMatrix() : Matrix::Identity;

    const Matrix anchorWorldMatrix = *boneMatrix * ownerWorldMatrix;
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

bool Animation_View::Try_ResolvePreviewTrailAnchorWorldPosition(
    const string& partTag,
    const Client::TrailAnchorPointDesc& anchor,
    Vec3& outWorldPosition) const
{
    if (anchor.boneName.empty())
        return false;

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewTrailPartTag = String::ToWString(partTag);
    Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(previewTrailPartTag));
    if (weaponPart == nullptr)
        return false;

    const Shared<ModelCom> weaponModel = weaponPart->Get_Model();
    if (weaponModel == nullptr)
        return false;

    const int32 boneIndex = weaponModel->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = weaponModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorWorldMatrix = *boneMatrix * weaponPart->Get_CombinedWorldMatrix();
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

void Animation_View::Validate_SelectedBoneSelection()
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
    {
        _selectedBoneIndex = -1;
        return;
    }

    if (_selectedBoneIndex < 0)
        return;

    if (static_cast<uint32>(_selectedBoneIndex) >= playbackModel->Get_NumBones())
        _selectedBoneIndex = -1;
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

bool Animation_View::Should_DeferToCompanionPreview()
{
    return !EDITOR->Is_ActivePreviewOwner(PreviewOwner::Animation);
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
    playbackModel->Play_Animation(0.f); // preview 애니메이션 교체 직후에도 현재 시점 notify/state를 한 번 즉시 반영한다.
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

void Animation_View::Clear_SelectedNotifySelection()
{
    _selectedNotifyId = -1;
    _selectedNotifyIds.clear();
}

bool Animation_View::Is_NotifySelected(int32 notifyId) const
{
    return _selectedNotifyIds.find(notifyId) != _selectedNotifyIds.end();
}

Shared<Animation_View> Animation_View::Create()
{
    return make_shared<Animation_View>();
}
