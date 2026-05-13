#include "pch.h"
#include "AnimState_View.h"
#include "Animation_NameUtils.h"
#include "ContainerObject.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "RenderTarget.h"
#include "TransformCom.h"

AnimState_View::AnimState_View()
    : Editor_Window(L"AnimState")
{
    Set_Open(false);
}

void AnimState_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    if (Is_CompanionPreviewFocused())
        return;

    if (_previewCamera && _isPreviewHovered)
        _previewCamera->Priority_Update(timeDelta);
}

void AnimState_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (Is_CompanionPreviewFocused())
        return;

    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    if (previewRenderObject == nullptr)
        return;

    Ensure_PreviewCamera();
    if (_previewCamera == nullptr)
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
    const Matrix previewProj = XMMatrixPerspectiveFovLH(
        _previewCamera->Get_FovY(),
        aspect,
        _previewCamera->Get_Near(),
        _previewCamera->Get_Far());

    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(previewView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(previewProj));

    vector<LIGHT_DESC> previewLightDescs{};

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
    previewRenderObject->Priority_Update(dt);
    previewRenderObject->Update(dt);
    previewRenderObject->Late_Update(dt);

    GAME->Draw_RendererOnly();

    Ensure_PreviewGridResources();
    Draw_PreviewGrid();

    GAME->Set_Lights(savedLightDescs);
    GAME->Set_PlayerInputBlocked(wasPlayerInputBlocked);

    GAME->Bind_BackBuffer();
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
}

void AnimState_View::Render()
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

        Draw_Header();
        ImGui::Separator();

        if (ImGui::BeginTable("AnimStateViewLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("AnimationList", ImGuiTableColumnFlags_WidthFixed, 320.f);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Draw_AnimationSelector();

            ImGui::TableSetColumnIndex(1);
            Draw_PreviewPanel();

            ImGui::EndTable();
        }

        Draw_PlaybackToolbar();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void AnimState_View::Open_FromModelComponent(
    const Shared<ModelCom>& modelCom,
    const string& preferredAnimationName,
    uint32 preferredDockId,
    bool focusOnOpen)
{
    const wstring sourceModelPath = modelCom ? modelCom->Get_SourceModelPath() : wstring{};

    _liveModelCom = modelCom;
    _sourceModelPath = sourceModelPath;
    _modelGuid = _sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = focusOnOpen;

    Refresh_AnimationList();

    if (!preferredAnimationName.empty())
        _currentAnimationName = preferredAnimationName;
    else if (_currentAnimationName.empty() && !_availableAnimations.empty())
        _currentAnimationName = _availableAnimations.front().displayName;

    Sync_PreviewAnimationSelection(true);
    Fit_PreviewCameraToModel();

    Set_Open(true);
}

bool AnimState_View::Is_CompanionPreviewFocused()
{
    Shared<Editor_Window> prefabWindow = EDITOR->Get_Window(L"Prefab");
    if (prefabWindow != nullptr && prefabWindow->Is_Open() && prefabWindow->Is_Focused())
        return true;

    Shared<Editor_Window> animationWindow = EDITOR->Get_Window(L"Animation");
    return animationWindow != nullptr && animationWindow->Is_Open() && animationWindow->Is_Focused();
}

void AnimState_View::Refresh_AnimationList()
{
    _availableAnimations.clear();

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

void AnimState_View::Draw_Header()
{
    const AnimationEntry* currentEntry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    const char* previewName = currentEntry ? currentEntry->displayName.c_str() : "(select animation)";

    if (ImGui::Button("Save"))
    {
    }

    ImGui::SameLine();
    ImGui::Text("Model");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _modelGuid.empty() ? "(none)" : _modelGuid.c_str());
    ImGui::SameLine();
    ImGui::Text("| Source");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _liveModelCom ? "LiveModel" : "None");
    ImGui::SameLine();
    ImGui::Text("| Animation");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", previewName);
}

void AnimState_View::Draw_AnimationSelector()
{
    ImGui::Text("Animation Clips");
    ImGui::Separator();

    if (!ImGui::BeginChild("AnimStateClipBrowserPanel", ImVec2(0.f, 0.f), true))
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
        const bool isSelected =
            (_currentAnimationName == entry.displayName ||
             _currentAnimationName == entry.name ||
             _currentAnimationName == entry.playbackName);

        const string label = Make_AnimationComboLabel(entry);

        if (ImGui::Selectable(label.c_str(), isSelected))
        {
            _currentAnimationName = entry.displayName;
            Sync_PreviewAnimationSelection();
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndChild();
}

void AnimState_View::Draw_PreviewPanel()
{
    ImGui::BeginChild("AnimStatePreviewPanel", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 panelPos = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float toolbarHeight = 0.f;
    const ImVec2 previewSize(max(1.f, avail.x), max(1.f, avail.y - toolbarHeight));

    _previewWidth = max(1u, static_cast<uint32>(previewSize.x));
    _previewHeight = max(1u, static_cast<uint32>(previewSize.y));

    _isPreviewHovered = false;
    _isPreviewActive = false;

    Shared<GameObject> previewRenderObject = Get_PreviewRenderObject();
    Shared<ModelCom> playbackModel = Get_PlaybackModel();

    if (previewRenderObject != nullptr && playbackModel != nullptr && _previewRenderTarget && _previewRenderTarget->Get_SRV())
    {
        ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(_previewRenderTarget->Get_SRV()));
        ImGui::Image(textureId, previewSize);

        _isPreviewHovered = ImGui::IsItemHovered();
        _isPreviewActive = ImGui::IsItemActive();

        if (_isPreviewHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            Fit_PreviewCameraToModel();
    }
    else
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            panelPos,
            ImVec2(panelPos.x + previewSize.x, panelPos.y + previewSize.y),
            IM_COL32(34, 38, 44, 255));

        drawList->AddRect(
            panelPos,
            ImVec2(panelPos.x + previewSize.x, panelPos.y + previewSize.y),
            IM_COL32(85, 95, 110, 255));

        drawList->AddText(
            ImVec2(panelPos.x + 12.f, panelPos.y + 12.f),
            IM_COL32(210, 216, 224, 255),
            "Preview is not ready.");
    }

    ImGui::EndChild();
}

void AnimState_View::Draw_PlaybackToolbar()
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    ImGui::Separator();

    if (ImGui::Button(playbackModel->Is_AnimationPaused() ? "Play" : "Pause", ImVec2(64.f, 0.f)))
        playbackModel->Set_AnimationPaused(!playbackModel->Is_AnimationPaused());

    ImGui::SameLine();

    if (ImGui::Button("Stop", ImVec2(64.f, 0.f)))
    {
        playbackModel->Set_AnimationPaused(true);
        playbackModel->Set_CurrentAnimationTimeSec(0.f);
        playbackModel->Refresh_CurrentAnimationPose();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Grid", &_showPreviewGrid);

    const float durationSec = max(0.01f, playbackModel->Get_CurrentAnimationDurationSec());
    float currentSec = playbackModel->Get_CurrentAnimationTimeSec();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("##AnimStatePreviewTime", &currentSec, 0.f, durationSec, "%.2f"))
    {
        playbackModel->Set_AnimationPaused(true);
        playbackModel->Set_CurrentAnimationTimeSec(currentSec);
        playbackModel->Refresh_CurrentAnimationPose();
    }
}

Shared<ModelCom> AnimState_View::Get_PlaybackModel() const
{
    return _liveModelCom;
}

Shared<GameObject> AnimState_View::Get_PreviewRenderObject() const
{
    if (_liveModelCom == nullptr)
        return nullptr;

    return Resolve_RootPreviewObject(_liveModelCom->Get_Owner());
}

void AnimState_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
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

void AnimState_View::Ensure_PreviewCamera()
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

void AnimState_View::Ensure_PreviewGridResources()
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

void AnimState_View::Fit_PreviewCameraToModel()
{
    if (_previewCamera == nullptr)
        Ensure_PreviewCamera();

    if (_previewCamera == nullptr)
        return;

    _previewCamera->Apply_EditorDesc(Build_PreviewCameraDesc(Get_PreviewRenderObject()));
    Apply_PreviewCameraTransform(_previewCamera);
}

void AnimState_View::Sync_PreviewAnimationSelection(bool keepCurrentTime)
{
    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (playbackModel == nullptr)
        return;

    const AnimationEntry* entry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName);
    if (entry == nullptr)
        return;

    const int32 animationIndex = Find_AnimationIndex(playbackModel, entry->playbackName);
    if (animationIndex < 0)
        return;

    const float currentTime = keepCurrentTime ? playbackModel->Get_CurrentAnimationTimeSec() : 0.f;

    playbackModel->Set_Animation(static_cast<uint32>(animationIndex), true);
    playbackModel->Set_AnimationPaused(false);
    playbackModel->Set_CurrentAnimationTimeSec(currentTime);
    playbackModel->Refresh_CurrentAnimationPose();
}

void AnimState_View::Draw_PreviewGrid() const
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

        DirectX::VertexPositionColor lineXStart(Vec3(-gridHalfExtent, y, offset), colorX);
        DirectX::VertexPositionColor lineXEnd(Vec3(gridHalfExtent, y, offset), colorX);
        _previewGridBatch->DrawLine(lineXStart, lineXEnd);

        DirectX::VertexPositionColor lineZStart(Vec3(offset, y, -gridHalfExtent), colorZ);
        DirectX::VertexPositionColor lineZEnd(Vec3(offset, y, gridHalfExtent), colorZ);
        _previewGridBatch->DrawLine(lineZStart, lineZEnd);
    }

    _previewGridBatch->End();
}

string AnimState_View::Make_AnimationComboLabel(const AnimationEntry& entry)
{
    return entry.displayName + "##" + entry.name;
}

const AnimState_View::AnimationEntry* AnimState_View::Find_AnimationEntryByAnyName(
    const vector<AnimationEntry>& entries,
    const string& name)
{
    for (const AnimationEntry& entry : entries)
    {
        if (entry.name == name || entry.displayName == name || entry.playbackName == name)
            return &entry;
    }

    return nullptr;
}

int32 AnimState_View::Find_AnimationIndex(const Shared<ModelCom>& modelCom, const string& animationName)
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

Shared<GameObject> AnimState_View::Resolve_RootPreviewObject(const Shared<GameObject>& gameObject)
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

Editor_Camera_Free::EDITOR_CAMERA_DESC AnimState_View::Build_PreviewCameraDesc(const Shared<GameObject>& gameObject)
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

void AnimState_View::Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera)
{
    if (camera == nullptr)
        return;

    Shared<TransformCom> cameraTransform = camera->Get_Transform();
    if (cameraTransform == nullptr)
        return;

    cameraTransform->Set_WorldPosition(Vec3(-1.35f, 2.25f, 6.2f));
    cameraTransform->LookAt(Vec3(-1.6f, -1.2f, -2.3f));
}

Shared<AnimState_View> AnimState_View::Create()
{
    return make_shared<AnimState_View>();
}
