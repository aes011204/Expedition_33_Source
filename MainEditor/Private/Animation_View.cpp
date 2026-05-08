#include "Animation_View.h"
#include "Animation_PreviewObject.h"
#include "AnimSequencerAdapter.h"
#include "AnimNotify_Parser.h"
#include "ContainerObject.h"
#include "EditorInstance.h"
#include "Editor_Camera_Free.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "RenderTarget.h"

static int32 Find_AnimationIndex(const Shared<ModelCom>& modelCom, const string& animationName)
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

static float Resolve_AnimNotifyDataLengthSec(const AnimNotifyData* animationData)
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

static Shared<GameObject> Resolve_RootPreviewObject(const Shared<GameObject>& gameObject)
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

static Vec3 Resolve_PreviewForward(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return Vec3::Look;

    Shared<TransformCom> transformCom = gameObject->Get_Transform();
    if (transformCom == nullptr)
        return Vec3::Look;

    Vec3 forward = transformCom->Get_WorldForward();
    forward.y = 0.f;

    if (forward.LengthSquared() <= 0.000001f)
        return Vec3::Look;

    forward.Normalize();
    return forward;
}

static Editor_Camera_Free::EDITOR_CAMERA_DESC Build_PreviewCameraDesc(const Shared<GameObject>& gameObject)
{
    Editor_Camera_Free::EDITOR_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(0.f, 1.45f, -2.35f, 1.f);
    desc.at = XMFLOAT4(0.f, 1.1f, 0.f, 1.f);
    desc.fovy = XM_PIDIV4;
    desc.nearPlane = 0.1f;
    desc.farPlane = 100.f;
    desc.yaw = 0.f;
    desc.pitch = 0.f;
    desc.mouseSensor = 0.15f;
    desc.speedPerSec = 5.f;
    desc.degreePerSec = 180.f;

    if (gameObject == nullptr)
        return desc;

    const Shared<TransformCom> transformCom = gameObject->Get_Transform();
    const Vec3 center = transformCom ? transformCom->Get_WorldPosition() : Vec3::Zero;
    const Vec3 forward = Resolve_PreviewForward(gameObject);
    const Vec3 focus = center + Vec3(0.f, 1.1f, 0.f);
    const Vec3 eye = focus - forward * 2.35f + Vec3(0.f, 0.35f, 0.f);

    desc.eye = XMFLOAT4(eye.x, eye.y, eye.z, 1.f);
    desc.at = XMFLOAT4(focus.x, focus.y, focus.z, 1.f);
    return desc;
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

    Ensure_PreviewObject();
    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    if (previewRenderObject == nullptr || _previewCamera == nullptr)
        return;

    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);
    Ensure_PreviewGridResources();
    if (_previewRenderTarget == nullptr)
        return;

    if (_sourceMode == SourceMode::AssetOnly && !_previewHasBegunPlay)
    {
        previewRenderObject->Try_BeginPlay();
        _previewHasBegunPlay = true;
    }

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
    if (_sourceMode == SourceMode::AssetOnly)
    {
        previewRenderObject->Priority_Update(dt);
        previewRenderObject->Update(dt);
    }

    previewRenderObject->Late_Update(dt);

    GAME->Draw_RendererOnly();
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
        ImGui::SetNextWindowDockID(_pendingDockId, ImGuiCond_Always);
    if (_requestFocusOnOpen)
        ImGui::SetNextWindowFocus();

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

        Draw_Header();
        ImGui::Separator();

        Draw_AnimationSelector();
        ImGui::Separator();

        if (ImGui::BeginTable("AnimationViewTopLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 2.3f);
            ImGui::TableSetupColumn("SelectedDetail", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Draw_PreviewPanel();

            ImGui::TableSetColumnIndex(1);
            Draw_SelectedDetail();

            ImGui::EndTable();
        }

        ImGui::Separator();
        Draw_Sequencer();
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
    const bool reusePreview = (_sourceModelPath == modelPath && _previewObject != nullptr);

    _sourceMode = SourceMode::AssetOnly;
    _sourceModelPath = modelPath;
    _liveModelCom.reset();
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = true;
    _requestAutoPlayOnOpen = true;

    if (!reusePreview)
    {
        _previewObject.reset();
        _previewModelCom.reset();
        _previewHasBegunPlay = false;
    }

    _modelGuid = modelGuid;
    _currentAnimationName = preferredAnimationName;
    _selectedNotifyId = -1;
    _selectedStateId = -1;
    _isDirty = false;

    Refresh_Source();
    Load_NotifyAsset();

    if (reusePreview)
    {
        Sync_PreviewAnimationSelection();
        if (_previewModelCom != nullptr)
            _previewModelCom->Set_AnimationPaused(false);
        _requestAutoPlayOnOpen = false;
    }

    Set_Open(true);
}

void Animation_View::Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName, uint32 preferredDockId)
{
    const wstring sourceModelPath = modelCom ? modelCom->Get_SourceModelPath() : wstring{};

    _sourceMode = SourceMode::LiveModel;
    _liveModelCom = modelCom;
    _sourceModelPath = sourceModelPath;
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = true;
    _requestAutoPlayOnOpen = false;
    _previewObject.reset();
    _previewModelCom.reset();
    _previewHasBegunPlay = false;

    _modelGuid = _sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _currentAnimationName = preferredAnimationName;
    _selectedNotifyId = -1;
    _selectedStateId = -1;
    _isDirty = false;

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
    if (_previewModelCom != nullptr)
        return _previewModelCom->Get_CurrentAnimationDurationSec();

    if (_liveModelCom != nullptr)
        return _liveModelCom->Get_CurrentAnimationDurationSec();

    return Resolve_AnimNotifyDataLengthSec(Get_CurrentAnimation());
}

void Animation_View::Refresh_Source()
{
    _availableAnimationNames.clear();

    if (_previewModelCom != nullptr)
    {
        const uint32 numAnimations = static_cast<uint32>(_previewModelCom->Get_NumAnimations());
        for (uint32 i = 0; i < numAnimations; ++i)
        {
            if (const char* animationName = _previewModelCom->Get_AnimationName(i))
                _availableAnimationNames.push_back(animationName);
        }
    }
    else if (_sourceMode == SourceMode::LiveModel)
    {
        Refresh_LiveAnimationList();
    }
    else
    {
        Refresh_AssetAnimationList();
    }

    if (_currentAnimationName.empty() && !_availableAnimationNames.empty())
        _currentAnimationName = _availableAnimationNames.front();
}

void Animation_View::Refresh_AssetAnimationList()
{
    if (_sourceModelPath.empty())
        return;

    const fs::path modelDir = fs::path(_sourceModelPath).parent_path();
    if (!fs::exists(modelDir))
        return;

    const auto scanDirectory = [this](const fs::path& directory)
    {
        if (!fs::exists(directory) || !fs::is_directory(directory))
            return;

        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".anim")
                continue;

            _availableAnimationNames.push_back(entry.path().stem().string());
        }
    };

    scanDirectory(modelDir);
    scanDirectory(modelDir / "Animations");

    ::sort(_availableAnimationNames.begin(), _availableAnimationNames.end());
    _availableAnimationNames.erase(
        ::unique(_availableAnimationNames.begin(), _availableAnimationNames.end()),
        _availableAnimationNames.end());
}

void Animation_View::Refresh_LiveAnimationList()
{
    if (_liveModelCom == nullptr)
        return;

    const uint32 numAnimations = static_cast<uint32>(_liveModelCom->Get_NumAnimations());
    for (uint32 i = 0; i < numAnimations; ++i)
    {
        if (const char* animationName = _liveModelCom->Get_AnimationName(i))
            _availableAnimationNames.push_back(animationName);
    }
}

void Animation_View::Load_NotifyAsset()
{
    _asset = {};
    _asset.modelGuid = _modelGuid;

    const fs::path filePath = AnimNotify_Parser::Get_ModelNotifyFilePath(_modelGuid);
    AnimNotify_Parser::Load_FromFile(filePath.wstring(), _asset);

    if (_asset.modelGuid.empty())
        _asset.modelGuid = _modelGuid;

    if (!_currentAnimationName.empty())
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
        _isDirty = false;
}

void Animation_View::Draw_Header()
{
    ImGui::Text("Model GUID");
    ImGui::SameLine(120.f);
    ImGui::TextWrapped("%s", _modelGuid.empty() ? "(none)" : _modelGuid.c_str());

    ImGui::Text("Source");
    ImGui::SameLine(120.f);

    const char* sourceLabel = "None";
    if (_sourceMode == SourceMode::AssetOnly)
        sourceLabel = "Asset";
    else if (_sourceMode == SourceMode::LiveModel)
        sourceLabel = "LiveModel";

    ImGui::TextUnformatted(sourceLabel);

    ImGui::Text("Animation");
    ImGui::SameLine(120.f);
    ImGui::TextWrapped("%s", _currentAnimationName.empty() ? "(none)" : _currentAnimationName.c_str());
}

void Animation_View::Draw_AnimationSelector()
{
    const char* previewName = _currentAnimationName.empty() ? "(select animation)" : _currentAnimationName.c_str();

    ImGui::SetNextItemWidth(280.f);
    if (ImGui::BeginCombo("Animation", previewName))
    {
        for (const string& animationName : _availableAnimationNames)
        {
            const bool isSelected = (_currentAnimationName == animationName);

            if (ImGui::Selectable(animationName.c_str(), isSelected))
            {
                _currentAnimationName = animationName;
                AnimNotify_Parser::Get_OrAddAnimation(_asset, _currentAnimationName);
                _selectedNotifyId = -1;
                _selectedStateId = -1;
                _sequencerCurrentFrame = 0;
                _sequencerSelectedRow = -1;

                Sync_PreviewAnimationSelection();

                if (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr)
                {
                    const int32 animationIndex = Find_AnimationIndex(_liveModelCom, animationName);
                    if (animationIndex >= 0)
                    {
                        _liveModelCom->Set_Animation(static_cast<uint32>(animationIndex), true);
                        _liveModelCom->Set_AnimationPaused(true);
                        _liveModelCom->Set_CurrentAnimationTimeSec(0.f);
                        _liveModelCom->Refresh_CurrentAnimationPose();
                    }
                }
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("FPS: %d", Get_CurrentAnimationFps());
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

        if (ImGui::Button("Frame", playbackButtonSize))
            Fit_PreviewCameraToModel();

        ImGui::SameLine();

        ImGui::Checkbox("Grid", &_showPreviewGrid);

        ImGui::SameLine();

        float currentTimeSec = playbackModel->Get_CurrentAnimationTimeSec();
        ImGui::SetNextItemWidth(260.f);
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
        ImGui::Button("Frame", playbackButtonSize);
        ImGui::SameLine();
        ImGui::Checkbox("Grid", &_showPreviewGrid);
        ImGui::SameLine();

        float dummyTimeSec = 0.f;
        ImGui::SetNextItemWidth(260.f);
        ImGui::SliderFloat("##AnimScrub", &dummyTimeSec, 0.f, sliderMaxTimeSec, "%.3f s");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    if (ImGui::Button("Save"))
        Save_NotifyAsset();

    ImGui::SameLine();

    if (ImGui::Button("Add Notify"))
        Add_Notify_AtCurrentTime();

    ImGui::SameLine();

    if (ImGui::Button("Add NotifyState"))
        Add_State_AtCurrentTime();

    ImGui::SameLine();

    if (ImGui::Button("Delete Selected"))
        Delete_Selected();
}

void Animation_View::Draw_PreviewPanel()
{
    const float previewPanelHeight = max(420.f, ImGui::GetContentRegionAvail().y * 0.58f);
    ImGui::BeginChild("AnimationPreviewPanel", ImVec2(0.f, previewPanelHeight), false);

    ImGui::TextUnformatted("Preview");
    ImGui::Separator();

    const float toolbarHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y + 8.f;
    ImVec2 previewSize = ImGui::GetContentRegionAvail();
    previewSize.y = max(1.f, previewSize.y - toolbarHeight);

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

    ImGui::SetCursorScreenPos(ImVec2(previewPos.x, previewPos.y + previewSize.y + ImGui::GetStyle().ItemSpacing.y));
    Draw_PlaybackToolbar();

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
}

void Animation_View::Draw_SelectedDetail()
{
    const float detailMaxTimeSec = max(0.01f, Get_CurrentAnimationLengthSec());

    if (AnimNotifyEventEntry* notify = Find_SelectedNotify())
    {
        char nameBuffer[128]{};
        strcpy_s(nameBuffer, notify->name.c_str());

        ImGui::Text("Selected Notify");

        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            notify->name = nameBuffer;
            _isDirty = true;
        }

        if (ImGui::DragFloat("Time", &notify->timeSec, 0.01f, 0.f, detailMaxTimeSec))
            _isDirty = true;

        return;
    }

    if (AnimNotifyStateEntry* notifyState = Find_SelectedState())
    {
        char nameBuffer[128]{};
        strcpy_s(nameBuffer, notifyState->name.c_str());

        ImGui::Text("Selected NotifyState");

        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            notifyState->name = nameBuffer;
            _isDirty = true;
        }

        if (ImGui::DragFloat("Start", &notifyState->startSec, 0.01f, 0.f, detailMaxTimeSec))
            _isDirty = true;

        if (ImGui::DragFloat("Duration", &notifyState->durationSec, 0.01f, 0.f, detailMaxTimeSec))
            _isDirty = true;

        return;
    }

    ImGui::TextDisabled("No notify selected.");
}

void Animation_View::Add_Notify_AtCurrentTime()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const float currentTimeSec =
        (playbackModel != nullptr)
        ? playbackModel->Get_CurrentAnimationTimeSec()
        : static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(max(1, Get_CurrentAnimationFps()));

    AnimNotifyEventEntry entry{};
    entry.id = animationData->nextNotifyId++;
    entry.timeSec = currentTimeSec;
    entry.trackIndex = 0;
    entry.name = "NewNotify";

    animationData->notifies.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    _isDirty = true;
}

void Animation_View::Add_State_AtCurrentTime()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    const float currentTimeSec =
        (playbackModel != nullptr)
        ? playbackModel->Get_CurrentAnimationTimeSec()
        : static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(max(1, Get_CurrentAnimationFps()));

    AnimNotifyStateEntry entry{};
    entry.id = animationData->nextStateId++;
    entry.startSec = currentTimeSec;
    entry.durationSec = 0.2f;
    entry.trackIndex = 0;
    entry.name = "NewNotifyState";

    animationData->notifyStates.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    _isDirty = true;
}

void Animation_View::Delete_Selected()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyId != -1)
    {
        auto begin = animationData->notifies.begin();
        auto end = animationData->notifies.end();

        auto it = std::remove_if(
            begin,
            end,
            [this](const AnimNotifyEventEntry& entry)
            {
                return entry.id == _selectedNotifyId;
            });

        if (it != end)
        {
            animationData->notifies.erase(it, end);
            _selectedNotifyId = -1;
            _isDirty = true;
        }

        return;
    }

    if (_selectedStateId != -1)
    {
        auto begin = animationData->notifyStates.begin();
        auto end = animationData->notifyStates.end();

        auto it = std::remove_if(
            begin,
            end,
            [this](const AnimNotifyStateEntry& entry)
            {
                return entry.id == _selectedStateId;
            });

        if (it != end)
        {
            animationData->notifyStates.erase(it, end);
            _selectedStateId = -1;
            _isDirty = true;
        }
    }
}

Shared<ModelCom> Animation_View::Get_PlaybackModel() const
{
    if (_previewModelCom != nullptr)
        return _previewModelCom;

    return _liveModelCom;
}

void Animation_View::Ensure_PreviewObject()
{
    if (_sourceModelPath.empty())
        return;

    if (_sourceMode == SourceMode::LiveModel)
    {
        Ensure_PreviewCamera();
        return;
    }

    if (_previewObject == nullptr)
    {
        Shared<Animation_PreviewObject> previewObject = Animation_PreviewObject::Create(GAME->Get_Device(), GAME->Get_Context());
        if (previewObject == nullptr)
            return;

        if (!previewObject->Set_ModelPath(_sourceModelPath))
            return;

        _previewObject = previewObject;
        _previewModelCom = previewObject->Get_ModelComponent();

        if (Shared<TransformCom> previewTransform = _previewObject->Get_Transform())
            previewTransform->Set_Position(Vec3::Zero);

        Ensure_PreviewCamera();
        Fit_PreviewCameraToModel();
        Refresh_Source();

        if (Find_AnimationIndex(_previewModelCom, _currentAnimationName) < 0 && !_availableAnimationNames.empty())
            _currentAnimationName = _availableAnimationNames.front();

        if (!_currentAnimationName.empty())
            AnimNotify_Parser::Get_OrAddAnimation(_asset, _currentAnimationName);

        Sync_PreviewAnimationSelection();

        if (_requestAutoPlayOnOpen && _previewModelCom != nullptr)
        {
            _previewModelCom->Set_AnimationPaused(false);
            _requestAutoPlayOnOpen = false;
        }
    }
}

void Animation_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
    width = max(1u, width);
    height = max(1u, height);

    if (_previewRenderTarget == nullptr)
    {
        _previewRenderTarget = RenderTarget::Create(
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

void Animation_View::Ensure_PreviewGridResources()
{
    if (_previewGridBatch && _previewGridEffect && _previewGridInputLayout && _previewGridDepthState)
        return;

    static constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
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
        inputElements,
        static_cast<UINT>(_countof(inputElements)),
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
}

Shared<GameObject> Animation_View::Get_PreviewRenderObject() const
{
    if (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr)
        return Resolve_RootPreviewObject(_liveModelCom->Get_Owner());

    return _previewObject;
}

void Animation_View::Fit_PreviewCameraToModel()
{
    if (_previewCamera == nullptr)
        return;

    _previewCamera->Apply_EditorDesc(Build_PreviewCameraDesc(Get_PreviewRenderObject()));
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

        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(Vec3(-gridHalfExtent, y, offset), colorX),
            DirectX::VertexPositionColor(Vec3(gridHalfExtent, y, offset), colorX));

        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(Vec3(offset, y, -gridHalfExtent), colorZ),
            DirectX::VertexPositionColor(Vec3(offset, y, gridHalfExtent), colorZ));
    }

    _previewGridBatch->End();
}

void Animation_View::Sync_PreviewAnimationSelection(bool keepCurrentTime)
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    const int32 animationIndex = Find_AnimationIndex(playbackModel, _currentAnimationName);
    if (animationIndex < 0)
        return;

    const float currentTimeSec = keepCurrentTime ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;
    playbackModel->Set_Animation(static_cast<uint32>(animationIndex), true);
    playbackModel->Set_AnimationPaused(_sourceMode == SourceMode::LiveModel ? false : true);
    playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
    playbackModel->Refresh_CurrentAnimationPose();
    _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
}

AnimNotifyData* Animation_View::Get_CurrentAnimation()
{
    if (_currentAnimationName.empty())
        return nullptr;

    return AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName);
}

const AnimNotifyData* Animation_View::Get_CurrentAnimation() const
{
    if (_currentAnimationName.empty())
        return nullptr;

    return AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName);
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
