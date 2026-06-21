#include "Animation_View.h"
#include "Animation_NameUtils.h"
#include "AnimSequencerAdapter.h"
#include "AnimNotify_Parser.h"
#include "AnimationModifier.h"
#include "ChainModifier.h"
#include "PoseFollowChainModifier.h"
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
#include "PlayableParts.h"
#include "PreviewObjectHelpers.h"
#include "RenderTarget.h"
#include "SkyBoxFlow.h"
#include "SkyCloudCard.h"
#include "TransformCom.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "imgui_internal.h"
#include "Notification_Manager.h"
#include "Weapon.h"
#include <algorithm>
#include <cctype>

namespace
{
    const Vec3 kPreviewSkyCloudLocalOffset = Vec3(28.f, 34.f, 82.f);

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

    float Resolve_SourceHistoryDrainDuration(const Shared<const EffectDefinition>& definition)
    {
        constexpr float fallbackDuration = 0.5f;
        if (definition == nullptr)
            return fallbackDuration;

        float duration = fallbackDuration;
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled)
                continue;

            if (emitter.kind == EffectEmitterKind::Ribbon)
            {
                const ComputeRibbonEmitterDesc* ribbonDesc = get_if<ComputeRibbonEmitterDesc>(&emitter.concreteDesc);
                if (ribbonDesc == nullptr || ribbonDesc->sourceMode != EffectSourceHistoryRibbonSourceMode::SelfRoot)
                    continue;

                duration = max(duration, ribbonDesc->sampleLifetime);
                continue;
            }

            if (emitter.kind == EffectEmitterKind::SourceHistorySpriteTrail)
            {
                const ComputeSourceHistorySpriteTrailEmitterDesc* spriteTrailDesc =
                    get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitter.concreteDesc);
                if (spriteTrailDesc == nullptr || spriteTrailDesc->sourceMode != EffectSourceHistoryRibbonSourceMode::SelfRoot)
                    continue;

                float spriteDuration = spriteTrailDesc->sampleLifetime + spriteTrailDesc->stampLifetime;
                if (spriteTrailDesc->pathReplay.enabled)
                {
                    spriteDuration +=
                        spriteTrailDesc->pathReplay.delayTime +
                        spriteTrailDesc->pathReplay.drainDuration;
                }

                duration = max(duration, spriteDuration);
            }
        }

        return max(0.001f, duration + 0.1f);
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

    bool Contains_CaseInsensitive(const string& text, const char* filter)
    {
        if (filter == nullptr || filter[0] == '\0')
            return true;

        string source = text;
        string needle = filter;

        auto toLower = [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        };

        std::transform(source.begin(), source.end(), source.begin(), toLower);
        std::transform(needle.begin(), needle.end(), needle.begin(), toLower);

        return source.find(needle) != string::npos;
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

    if (_isSectionPreviewPlaying)
    {
        const AnimNotifyData* animationData = Get_CurrentAnimation();
        if (animationData == nullptr)
        {
            _isSectionPreviewPlaying = false;
            return;
        }

        const float startSec = Get_PlaySectionStartSec(animationData);
        const float endSec = Get_PlaySectionEndSec(animationData);

        if (playbackModel->Get_CurrentAnimationTimeSec() + 0.0001f >= endSec)
        {
            playbackModel->Set_CurrentAnimationTimeSec(startSec);
            playbackModel->Play_Animation(0.f);
            playbackModel->Set_AnimationPaused(false);
            _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
            return;
        }
    }

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

    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);
    if (_previewRenderTarget == nullptr)
        return;

    if (_showPreviewSkyBackdrop)
        Ensure_PreviewSkyBackdrop();

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
    if (_showPreviewSkyBackdrop)
        Update_PreviewSkyBackdrop(dt);

    if (_sourceMode == SourceMode::LiveModel)
    {
        const float previousTimeSec = _previewModelCom ? _previewModelCom->Get_CurrentAnimationTimeSec() : 0.f;
        GAME->Backup_RenderGroup();
        if (_showPreviewSkyBackdrop)
            Late_Update_PreviewSkyBackdrop(dt);

        Update_PreviewAnimSpeedStates(dt); // ANS_AnimSpeed는 sampling 전에 적용해야 현재 preview 재생 속도에 바로 반영된다.
        Update_PreviewRootControllerStates(dt);
        PreviewObjectHelpers::Update_PreviewObject(previewRenderObject, dt, false);
        const float currentTimeSec = _previewModelCom ? _previewModelCom->Get_CurrentAnimationTimeSec() : previousTimeSec;
        Update_PreviewPlayEffectNotifies(previousTimeSec, currentTimeSec, dt);
        PreviewObjectHelpers::Tick_AttachedPreviewEffects(previewRenderObject, dt);
        Update_PreviewSLDodgeMoveStates(dt);
        Update_PreviewWeaponRotateStates(dt); // preview 전용 WeaponRotate를 먼저 반영하고 무기 world를 다시 맞춘 뒤 trail sample을 계산한다.
        Update_PreviewSourceHistoryEffect(dt);
        Update_PreviewTrailEffect(dt);
    }
    else
    {
        GAME->Backup_RenderGroup();
        if (_showPreviewSkyBackdrop)
            Late_Update_PreviewSkyBackdrop(dt);
    }

    for (PreviewTrailRuntime& runtime : _previewTrailRuntimes)
    {
        if (runtime.effect && runtime.isPlaying)
            runtime.effect->Late_Update(dt); // preview에서도 활성 trail runtime마다 Late_Update를 따로 돌린다.
    }

    for (PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
    {
        if (runtime.effect && runtime.isPlaying)
            runtime.effect->Late_Update(dt);
    }

    GAME->Draw_RendererOnly();
    GAME->Restore_RenderGroup();
    Ensure_PreviewGridResources();
    Draw_SelectedBoneMarker();
    Draw_TrailAnchorPreview();
    Draw_PreviewGrid();
    EDITOR->Capture_BackBuffer_ToRenderTarget(_previewRenderTarget);
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
            {
                if (playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
                else
                {
                    playbackModel->Set_AnimationPaused(true);
                    _isSectionPreviewPlaying = false;
                }
            }
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

        if (_hasPendingNotifyUndoSnapshot &&
            !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemActive())
        {
            Commit_PendingNotifyUndo();
        }
    }

    Set_Open(isOpen);
    ImGui::End();

    if (!isOpen)
    {
        Reset_PreviewAnimSpeedStates();
        Reset_PreviewPlayEffectStates();
        Reset_PreviewSourceHistoryEffect();
        Reset_PreviewRootControllerStates();
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
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewAnimSpeedStates();
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
    Clear_NotifyUndoHistory();

    Refresh_Source();
    Load_NotifyAsset();
    Load_TrailAnchorAsset();
    Set_Open(true);
}

void Animation_View::Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName, uint32 preferredDockId, bool focusOnOpen)
{
    const wstring sourceModelPath = modelCom ? modelCom->Get_SourceModelPath() : wstring{};

    Reset_PreviewTrailEffect();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewAnimSpeedStates();
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
    Clear_NotifyUndoHistory();

    Refresh_Source();
    Rebuild_PreviewClone();
    Load_NotifyAsset();
    Load_TrailAnchorAsset();
    Ensure_PreviewCamera();
    Fit_PreviewCameraToModel();
    Sync_PreviewAnimationSelection(true);
    EDITOR->Set_StickyPreviewOwner(PreviewOwner::Animation);

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
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewAnimSpeedStates();
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
    Clear_NotifyUndoHistory();
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
    Reset_PreviewAnimSpeedStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewSourceHistoryEffect();
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
    Reset_PreviewSourceHistoryEffect();
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

bool Animation_View::Load_PreviewSourceHistoryAnchorAsset(
    const string& partTag,
    PreviewSourceHistoryRuntime& outRuntime)
{
    outRuntime.trailAnchorAsset = {};
    outRuntime.modelGuid.clear();

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewPartTag = String::ToWString(partTag);
    Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(previewPartTag));
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
                        Clear_NotifyUndoHistory();

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

float Animation_View::Get_PlaySectionStartSec(const AnimNotifyData* animationData) const
{
    if (animationData == nullptr)
        return 0.f;

    return std::clamp(animationData->playSectionStartSec, 0.f, max(0.f, Get_CurrentAnimationLengthSec()));
}

float Animation_View::Get_PlaySectionEndSec(const AnimNotifyData* animationData) const
{
    const float animationLengthSec = max(0.f, Get_CurrentAnimationLengthSec());
    if (animationData == nullptr)
        return animationLengthSec;

    const float startSec = Get_PlaySectionStartSec(animationData);
    const float endSec = animationData->playSectionEndSec >= 0.f
        ? animationData->playSectionEndSec
        : animationLengthSec;

    return std::clamp(endSec, startSec, animationLengthSec);
}

void Animation_View::Clamp_PlaySection(AnimNotifyData* animationData)
{
    if (animationData == nullptr)
        return;

    const float animationLengthSec = max(0.f, Get_CurrentAnimationLengthSec());
    animationData->playSectionStartSec = std::clamp(animationData->playSectionStartSec, 0.f, animationLengthSec);

    if (animationData->playSectionEndSec >= 0.f)
        animationData->playSectionEndSec = std::clamp(animationData->playSectionEndSec, animationData->playSectionStartSec, animationLengthSec);
}

void Animation_View::Play_CurrentSection(const Shared<ModelCom>& playbackModel)
{
    if (playbackModel == nullptr)
        return;

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    const float startSec = Get_PlaySectionStartSec(animationData);
    const float endSec = Get_PlaySectionEndSec(animationData);

    if (endSec <= startSec + 0.0001f)
    {
        playbackModel->Set_AnimationPaused(true);
        playbackModel->Set_CurrentAnimationTimeSec(startSec);
        playbackModel->Play_Animation(0.f);
        _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
        _isSectionPreviewPlaying = false;
        return;
    }

    const float currentSec = playbackModel->Get_CurrentAnimationTimeSec();
    if (currentSec < startSec - 0.0001f || currentSec >= endSec - 0.0001f)
    {
        playbackModel->Set_CurrentAnimationTimeSec(startSec);
        playbackModel->Play_Animation(0.f);
        _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
    }

    playbackModel->Set_AnimationPaused(false);
    _isSectionPreviewPlaying = true;
}

void Animation_View::Stop_CurrentSection(const Shared<ModelCom>& playbackModel)
{
    if (playbackModel == nullptr)
        return;

    const float startSec = Get_PlaySectionStartSec(Get_CurrentAnimation());
    playbackModel->Set_AnimationPaused(true);
    playbackModel->Set_CurrentAnimationTimeSec(startSec);
    playbackModel->Play_Animation(0.f);
    _sequencerCurrentFrame = static_cast<int32>(std::round(startSec * static_cast<float>(Get_CurrentAnimationFps())));
    _isSectionPreviewPlaying = false;
}

void Animation_View::Draw_PlaybackToolbar()
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const bool hasPlayback = (playbackModel != nullptr);
    const float animationLengthSec = Get_CurrentAnimationLengthSec();
    const float sliderMaxTimeSec = max(0.01f, animationLengthSec);
    const ImVec2 playbackButtonSize(58.f, 0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 4.f));

    if (hasPlayback)
    {
        if (ImGui::Button(playbackModel->Is_AnimationPaused() ? "Play" : "Pause", playbackButtonSize))
        {
            if (playbackModel->Is_AnimationPaused())
                Play_CurrentSection(playbackModel);
            else
            {
                playbackModel->Set_AnimationPaused(true);
                _isSectionPreviewPlaying = false;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Stop", playbackButtonSize))
            Stop_CurrentSection(playbackModel);

        ImGui::SameLine();
        ImGui::TextDisabled("Frame %d / %d", _sequencerCurrentFrame, Get_CurrentAnimationFrameMax());
        ImGui::SameLine(0.f, 12.f);
        ImGui::Checkbox("Grid", &_showPreviewGrid);

        ImGui::SameLine();
        ImGui::TextDisabled("Time");
        ImGui::SameLine();

        float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
        ImGui::SetNextItemWidth(260.f);
        if (ImGui::SliderFloat("##AnimScrub", &currentTimeSec, 0.f, sliderMaxTimeSec, "%.3f s"))
        {
            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
            playbackModel->Play_Animation(0.f); // 시간은 고정한 채 notify/state만 현재 위치 기준으로 다시 평가한다.
            _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
            _isSectionPreviewPlaying = false;
        }

        AnimNotifyData* animationData = Get_CurrentAnimation();
        if (animationData != nullptr)
        {
            const float sectionLengthSec = max(0.f, animationLengthSec);
            float sectionStartSec = Get_PlaySectionStartSec(animationData);
            float sectionEndSec = Get_PlaySectionEndSec(animationData);
            const float selectedSectionLengthSec = max(0.f, sectionEndSec - sectionStartSec);

            ImGui::Spacing();
            ImGui::PushID("PlaySectionControls");
            ImGui::TextDisabled("Section");
            ImGui::SameLine(0.f, 12.f);

            ImGui::BeginGroup();
            ImGui::TextDisabled("Start");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(74.f);
            const NotifyUndoSnapshot sectionStartUndoSnapshot = Capture_NotifyUndoSnapshot();
            if (ImGui::DragFloat("##SectionStartSec", &sectionStartSec, 0.01f, 0.f, sectionLengthSec, "%.3f"))
            {
                if (!_hasPendingNotifyUndoSnapshot)
                {
                    _pendingNotifyUndoSnapshot = sectionStartUndoSnapshot;
                    _hasPendingNotifyUndoSnapshot = true;
                }
                animationData->playSectionStartSec = std::clamp(sectionStartSec, 0.f, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
                Commit_PendingNotifyUndo();

            ImGui::SameLine();

            if (ImGui::Button("Set##SectionStartSet"))
            {
                const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
                animationData->playSectionStartSec = std::clamp(playbackModel->Get_CurrentAnimationTimeSec(), 0.f, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;
                Commit_NotifyUndoSnapshot(undoSnapshot);

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            ImGui::EndGroup();

            ImGui::SameLine(0.f, 12.f);

            ImGui::BeginGroup();
            ImGui::TextDisabled("End");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(74.f);
            const NotifyUndoSnapshot sectionEndUndoSnapshot = Capture_NotifyUndoSnapshot();
            if (ImGui::DragFloat("##SectionEndSec", &sectionEndSec, 0.01f, 0.f, sectionLengthSec, "%.3f"))
            {
                if (!_hasPendingNotifyUndoSnapshot)
                {
                    _pendingNotifyUndoSnapshot = sectionEndUndoSnapshot;
                    _hasPendingNotifyUndoSnapshot = true;
                }
                animationData->playSectionEndSec = std::clamp(sectionEndSec, animationData->playSectionStartSec, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
                Commit_PendingNotifyUndo();

            ImGui::SameLine();

            if (ImGui::Button("Set##SectionEndSet"))
            {
                const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
                animationData->playSectionEndSec = std::clamp(playbackModel->Get_CurrentAnimationTimeSec(), animationData->playSectionStartSec, sectionLengthSec);
                Clamp_PlaySection(animationData);
                _isDirty = true;
                Commit_NotifyUndoSnapshot(undoSnapshot);

                if (!playbackModel->Is_AnimationPaused())
                    Play_CurrentSection(playbackModel);
            }
            ImGui::EndGroup();

            ImGui::SameLine(0.f, 12.f);
            ImGui::TextDisabled("Len %.3fs", selectedSectionLengthSec);
            ImGui::PopID();

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

    ImGui::PopStyleVar();
}

void Animation_View::Draw_PreviewPanel()
{
    ImGui::BeginChild("AnimationPreviewPanel", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::Checkbox("SkyBox", &_showPreviewSkyBackdrop);
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

    ID3D11ShaderResourceView* previewSRV =
        _previewRenderTarget != nullptr ? _previewRenderTarget->Get_SRV() : nullptr;
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
            Update_PreviewRootControllerStates(0.f);
            playbackModel->Play_Animation(0.f); // 시퀀서 프레임 점프에서도 preview state notify를 같이 갱신한다.
        }
    }

    if (const AnimNotifyData* animationData = Get_CurrentAnimation())
    {
        const ImRect sequencerRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        const float timelineMinX = sequencerRect.Min.x + 200.f;
        const float timelineMaxX = sequencerRect.Max.x;
        const float timelineWidth = max(1.f, timelineMaxX - timelineMinX);
        const int32 frameMax = max(1, Get_CurrentAnimationFrameMax());
        const int32 fps = max(1, Get_CurrentAnimationFps());

        const int32 startFrame = std::clamp(
            static_cast<int32>(std::round(Get_PlaySectionStartSec(animationData) * static_cast<float>(fps))),
            0,
            frameMax);
        const int32 endFrame = std::clamp(
            static_cast<int32>(std::round(Get_PlaySectionEndSec(animationData) * static_cast<float>(fps))),
            0,
            frameMax);

        const float startX = timelineMinX + timelineWidth * (static_cast<float>(startFrame) / static_cast<float>(frameMax));
        const float endX = timelineMinX + timelineWidth * (static_cast<float>(endFrame) / static_cast<float>(frameMax));

        const float markerY = sequencerRect.Min.y + 5.f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 sectionOutside = IM_COL32(255, 72, 72, 24);
        const ImU32 sectionLine = IM_COL32(255, 82, 72, 210);
        const ImU32 sectionGuide = IM_COL32(255, 82, 72, 95);

        if (startX > timelineMinX)
        {
            drawList->AddRectFilled(
                ImVec2(timelineMinX, sequencerRect.Min.y),
                ImVec2(startX, sequencerRect.Max.y),
                sectionOutside);
        }

        if (endX < timelineMaxX)
        {
            drawList->AddRectFilled(
                ImVec2(endX, sequencerRect.Min.y),
                ImVec2(timelineMaxX, sequencerRect.Max.y),
                sectionOutside);
        }

        if (endX > startX)
        {
            drawList->AddLine(
                ImVec2(startX, markerY),
                ImVec2(endX, markerY),
                sectionLine,
                2.f);
        }

        drawList->AddLine(
            ImVec2(startX, sequencerRect.Min.y),
            ImVec2(startX, sequencerRect.Max.y),
            sectionGuide,
            1.f);

        drawList->AddLine(
            ImVec2(endX, sequencerRect.Min.y),
            ImVec2(endX, sequencerRect.Max.y),
            sectionGuide,
            1.f);
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
        {
            const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
            if (Inspector::Draw_ReflectionSection("SelectedNotify", notify->notify.get(), notify->notify->Get_ReflectionInfo()))
            {
                if (ImGui::IsAnyItemActive())
                {
                    if (!_hasPendingNotifyUndoSnapshot)
                    {
                        _pendingNotifyUndoSnapshot = undoSnapshot;
                        _hasPendingNotifyUndoSnapshot = true;
                    }
                }
                else
                {
                    Commit_NotifyUndoSnapshot(undoSnapshot);
                }

                _isDirty = true;
            }
        }
    }
    else if (AnimNotifyStateEntry* notifyState = Find_SelectedState())
    {
        if (notifyState->notifyState != nullptr)
        {
            const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();
            if (Inspector::Draw_ReflectionSection("SelectedNotifyState", notifyState->notifyState.get(), notifyState->notifyState->Get_ReflectionInfo()))
            {
                if (ImGui::IsAnyItemActive())
                {
                    if (!_hasPendingNotifyUndoSnapshot)
                    {
                        _pendingNotifyUndoSnapshot = undoSnapshot;
                        _hasPendingNotifyUndoSnapshot = true;
                    }
                }
                else
                {
                    Commit_NotifyUndoSnapshot(undoSnapshot);
                }

                _isDirty = true;
            }
        }
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

    Draw_ModifierRuntimePanel(playbackModel);

    ImGui::EndChild();
}

void Animation_View::Draw_ModifierRuntimePanel(const Shared<ModelCom>& playbackModel)
{
    ImGui::Separator();
    ImGui::TextUnformatted("Animation Modifiers");

    if (playbackModel == nullptr)
    {
        ImGui::TextDisabled("No playback model.");
        return;
    }

    const auto& modifiers = playbackModel->Get_Modifiers();
    bool hasEditableModifier = false;

    for (const Shared<AnimationModifier>& modifier : modifiers)
    {
        if (modifier == nullptr)
            continue;

        const ClassReflectionInfo* reflectionInfo = modifier->Get_RuntimeReflectionInfo();
        void* reflectionTarget = modifier->Get_RuntimeReflectionTarget();
        if (reflectionInfo == nullptr || reflectionTarget == nullptr || reflectionInfo->properties.empty())
            continue;

        hasEditableModifier = true;

        const string tag = String::ToString(modifier->Get_ModifierTag());
        const string typeName = reflectionInfo->displayName.empty() ? reflectionInfo->className : reflectionInfo->displayName;
        const string headerLabel = (tag.empty() ? typeName : tag + "  [" + typeName + "]");

        ImGui::PushID(modifier.get());
        if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool active = modifier->Is_Active();
            if (ImGui::Checkbox("Active", &active))
                modifier->Set_Active(active);

            ImGui::SameLine();
            float strengthAlpha = modifier->Get_StrengthAlpha();
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::DragFloat("Strength", &strengthAlpha, 0.01f, 0.f, 1.f))
                modifier->Set_StrengthAlpha(std::clamp(strengthAlpha, 0.f, 1.f));

            if (modifier->Can_RuntimeResetPose())
            {
                if (ImGui::Button("Reset Pose"))
                    modifier->RuntimeResetPose(playbackModel.get());
            }

            if (modifier->Can_RuntimeRebuild())
            {
                ImGui::SameLine();
                if (ImGui::Button("Rebuild Chains"))
                    modifier->RuntimeRebuild(playbackModel.get());
            }

            ImGui::Spacing();
            for (const PropertyInfo& property : reflectionInfo->properties)
            {
                if (Inspector::Draw_ReflectionProperty(reflectionInfo->className, reflectionTarget, property))
                    modifier->On_RuntimeReflectionChanged(playbackModel.get(), property);

                ImGui::Spacing();
            }

            if (auto* chainModifier = dynamic_cast<ChainModifier*>(modifier.get()))
            {
                const size_t groupCount = chainModifier->Get_RuntimeGroupCount();
                if (groupCount > 0 && ImGui::TreeNodeEx("Group Root Offsets", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextDisabled("Root rotation applies while dragging. Use Rebuild only if bind rest must refresh.");
                    for (size_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
                    {
                        ImGui::PushID(static_cast<int>(groupIndex));

                        const string rootBoneName = chainModifier->Get_RuntimeGroupRootBoneName(groupIndex);
                        ImGui::TextWrapped("%s", rootBoneName.empty() ? "(empty root)" : rootBoneName.c_str());

                        Vec3 offsetDegrees = chainModifier->Get_RuntimeGroupRootRotationOffset(groupIndex);
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragFloat3("Rot", &offsetDegrees.x, 0.25f, -180.f, 180.f))
                            chainModifier->Set_RuntimeGroupRootRotationOffset(groupIndex, offsetDegrees);

                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }

            if (auto* poseFollowModifier = dynamic_cast<PoseFollowChainModifier*>(modifier.get()))
            {
                const size_t groupCount = poseFollowModifier->Get_RuntimeGroupCount();
                if (groupCount > 0 && ImGui::TreeNodeEx("Group Multipliers", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextDisabled("X: Wind, Y: Motion, Z: Follow");
                    for (size_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
                    {
                        ImGui::PushID(static_cast<int>(groupIndex));

                        const string rootBoneName = poseFollowModifier->Get_RuntimeGroupRootBoneName(groupIndex);
                        ImGui::TextWrapped("%s", rootBoneName.empty() ? "(empty root)" : rootBoneName.c_str());

                        Vec3 multipliers = poseFollowModifier->Get_RuntimeGroupMultipliers(groupIndex);
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragFloat3("Mul", &multipliers.x, 0.01f, 0.f, 10.f))
                            poseFollowModifier->Set_RuntimeGroupMultipliers(groupIndex, multipliers);

                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::PopID();
    }

    if (!hasEditableModifier)
        ImGui::TextDisabled("No ChainModifier or PoseFollowChainModifier on this model.");
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

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    AnimNotifyTrackDesc trackDesc{};
    trackDesc.name = trackName.empty()
        ? "Notify"
        : trackName;

    animationData->notifyTracks.push_back(trackDesc);
    _selectedNotifyTrackIndex = static_cast<int32>(animationData->notifyTracks.size()) - 1;
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Add_NotifyStateTrack(const string& trackName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    AnimNotifyTrackDesc trackDesc{};
    trackDesc.name = trackName.empty()
        ? "Notify State"
        : trackName;

    animationData->notifyStateTracks.push_back(trackDesc);
    _selectedNotifyStateTrackIndex = static_cast<int32>(animationData->notifyStateTracks.size()) - 1;
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Add_Notify_AtFrame(int32 frame, const string& typeName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<AnimNotify> notifyInstance = GAME->Create_AnimNotifyPrototype(typeName);
    if (notifyInstance == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    if (animationData->notifyTracks.empty())
        animationData->notifyTracks.push_back({ "Notify" });

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
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Add_State_ByFrameRange(int32 startFrame, int32 endFrame, const string& typeName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<AnimNotifyState> notifyStateInstance = GAME->Create_AnimNotifyStatePrototype(typeName);
    if (notifyStateInstance == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    if (animationData->notifyStateTracks.empty())
        animationData->notifyStateTracks.push_back({ "Notify State" });

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
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Delete_SelectedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyIds.empty())
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

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
        Commit_NotifyUndoSnapshot(undoSnapshot);
    }
}

void Animation_View::Delete_SelectedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedStateId == -1)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

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
        Commit_NotifyUndoSnapshot(undoSnapshot);
    }
}

void Animation_View::Delete_SelectedNotifyTrack()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyTrackIndex < 0 || _selectedNotifyTrackIndex >= static_cast<int32>(animationData->notifyTracks.size()))
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    Remove_NotifyTrackEntries(*animationData, _selectedNotifyTrackIndex);
    animationData->notifyTracks.erase(animationData->notifyTracks.begin() + _selectedNotifyTrackIndex);
    _selectedNotifyTrackIndex = animationData->notifyTracks.empty() ? 0 : min(_selectedNotifyTrackIndex, static_cast<int32>(animationData->notifyTracks.size()) - 1);
    Clear_SelectedNotifySelection();
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Delete_SelectedNotifyStateTrack()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyStateTrackIndex < 0 || _selectedNotifyStateTrackIndex >= static_cast<int32>(animationData->notifyStateTracks.size()))
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    Remove_NotifyStateTrackEntries(*animationData, _selectedNotifyStateTrackIndex);
    animationData->notifyStateTracks.erase(animationData->notifyStateTracks.begin() + _selectedNotifyStateTrackIndex);
    _selectedNotifyStateTrackIndex = animationData->notifyStateTracks.empty() ? 0 : min(_selectedNotifyStateTrackIndex, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    _selectedStateId = -1;
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

Animation_View::NotifyUndoSnapshot Animation_View::Capture_NotifyUndoSnapshot() const
{
    NotifyUndoSnapshot snapshot{};
    snapshot.assetJson = AnimNotify_Parser::To_Json(_asset);
    snapshot.currentAnimationName = _currentAnimationName;
    snapshot.selectedNotifyId = _selectedNotifyId;
    snapshot.selectedStateId = _selectedStateId;
    snapshot.selectedNotifyTrackIndex = _selectedNotifyTrackIndex;
    snapshot.selectedNotifyStateTrackIndex = _selectedNotifyStateTrackIndex;
    snapshot.sequencerCurrentFrame = _sequencerCurrentFrame;
    snapshot.selectedNotifyIds.assign(_selectedNotifyIds.begin(), _selectedNotifyIds.end());
    return snapshot;
}

void Animation_View::Restore_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot)
{
    AnimNotifyAsset restoredAsset{};
    if (!AnimNotify_Parser::From_Json(snapshot.assetJson, restoredAsset))
        return;

    _asset = std::move(restoredAsset);
    if (_asset.modelGuid.empty())
        _asset.modelGuid = _modelGuid;

    _currentAnimationName = snapshot.currentAnimationName;
    if (!_currentAnimationName.empty())
    {
        if (const AnimationEntry* entry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName))
            _currentAnimationName = entry->displayName;
    }

    _selectedNotifyId = snapshot.selectedNotifyId;
    _selectedStateId = snapshot.selectedStateId;
    _selectedNotifyTrackIndex = snapshot.selectedNotifyTrackIndex;
    _selectedNotifyStateTrackIndex = snapshot.selectedNotifyStateTrackIndex;
    _sequencerCurrentFrame = snapshot.sequencerCurrentFrame;

    _selectedNotifyIds.clear();
    for (const int32 notifyId : snapshot.selectedNotifyIds)
        _selectedNotifyIds.insert(notifyId);

    if (Find_SelectedNotify() == nullptr)
        Clear_SelectedNotifySelection();

    if (Find_SelectedState() == nullptr)
        _selectedStateId = -1;

    if (AnimNotifyData* animationData = Get_CurrentAnimation())
    {
        _selectedNotifyTrackIndex = animationData->notifyTracks.empty()
            ? 0
            : std::clamp(_selectedNotifyTrackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);
        _selectedNotifyStateTrackIndex = animationData->notifyStateTracks.empty()
            ? 0
            : std::clamp(_selectedNotifyStateTrackIndex, 0, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    }

    Reset_PreviewAnimSpeedStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewTrailEffect();
    Reset_PreviewWeaponRotateStates();

    _isDirty = true;
}

void Animation_View::Commit_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot)
{
    const NotifyUndoSnapshot currentSnapshot = Capture_NotifyUndoSnapshot();
    if (snapshot.assetJson == currentSnapshot.assetJson)
        return;

    static constexpr size_t MaxUndoCount = 64;
    _notifyUndoStack.push_back(snapshot);
    if (_notifyUndoStack.size() > MaxUndoCount)
        _notifyUndoStack.erase(_notifyUndoStack.begin());

    _notifyRedoStack.clear();
}

void Animation_View::Begin_PendingNotifyUndo()
{
    if (_hasPendingNotifyUndoSnapshot)
        return;

    _pendingNotifyUndoSnapshot = Capture_NotifyUndoSnapshot();
    _hasPendingNotifyUndoSnapshot = true;
}

void Animation_View::Commit_PendingNotifyUndo()
{
    if (!_hasPendingNotifyUndoSnapshot)
        return;

    Commit_NotifyUndoSnapshot(_pendingNotifyUndoSnapshot);
    _pendingNotifyUndoSnapshot = {};
    _hasPendingNotifyUndoSnapshot = false;
}

void Animation_View::Clear_NotifyUndoHistory()
{
    _notifyUndoStack.clear();
    _notifyRedoStack.clear();
    _pendingNotifyUndoSnapshot = {};
    _hasPendingNotifyUndoSnapshot = false;
}

void Animation_View::Undo_NotifyEdit()
{
    if (_notifyUndoStack.empty())
        return;

    NotifyUndoSnapshot currentSnapshot = Capture_NotifyUndoSnapshot();
    NotifyUndoSnapshot undoSnapshot = std::move(_notifyUndoStack.back());
    _notifyUndoStack.pop_back();
    _notifyRedoStack.push_back(std::move(currentSnapshot));

    Restore_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Redo_NotifyEdit()
{
    if (_notifyRedoStack.empty())
        return;

    NotifyUndoSnapshot currentSnapshot = Capture_NotifyUndoSnapshot();
    NotifyUndoSnapshot redoSnapshot = std::move(_notifyRedoStack.back());
    _notifyRedoStack.pop_back();
    _notifyUndoStack.push_back(std::move(currentSnapshot));

    Restore_NotifyUndoSnapshot(redoSnapshot);
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

    if (!io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
        {
            if (!_selectedNotifyIds.empty())
                Move_SelectedNotifiesByFrame(-1);
            else if (_selectedStateId != -1)
                Move_SelectedStateByFrame(-1);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
        {
            if (!_selectedNotifyIds.empty())
                Move_SelectedNotifiesByFrame(1);
            else if (_selectedStateId != -1)
                Move_SelectedStateByFrame(1);
        }
    }

    if (!io.KeyCtrl)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        Undo_NotifyEdit();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        Redo_NotifyEdit();
        return;
    }

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

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

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
            playbackModel->Play_Animation(0.f);
        }
    }

    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Move_SelectedStateByFrame(int32 frameDelta)
{
    if (frameDelta == 0)
        return;

    AnimNotifyData* animationData = Get_CurrentAnimation();
    AnimNotifyStateEntry* selectedState = Find_SelectedState();
    if (animationData == nullptr || selectedState == nullptr)
        return;

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 maxFrame = max(0, Get_CurrentAnimationFrameMax());
    const int32 startFrame = std::clamp(
        static_cast<int32>(std::round(selectedState->startSec * static_cast<float>(fps))),
        0,
        maxFrame);
    const int32 durationFrames = max(
        1,
        static_cast<int32>(std::round(selectedState->durationSec * static_cast<float>(fps))));
    const int32 endFrame = min(maxFrame, startFrame + durationFrames);
    const int32 clampedDelta = std::clamp(frameDelta, -startFrame, maxFrame - endFrame);
    if (clampedDelta == 0)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    const int32 movedStartFrame = startFrame + clampedDelta;
    selectedState->startSec = static_cast<float>(movedStartFrame) / static_cast<float>(fps);

    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    _sequencerCurrentFrame = movedStartFrame;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (_sourceMode == SourceMode::LiveModel && playbackModel != nullptr)
    {
        const float currentTimeSec = static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(fps);
        playbackModel->Set_AnimationPaused(true);
        playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
        playbackModel->Play_Animation(0.f);
    }

    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
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

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    if (animationData->notifyTracks.empty())
        animationData->notifyTracks.push_back({ "Notify" });

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

    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void Animation_View::Paste_CopiedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _copiedNotifyState.typeName.empty())
        return;

    Shared<AnimNotifyState> notifyStateInstance = GAME->Create_AnimNotifyStatePrototype(_copiedNotifyState.typeName);
    if (notifyStateInstance == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    notifyStateInstance->From_Json(_copiedNotifyState.payload);

    if (animationData->notifyStateTracks.empty())
        animationData->notifyStateTracks.push_back({ "Notify State" });

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
    Commit_NotifyUndoSnapshot(undoSnapshot);
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
    const bool requestSearchFocus = _openCreateNotifyPopup;

    if (_openCreateNotifyPopup)
    {
        _openCreateNotifyPopup = false;
        _createNotifyFilter[0] = '\0';
        ImGui::OpenPopup("CreateNotifyPopup");
    }

    if (!ImGui::BeginPopup("CreateNotifyPopup"))
        return;

    ImGui::SetNextItemWidth(280.f);
    if (requestSearchFocus)
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##NotifySearch", "Search notify...", _createNotifyFilter, IM_ARRAYSIZE(_createNotifyFilter));
    ImGui::Separator();

    bool hasAny = false;
    bool hasVisible = false;
    GAME->Visit_AnimNotifyTypes([&](const TypeRegistry::AnimNotifyTypeInfo& info)
    {
        hasAny = true;

        if (!Contains_CaseInsensitive(info.className, _createNotifyFilter))
            return;

        hasVisible = true;
        if (ImGui::Selectable(info.className.c_str()))
        {
            Add_Notify_AtFrame(_requestedCreateNotifyFrame, info.className);
            ImGui::CloseCurrentPopup();
        }
    });

    if (!hasAny)
        ImGui::TextDisabled("No AnimNotify types registered.");
    else if (!hasVisible)
        ImGui::TextDisabled("No matching AnimNotify types.");

    ImGui::EndPopup();
}

void Animation_View::Draw_CreateNotifyStatePopup()
{
    const bool requestSearchFocus = _openCreateNotifyStatePopup;

    if (_openCreateNotifyStatePopup)
    {
        _openCreateNotifyStatePopup = false;
        _createNotifyStateFilter[0] = '\0';
        ImGui::OpenPopup("CreateNotifyStatePopup");
    }

    if (!ImGui::BeginPopup("CreateNotifyStatePopup"))
        return;

    ImGui::SetNextItemWidth(280.f);
    if (requestSearchFocus)
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##NotifyStateSearch", "Search notify state...", _createNotifyStateFilter, IM_ARRAYSIZE(_createNotifyStateFilter));
    ImGui::Separator();

    bool hasAny = false;
    bool hasVisible = false;
    GAME->Visit_AnimNotifyStateTypes([&](const TypeRegistry::AnimNotifyStateTypeInfo& info)
    {
        hasAny = true;

        if (!Contains_CaseInsensitive(info.className, _createNotifyStateFilter))
            return;

        hasVisible = true;
        if (ImGui::Selectable(info.className.c_str()))
        {
            Add_State_ByFrameRange(_requestedCreateStateStartFrame, _requestedCreateStateEndFrame, info.className);
            ImGui::CloseCurrentPopup();
        }
    });

    if (!hasAny)
        ImGui::TextDisabled("No AnimNotifyState types registered.");
    else if (!hasVisible)
        ImGui::TextDisabled("No matching AnimNotifyState types.");

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

bool Animation_View::Try_GetSourcePointSample(EffectSourcePointSample& outSample) const
{
    for (const PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
    {
        if (runtime.stateId != _previewSourceHistoryProviderStateId)
            continue;

        return Try_GetSourcePointSampleForRuntime(runtime, outSample);
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

bool Animation_View::Try_GetSourcePointSampleForRuntime(
    const PreviewSourceHistoryRuntime& runtime,
    EffectSourcePointSample& outSample) const
{
    if (runtime.sourceBindingMode == 1)
    {
        return Try_ResolvePreviewSocketWorldPosition(runtime.partTag, runtime.socketName, outSample.worldPosition);
    }

    PreviewTrailRuntime trailRuntime{};
    trailRuntime.partTag = runtime.partTag;
    trailRuntime.trailAnchorAsset = runtime.trailAnchorAsset;

    EffectTrailSample trailSample{};
    if (!Try_GetTrailSampleForRuntime(trailRuntime, trailSample))
        return false;

    switch (runtime.sourcePointPolicy)
    {
    case 1:
        outSample.worldPosition = trailSample.baseWorldPosition;
        return true;
    case 2:
        outSample.worldPosition = (trailSample.baseWorldPosition + trailSample.tipWorldPosition) * 0.5f;
        return true;
    case 0:
    default:
        outSample.worldPosition = trailSample.tipWorldPosition;
        return true;
    }
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

void Animation_View::Update_PreviewSLDodgeMoveStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewSLDodgeMoveStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewSLDodgeMoveStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    if (playbackModel == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewSLDodgeMoveStates();
        return;
    }

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = currentTimeSec;
    context.previousTimeSec = max(0.f, currentTimeSec - max(0.f, timeDelta));
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_SL_DodgeMove")
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
            _previewSLDodgeMoveRuntimes.begin(),
            _previewSLDodgeMoveRuntimes.end(),
            [&](const PreviewSLDodgeMoveRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewSLDodgeMoveRuntimes.end())
        {
            PreviewSLDodgeMoveRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewSLDodgeMoveRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext);
            entry.notifyState->On_Tick(stateContext);
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
        }

        entry.notifyState->On_Tick(stateContext);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewSLDodgeMoveRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewSLDodgeMoveRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewSLDodgeMoveRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            endContext.notifyStateDurationSec = 0.f;
            _previewSLDodgeMoveRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewSLDodgeMoveRuntimes.erase(_previewSLDodgeMoveRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewSLDodgeMoveStates()
{
    if (_previewSLDodgeMoveRuntimes.empty())
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.notifyStateDurationSec = 0.f;
    context.isPreview = true;

    for (PreviewSLDodgeMoveRuntime& runtime : _previewSLDodgeMoveRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(context);
    }

    _previewSLDodgeMoveRuntimes.clear();
}

void Animation_View::Update_PreviewRootControllerStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewRootControllerStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewRootControllerStates();
        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    if (playbackModel == nullptr || previewRootObject == nullptr)
    {
        Reset_PreviewRootControllerStates();
        return;
    }

    const float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = currentTimeSec;
    context.previousTimeSec = max(0.f, currentTimeSec - max(0.f, timeDelta));
    context.deltaTime = timeDelta;
    context.isLooping = true;
    context.wrapped = false;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_RootController")
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
            _previewRootControllerRuntimes.begin(),
            _previewRootControllerRuntimes.end(),
            [&](const PreviewRootControllerRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewRootControllerRuntimes.end())
        {
            PreviewRootControllerRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewRootControllerRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext);
            entry.notifyState->On_Tick(stateContext);
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
        }

        entry.notifyState->On_Tick(stateContext);
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewRootControllerRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewRootControllerRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewRootControllerRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            endContext.notifyStateDurationSec = 0.f;
            _previewRootControllerRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewRootControllerRuntimes.erase(_previewRootControllerRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewRootControllerStates()
{
    if (_previewRootControllerRuntimes.empty())
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

    AnimNotifyContext context{};
    context.owner = previewRootObject.get();
    context.model = playbackModel.get();
    context.modelGuid = _asset.modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.notifyStateDurationSec = 0.f;
    context.isPreview = true;

    for (PreviewRootControllerRuntime& runtime : _previewRootControllerRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(context);
    }

    _previewRootControllerRuntimes.clear();
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

void Animation_View::Ensure_PreviewSkyBackdrop()
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

void Animation_View::Update_PreviewSkyBackdrop(float timeDelta)
{
    if (_previewSkyBox == nullptr)
        return;

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

                Shared<TransformCom> cloudTransform = skyCloudCard->Get_Transform();
                if (cloudTransform != nullptr)
                    cloudTransform->Set_WorldPosition(cameraPos + kPreviewSkyCloudLocalOffset);
            }
        }
    }

    if (!_previewSkyBackdropBegunPlay)
    {
        _previewSkyBox->Try_BeginPlay();
        for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
        {
            if (skyCloudCard != nullptr)
                skyCloudCard->Try_BeginPlay();
        }
        _previewSkyBackdropBegunPlay = true;
    }

    _previewSkyBox->Set_Visible(true);
    _previewSkyBox->Priority_Update(timeDelta);
    _previewSkyBox->Update(timeDelta);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard == nullptr)
            continue;

        skyCloudCard->Set_Visible(true);
        skyCloudCard->Priority_Update(timeDelta);
        skyCloudCard->Update(timeDelta);
    }
}

void Animation_View::Late_Update_PreviewSkyBackdrop(float timeDelta)
{
    if (_previewSkyBox != nullptr)
        _previewSkyBox->Late_Update(timeDelta);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard != nullptr)
            skyCloudCard->Late_Update(timeDelta);
    }
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

void Animation_View::Update_PreviewAnimSpeedStates(float timeDelta)
{
    if (_sourceMode != SourceMode::LiveModel || _previewModelCom == nullptr)
    {
        Reset_PreviewAnimSpeedStates();
        return;
    }

    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
    {
        Reset_PreviewAnimSpeedStates();
        return;
    }

    const float currentTimeSec = _previewModelCom->Get_CurrentAnimationTimeSec();

    AnimNotifyContext context{};
    context.owner = _previewModelCom->Get_Owner().get();
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.currentTimeSec = currentTimeSec;
    context.deltaTime = timeDelta;
    context.isPreview = true;

    vector<int32> activeStateIds{};
    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (entry.notifyState == nullptr)
            continue;
        if (entry.notifyState->Get_TypeName() != "ANS_AnimSpeed")
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
            _previewAnimSpeedStateRuntimes.begin(),
            _previewAnimSpeedStateRuntimes.end(),
            [&](const PreviewAnimSpeedStateRuntime& runtime)
            {
                return runtime.stateId == entry.id;
            });

        if (runtimeIt == _previewAnimSpeedStateRuntimes.end())
        {
            PreviewAnimSpeedStateRuntime runtime{};
            runtime.stateId = entry.id;
            runtime.notifyState = entry.notifyState;
            runtime.payloadJson = payloadJson;
            _previewAnimSpeedStateRuntimes.push_back(runtime);

            entry.notifyState->On_Begin(stateContext); // preview sampling 전에 배속을 먼저 세팅한다.
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
        }
    }

    for (int32 runtimeIndex = static_cast<int32>(_previewAnimSpeedStateRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewAnimSpeedStateRuntimes[runtimeIndex].stateId;
        const bool stillActive = std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();
        if (stillActive)
            continue;

        if (_previewAnimSpeedStateRuntimes[runtimeIndex].notifyState)
        {
            AnimNotifyContext endContext = context;
            endContext.notifyStateProgress = 1.f;
            _previewAnimSpeedStateRuntimes[runtimeIndex].notifyState->On_End(endContext);
        }

        _previewAnimSpeedStateRuntimes.erase(_previewAnimSpeedStateRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewAnimSpeedStates()
{
    if (_previewAnimSpeedStateRuntimes.empty())
        return;

    AnimNotifyContext context{};
    context.owner = _previewModelCom ? _previewModelCom->Get_Owner().get() : nullptr;
    context.model = _previewModelCom.get();
    context.modelGuid = _modelGuid;
    context.animationName = _currentAnimationName;
    context.notifyStateProgress = 1.f;
    context.isPreview = true;

    for (PreviewAnimSpeedStateRuntime& runtime : _previewAnimSpeedStateRuntimes)
    {
        if (runtime.notifyState)
            runtime.notifyState->On_End(context);
    }

    _previewAnimSpeedStateRuntimes.clear();

    if (_previewModelCom)
        _previewModelCom->Set_FrameSpeed(1.f); // preview clone이 남아 있을 때 배속 잔류를 정리한다.
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
        const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();

        for (const AnimNotifyEventEntry& entry : animationData->notifies)
        {
            if (entry.notify == nullptr)
                continue;
            const string typeName = entry.notify->Get_TypeName();
            if (typeName != "AN_PlayEffect" && typeName != "AN_PlaySound")
                continue;
            if (!Is_PreviewNotifyTimeInRange(entry.timeSec, previousTimeSec, currentTimeSec, wrapped))
                continue;

            AnimNotifyContext notifyContext = context;
            if (typeName == "AN_PlayEffect" && previewRootObject != nullptr)
                notifyContext.owner = previewRootObject.get();

            entry.notify->Execute(notifyContext);
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

void Animation_View::Update_PreviewSourceHistoryEffect(float timeDelta)
{
    const vector<const AnimNotifyStateEntry*> activeStates = Find_ActivePreviewSourceHistoryStates();
    if (activeStates.empty())
    {
        _previewSourceHistoryProviderStateId = -1;

        for (PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
            Begin_PreviewSourceHistoryDrain(runtime);

        for (int32 runtimeIndex = static_cast<int32>(_previewSourceHistoryRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
        {
            if (Tick_PreviewSourceHistoryDrain(_previewSourceHistoryRuntimes[runtimeIndex], timeDelta))
                _previewSourceHistoryRuntimes.erase(_previewSourceHistoryRuntimes.begin() + runtimeIndex);
        }

        return;
    }

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const float currentTimeSec = playbackModel ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;
    vector<int32> activeStateIds{};
    activeStateIds.reserve(activeStates.size());

    for (const AnimNotifyStateEntry* activeState : activeStates)
    {
        if (activeState == nullptr || activeState->notifyState == nullptr)
            continue;

        activeStateIds.push_back(activeState->id);

        const json payload = activeState->notifyState->To_Json();
        const string partTag = payload.value("_partTag", string{ "Part_Weapon" });
        const string effectNameValue = payload.value("_effectName", string{});
        const int32 sourceBindingMode = payload.value("_sourceBindingMode", 0);
        const string socketName = payload.value("_socketName", string{});
        const int32 sourcePointPolicy = payload.value("_sourcePointPolicy", 0);
        if (effectNameValue.empty())
            continue;

        const wstring resolvedEffectName = String::ToWString(effectNameValue);

        PreviewSourceHistoryRuntime* runtime = nullptr;
        for (PreviewSourceHistoryRuntime& entry : _previewSourceHistoryRuntimes)
        {
            if (entry.stateId == activeState->id)
            {
                runtime = &entry;
                break;
            }
        }

        if (runtime == nullptr)
        {
            PreviewSourceHistoryRuntime newRuntime{};
            newRuntime.stateId = activeState->id;
            newRuntime.partTag = partTag;
            newRuntime.sourceBindingMode = sourceBindingMode;
            newRuntime.socketName = socketName;
            newRuntime.sourcePointPolicy = sourcePointPolicy;
            _previewSourceHistoryRuntimes.push_back(std::move(newRuntime));
            runtime = &_previewSourceHistoryRuntimes.back();

            if (runtime->sourceBindingMode == 0 && !Load_PreviewSourceHistoryAnchorAsset(runtime->partTag, *runtime))
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

        if (runtime->partTag != partTag ||
            runtime->sourceBindingMode != sourceBindingMode ||
            runtime->socketName != socketName ||
            runtime->sourcePointPolicy != sourcePointPolicy)
        {
            if (runtime->effect)
                runtime->effect->Stop();

            runtime->partTag = partTag;
            runtime->sourceBindingMode = sourceBindingMode;
            runtime->socketName = socketName;
            runtime->sourcePointPolicy = sourcePointPolicy;
            runtime->effect.reset();
            runtime->effectName.clear();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
            runtime->lastTimeSec = -1.f;

            if (runtime->sourceBindingMode == 0 && !Load_PreviewSourceHistoryAnchorAsset(runtime->partTag, *runtime))
                continue;
        }

        if (runtime->sourceBindingMode == 0 && runtime->trailAnchorAsset.modelGuid.empty())
            continue;

        if (!Ensure_PreviewSourceHistoryEffect(*runtime, resolvedEffectName))
            continue;

        const bool timeMovedBackward =
            runtime->lastTimeSec >= 0.f &&
            currentTimeSec + 1e-4f < runtime->lastTimeSec;
        if (timeMovedBackward)
        {
            runtime->effect->Stop();
            runtime->isPlaying = false;
            runtime->isDraining = false;
            runtime->drainElapsed = 0.f;
        }

        EffectSourcePointSample sourceSample{};
        if (!Try_GetSourcePointSampleForRuntime(*runtime, sourceSample))
        {
            if (runtime->effect && runtime->isPlaying)
                Begin_PreviewSourceHistoryDrain(*runtime);
            continue;
        }

        if (!runtime->isPlaying)
        {
            runtime->effect->Bind_SourcePointSampleProvider(static_pointer_cast<IEffectSourcePointSampleProvider>(GetSharedPtr<Animation_View>()));
            if (FAILED(runtime->effect->Reset()))
                continue;

            const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
            if (effectTransform == nullptr)
                continue;

            effectTransform->Set_WorldPosition(sourceSample.worldPosition);

            if (FAILED(runtime->effect->Play()))
                continue;

            runtime->isPlaying = true;
        }

        const Shared<TransformCom> effectTransform = runtime->effect->Get_Transform();
        if (effectTransform == nullptr)
            continue;

        effectTransform->Set_WorldPosition(sourceSample.worldPosition);
        _previewSourceHistoryProviderStateId = runtime->stateId;
        runtime->effect->Priority_Update(timeDelta);
        runtime->effect->Update(timeDelta);
        runtime->lastTimeSec = currentTimeSec;
    }

    _previewSourceHistoryProviderStateId = -1;

    for (int32 runtimeIndex = static_cast<int32>(_previewSourceHistoryRuntimes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        const int32 stateId = _previewSourceHistoryRuntimes[runtimeIndex].stateId;
        const bool stillActive =
            std::find(activeStateIds.begin(), activeStateIds.end(), stateId) != activeStateIds.end();

        if (stillActive)
            continue;

        Begin_PreviewSourceHistoryDrain(_previewSourceHistoryRuntimes[runtimeIndex]);
        if (Tick_PreviewSourceHistoryDrain(_previewSourceHistoryRuntimes[runtimeIndex], timeDelta))
            _previewSourceHistoryRuntimes.erase(_previewSourceHistoryRuntimes.begin() + runtimeIndex);
    }
}

void Animation_View::Reset_PreviewSourceHistoryEffect()
{
    for (PreviewSourceHistoryRuntime& runtime : _previewSourceHistoryRuntimes)
    {
        if (runtime.effect)
            runtime.effect->Stop();
    }

    _previewSourceHistoryRuntimes.clear();
    _previewSourceHistoryProviderStateId = -1;
}

void Animation_View::Begin_PreviewSourceHistoryDrain(PreviewSourceHistoryRuntime& runtime)
{
    if (runtime.effect == nullptr || runtime.isDraining)
        return;

    runtime.isDraining = true;
    runtime.drainElapsed = 0.f;
    runtime.isPlaying = true;
}

bool Animation_View::Tick_PreviewSourceHistoryDrain(PreviewSourceHistoryRuntime& runtime, float timeDelta)
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

vector<const AnimNotifyStateEntry*> Animation_View::Find_ActivePreviewSourceHistoryStates() const
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
        if (entry.notifyState->Get_TypeName() != "ANS_SourceHistoryEffect")
            continue;
        if (currentTimeSec < entry.startSec)
            continue;
        if (currentTimeSec > entry.startSec + entry.durationSec)
            continue;

        activeStates.push_back(&entry);
    }

    return activeStates;
}

bool Animation_View::Ensure_PreviewSourceHistoryEffect(PreviewSourceHistoryRuntime& runtime, const wstring& effectName)
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
    {
        LOG_WARN("Preview SourceHistory effect definition load failed. effect={}", String::ToString(effectName));
        return false;
    }

    runtime.drainDuration = Resolve_SourceHistoryDrainDuration(definition);

    const Shared<EffectInstance> prototype = EffectInstance::Create(GAME->Get_Device(), GAME->Get_Context());
    if (prototype == nullptr)
        return false;

    EffectInstanceDesc effectDesc{};
    effectDesc.definition = definition;

    const Shared<GameObject> previewEffectObject = prototype->Clone(&effectDesc);
    runtime.effect = dynamic_pointer_cast<EffectInstance>(previewEffectObject);
    if (runtime.effect == nullptr)
        return false;

    runtime.effect->Bind_SourcePointSampleProvider(static_pointer_cast<IEffectSourcePointSampleProvider>(GetSharedPtr<Animation_View>()));
    CHECK_FAILED(runtime.effect->Try_BeginPlay(), false);
    CHECK_FAILED(runtime.effect->Reset(), false);

    runtime.effectName = effectName;
    return true;
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
        wstring resolvedEffectName{};
        if (!Try_ResolvePreviewTrailEffectName(effectNameValue, partTag, resolvedEffectName))
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

        if (!Ensure_PreviewTrailEffect(*runtime, resolvedEffectName))
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

bool Animation_View::Try_ResolvePreviewTrailEffectName(
    const string& effectNameOverride,
    const string& partTag,
    wstring& outEffectName) const
{
    outEffectName.clear();

    if (!effectNameOverride.empty())
    {
        outEffectName = String::ToWString(effectNameOverride);
        return true;
    }

    const Weapon* weapon = Resolve_PreviewWeapon(String::ToWString(partTag));
    if (weapon == nullptr)
        return false;

    outEffectName = weapon->Get_TrailEffectName(); // preview도 runtime과 같은 fallback 규칙을 써야 한다.
    return !outEffectName.empty();
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

bool Animation_View::Try_ResolvePreviewSocketWorldPosition(
    const string& partTag,
    const string& socketName,
    Vec3& outWorldPosition) const
{
    if (socketName.empty())
        return false;

    const Shared<GameObject> previewRootObject = Get_PreviewRenderObject();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(previewRootObject);
    if (ownerContainer == nullptr)
        return false;

    const wstring previewPartTag = String::ToWString(partTag);
    PartObject* partObject = ownerContainer->Get_PartObject(previewPartTag);
    if (partObject == nullptr)
        return false;

    if (const auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * playablePart->Get_CombinedWorldMatrix();
        outWorldPosition = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
        return true;
    }

    if (const auto* weaponPart = dynamic_cast<Weapon*>(partObject))
    {
        const Matrix* socketMatrix = weaponPart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * weaponPart->Get_CombinedWorldMatrix();
        outWorldPosition = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
        return true;
    }

    return false;
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
    desc.farPlane = 500.f;
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
