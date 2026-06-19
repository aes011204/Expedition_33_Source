#include "pch.h"
#include "AnimState_View.h"
#include "Animation_NameUtils.h"
#include "ContainerObject.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "PreviewObjectHelpers.h"
#include "RenderTarget.h"
#include "SkyBoxFlow.h"
#include "SkyCloudCard.h"
#include "TransformCom.h"
#include "AnimState_Parser.h"

namespace
{
const Vec3 kPreviewSkyCloudLocalOffset = Vec3(28.f, 34.f, 82.f);
}

AnimState_View::AnimState_View()
    : Editor_Window(L"AnimState")
{
    Set_Open(false);
}

bool AnimState_View::CanSave() const
{
	// 에디터의 전역 Save 기능(Ctrl+S 등)이 호출될 수 있는지 여부
	return strlen(_mappingSaveLoadBuffer) > 0 && !_stateNames.empty();
}

void AnimState_View::Save()
{
	if (CanSave())
	{
		Save_Mapping(_mappingSaveLoadBuffer);
	}
}

void AnimState_View::Update(float timeDelta)
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
}

void AnimState_View::Pre_Render()
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
    if (previewRenderObject == nullptr)
        return;

    Ensure_PreviewCamera();
    if (_previewCamera == nullptr)
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
    const Matrix previewProj = XMMatrixPerspectiveFovLH(
        _previewCamera->Get_FovY(),
        aspect,
        _previewCamera->Get_Near(),
        _previewCamera->Get_Far());

    GAME->Change_ActiveCamera(_previewCamera);
    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(previewView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(previewProj));
    GAME->UpdateFrustum(previewView, previewProj);

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
    if (_showPreviewSkyBackdrop)
        Update_PreviewSkyBackdrop(dt);

    GAME->Backup_RenderGroup();
    if (_showPreviewSkyBackdrop)
        Late_Update_PreviewSkyBackdrop(dt);

    PreviewObjectHelpers::Update_PreviewObject(previewRenderObject, dt);

    GAME->Draw_RendererOnly();
    GAME->Restore_RenderGroup();

    Ensure_PreviewGridResources();
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
    _isVisible = false;

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isVisible = true;
        _pendingDockId = 0;
        _requestFocusOnOpen = false;
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Draw_Header();
        ImGui::Separator();

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float minTopHeight = 240.f;
        const float maxTopHeight = max(minTopHeight, availableHeight - 200.f);
        _topSectionHeight = std::clamp(_topSectionHeight, minTopHeight, maxTopHeight);

        if (ImGui::BeginChild("AnimStateTopSection", ImVec2(0.f, _topSectionHeight), false))
        {
            if (ImGui::BeginTable("AnimStateViewLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("AnimationList", ImGuiTableColumnFlags_WidthFixed, 320.f);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("EnumSetting", ImGuiTableColumnFlags_WidthFixed, 220.f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                Draw_AnimationSelector();

                ImGui::TableSetColumnIndex(1);
                Draw_PreviewPanel();

                ImGui::TableSetColumnIndex(2);
                Draw_EnumSettingPanel();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // Splitter
        const ImVec2 splitterPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("AnimStateTopBottomSplitter", ImVec2(-FLT_MIN, 6.f));
        if (ImGui::IsItemActive())
            _topSectionHeight = std::clamp(
                _topSectionHeight + ImGui::GetIO().MouseDelta.y,
                minTopHeight, maxTopHeight);

        const bool splitterHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitterPos,
            ImVec2(splitterPos.x + ImGui::GetContentRegionAvail().x, splitterPos.y + 6.f),
            splitterHovered ? IM_COL32(110, 110, 118, 255) : IM_COL32(72, 72, 78, 255),
            2.f);

        Draw_PlaybackToolbar();

        if (!_stateNames.empty())
        {
            ImGui::Separator();
			
            // 왼쪽 테이블 자식 윈도우
            ImGui::BeginChild("AnimStateMappingTablePanel", ImVec2(ImGui::GetContentRegionAvail().x - 260.f, 0.f), false);
            Draw_MappingTable();
            ImGui::EndChild();

            ImGui::SameLine();

            // 오른쪽 Save/Load 자식 윈도우
            ImGui::BeginChild("AnimStateSaveLoadPanel", ImVec2(250.f, 0.f), true);
            Draw_MappingSaveLoadPanel();
            ImGui::EndChild();
        }
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

    Reset_PreviewClone();
    _liveModelCom = modelCom;
    _sourceModelPath = sourceModelPath;
    _modelGuid = _sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _pendingDockId = preferredDockId;
    _requestFocusOnOpen = focusOnOpen;
    _animationSearchBuffer[0] = '\0';

    Refresh_AnimmapFiles();
    Refresh_AnimationList();
    Rebuild_PreviewClone();

    if (!preferredAnimationName.empty())
        _currentAnimationName = preferredAnimationName;
    else if (_currentAnimationName.empty() && !_availableAnimations.empty())
        _currentAnimationName = _availableAnimations.front().displayName;

    Sync_PreviewAnimationSelection(true);
    Fit_PreviewCameraToModel();

    Set_Open(true);
}

void AnimState_View::Clear_LivePreviewBinding(const Shared<GameObject>& previewRoot)
{
    if (previewRoot != nullptr)
    {
        const Shared<GameObject> currentPreviewRoot =
            _liveModelCom ? Resolve_RootPreviewObject(_liveModelCom->Get_Owner()) : nullptr;
        if (currentPreviewRoot != previewRoot)
            return;
    }

    Reset_PreviewClone();
    _sourceModelPath.clear();
    _liveModelCom.reset();
    _modelGuid.clear();
    _currentAnimationName.clear();
    _availableAnimations.clear();
    _animationSearchBuffer[0] = '\0';
    _stateNames.clear();
    _stateIndices.clear();
    _animationMapping.clear();
    _selectedAnimmapFileIndex = -1;
}

void AnimState_View::Rebuild_PreviewClone()
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

    Shared<TransformCom> previewTransform = _previewRootObject->Get_Transform();
    if (previewTransform != nullptr)
        previewTransform->Set_WorldPosition(Vec3::Zero);

    const Shared<GameObject> previewOwnerObject =
        PreviewObjectHelpers::Find_GameObjectByKey(_previewRootObject, _previewOwnerObjectKey);
    _previewModelCom =
        PreviewObjectHelpers::Find_ModelComponentBySourcePath(previewOwnerObject, _sourceModelPath);
}

void AnimState_View::Reset_PreviewClone()
{
    _previewRootObject.reset();
    _previewModelCom.reset();
    _previewOwnerObjectKey.clear();
}

bool AnimState_View::Should_DeferToCompanionPreview()
{
    return !EDITOR->Is_ActivePreviewOwner(PreviewOwner::AnimState);
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
    ImGui::InputTextWithHint("##AnimStateClipSearch", "Search animation...", _animationSearchBuffer, IM_ARRAYSIZE(_animationSearchBuffer));
    const string searchTextLower = String::ToLowerCopy(string(_animationSearchBuffer));

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
            Sync_PreviewAnimationSelection();
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload(
                "ANIMSTATE_CLIP",
                entry.name.c_str(),
                entry.name.size() + 1);
            ImGui::Text("Assign: %s", entry.displayName.c_str());
            ImGui::EndDragDropSource();
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndChild();
}

void AnimState_View::Draw_PreviewPanel()
{
    ImGui::BeginChild("AnimStatePreviewPanel", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::Checkbox("SkyBox", &_showPreviewSkyBackdrop);
    ImGui::Separator();

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

    ID3D11ShaderResourceView* previewSRV =
        _previewRenderTarget != nullptr ? _previewRenderTarget->Get_SRV() : nullptr;
    if (previewRenderObject != nullptr && playbackModel != nullptr && previewSRV != nullptr)
    {
        ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSRV));
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
    if (_previewModelCom != nullptr)
        return _previewModelCom;

    return _liveModelCom;
}

Shared<GameObject> AnimState_View::Get_PreviewRenderObject() const
{
    if (_previewRootObject != nullptr)
        return _previewRootObject;

    if (_liveModelCom == nullptr)
        return nullptr;

    return Resolve_RootPreviewObject(_liveModelCom->Get_Owner());
}

void AnimState_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
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

void AnimState_View::Ensure_PreviewSkyBackdrop()
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

void AnimState_View::Update_PreviewSkyBackdrop(float timeDelta)
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

void AnimState_View::Late_Update_PreviewSkyBackdrop(float timeDelta)
{
    if (_previewSkyBox != nullptr)
        _previewSkyBox->Late_Update(timeDelta);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard != nullptr)
            skyCloudCard->Late_Update(timeDelta);
    }
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

void AnimState_View::Draw_EnumSettingPanel()
{
	ImGui::BeginChild("EnumSettingPanel", ImVec2(0.f, 0.f), true);
	ImGui::Text("Enum Setting");
	ImGui::Separator();

	// 검색 입력
	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputText("##EnumSearch", _enumSearchBuffer, sizeof(_enumSearchBuffer));

	const string searchStr = _enumSearchBuffer;

	vector<EnumSearchResult> results;

	if (!searchStr.empty())
	{
		Search_EnumProperties(searchStr, results);
	}
	else
	{
		GAME->Visit_GlobalEnums([&](const Engine::TypeRegistry::GlobalEnumInfo& info) {
			EnumSearchResult result;
			result.ownerClassName = "GlobalEnum";
			result.propertyDisplayName = info.displayName;
			result.enumNames = info.enumNames;
			result.enumValues = info.enumValues;
			results.push_back(std::move(result));
		});
	}

	ImGui::Separator();

	if (results.empty())
	{
		ImGui::TextDisabled(searchStr.empty() ? "No Global Enums registered." : "No matching enum found.");
	}
	else
	{
		ImGui::TextDisabled("Found %d:", static_cast<int32>(results.size()));

		for (const auto &result : results)
		{
			// "ClassName::PropertyName" 형태로 표시
			const string label = result.ownerClassName + "::" + result.propertyDisplayName;
			const bool isCurrent = (_enumTypeName == label);

			if (isCurrent)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.f, 1.f));

			if (ImGui::Selectable(label.c_str(), isCurrent))
			{
				Apply_EnumFromProperty(label, result.enumNames, result.enumValues);
			}

			if (isCurrent)
				ImGui::PopStyleColor();

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				for (size_t i = 0; i < result.enumNames.size(); ++i)
				{
					ImGui::Text("%d: %s", result.enumValues[i], result.enumNames[i].c_str());
				}
				ImGui::EndTooltip();
			}
		}
	}

	ImGui::Separator();

	// 현재 바인딩 정보
	if (!_enumTypeName.empty())
	{
		ImGui::Text("Bound:");
		ImGui::SameLine();
		ImGui::TextColored(
			ImVec4(0.4f, 0.8f, 1.f, 1.f),
			"%s", _enumTypeName.c_str());
		ImGui::TextDisabled("%d states", static_cast<int32>(_stateNames.size()));
	}
	else
	{
		ImGui::TextColored(
			ImVec4(1.f, 0.6f, 0.2f, 1.f),
			"No Enum bound.");
	}

	ImGui::EndChild();
}

void AnimState_View::Draw_MappingTable()
{
    if (!ImGui::BeginTable("AnimStateMappingTable", 3,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY,
        ImVec2(0.f, 0.f)))
    {
        return;
    }
    ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50.f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 160.f);
    ImGui::TableSetupColumn("Animation", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < _stateNames.size(); ++i)
    {
        const string& stateName = _stateNames[i];
        const int32 stateIndex = _stateIndices[i];
        string& mappedAnim = _animationMapping[stateName];
        ImGui::TableNextRow();
        ImGui::PushID(static_cast<int32>(i));
        // ── Index ──
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", stateIndex);
        // ── State Name ──
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(stateName.c_str());
        // ── Animation (InputText + DropTarget) ──
        ImGui::TableSetColumnIndex(2);
        char buffer[256] = "";
        strncpy_s(buffer, mappedAnim.c_str(), _TRUNCATE);
        // X 버튼 공간 확보
        const float clearBtnWidth = mappedAnim.empty() ? 0.f : 24.f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clearBtnWidth);
        if (ImGui::InputText("##Anim", buffer, sizeof(buffer)))
        {
            mappedAnim = buffer;
        }
        
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("ANIMSTATE_CLIP"))
            {
                const char* droppedName =
                    static_cast<const char*>(payload->Data);
                mappedAnim = droppedName;
            }
            ImGui::EndDragDropTarget();
        }
        // X 버튼 (매핑 해제)
        if (!mappedAnim.empty())
        {
            ImGui::SameLine(0.f, 2.f);
            if (ImGui::SmallButton("X"))
            {
                mappedAnim.clear();
            }
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void AnimState_View::Save_Mapping(const string &fileName)
{
	if (fileName.empty() || _sourceModelPath.empty())
	{
		return;
	}

    Refresh_BoundEnumBeforeSave();

	fs::path rootPath = GAME->Get_AssetRoot();
	fs::path modelDir = rootPath / fs::path(_sourceModelPath).parent_path();
	fs::path savePath = modelDir / (fileName + ".animmap.json");

	// 최상위 프리팹 이름 획득
	string prefabName = "";
	if (_liveModelCom != nullptr && _liveModelCom->Get_Owner() != nullptr)
	{
		auto owner = _liveModelCom->Get_Owner();
		if (auto containerOwner = owner->Get_Owner())
		{
			if (!containerOwner->Get_SourcePrefabName().empty())
			{
				prefabName = containerOwner->Get_SourcePrefabName(); // Body_Player에 남은 PB_*보다 루트 프리팹 이름을 우선 저장한다.
			}
			else
			{
				string selfClass = containerOwner->Get_ReflectionInfo().className;
				string ownerClass = "";
				if (auto ownerOfContainer = containerOwner->Get_Owner())
				{
					ownerClass = ownerOfContainer->Get_ReflectionInfo().className;
				}
				prefabName = AnimState_Parser::Find_PrefabName_ByStaticClass(ownerClass, selfClass);
				if (!prefabName.empty())
				{
					containerOwner->Set_SourcePrefabName(prefabName);
				}
			}
		}
		else if (!owner->Get_SourcePrefabName().empty())
		{
			prefabName = owner->Get_SourcePrefabName();
		}
		else
		{
			string selfClass = owner->Get_ReflectionInfo().className;
			prefabName = AnimState_Parser::Find_PrefabName_ByStaticClass("", selfClass);
			if (!prefabName.empty())
			{
				owner->Set_SourcePrefabName(prefabName);
			}
		}
	}
    unordered_map<string, string> resolvedMapping;
    for (size_t i = 0; i < _stateNames.size(); ++i)
    {
        const auto it = _animationMapping.find(_stateNames[i]);
        if (it == _animationMapping.end() || it->second.empty())
            continue;
        const AnimationEntry* animEntry = Find_AnimationEntryByAnyName(_availableAnimations, it->second);
        if (animEntry != nullptr && !animEntry->playbackName.empty())
            resolvedMapping[_stateNames[i]] = animEntry->playbackName;
        else
            resolvedMapping[_stateNames[i]] = it->second;
    }
    AnimState_Parser::Save_StateAnimations_ToFile(
        savePath.wstring(),
        _enumTypeName,
        prefabName,
        _stateNames,
        _stateIndices,
        resolvedMapping);
    Refresh_AnimmapFiles();
}

void AnimState_View::Refresh_AnimmapFiles()
{
    _availableAnimmapFiles.clear();
    _selectedAnimmapFileIndex = -1;

    if (_sourceModelPath.empty())
        return;

    fs::path rootPath = GAME->Get_AssetRoot();
    fs::path modelDir = rootPath / fs::path(_sourceModelPath).parent_path();

    if (!fs::exists(modelDir))
        return;

    for (const auto& entry : fs::directory_iterator(modelDir))
    {
        if (entry.is_regular_file())
        {
            string filename = entry.path().filename().string();
            if (filename.find(".animmap.json") != string::npos)
            {
                size_t extPos = filename.find(".animmap.json");
                _availableAnimmapFiles.push_back(filename.substr(0, extPos));
            }
        }
    }
}

void AnimState_View::Draw_MappingSaveLoadPanel()
{
	ImGui::Text("Save Mapping");
	ImGui::Separator();

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputText("##MappingSaveFile", _mappingSaveLoadBuffer, sizeof(_mappingSaveLoadBuffer));

	if (ImGui::Button("Save##Mapping", ImVec2(-1.f, 0.f)))
	{
		Save();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	ImGui::Text("Load Mapping");
	ImGui::Separator();

	ImGui::SetNextItemWidth(-1.f);
	const char* previewValue = "Select...";
	if (_selectedAnimmapFileIndex >= 0 && _selectedAnimmapFileIndex < (int32)_availableAnimmapFiles.size())
		previewValue = _availableAnimmapFiles[_selectedAnimmapFileIndex].c_str();

	if (ImGui::BeginCombo("##MappingLoadList", previewValue))
	{
		for (int32 i = 0; i < (int32)_availableAnimmapFiles.size(); ++i)
		{
			const bool isSelected = (_selectedAnimmapFileIndex == i);
			if (ImGui::Selectable(_availableAnimmapFiles[i].c_str(), isSelected))
			{
				_selectedAnimmapFileIndex = i;
				// 옵션: 선택 즉시 _mappingSaveLoadBuffer도 동기화
				strncpy_s(_mappingSaveLoadBuffer, _availableAnimmapFiles[i].c_str(), _TRUNCATE);
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Load##Mapping", ImVec2(-1.f, 0.f)))
	{
		if (_selectedAnimmapFileIndex >= 0 && _selectedAnimmapFileIndex < (int32)_availableAnimmapFiles.size())
		{
			Load_Mapping(_availableAnimmapFiles[_selectedAnimmapFileIndex]);
		}
	}

	ImGui::Spacing();
	if (ImGui::Button("Refresh Files", ImVec2(-1.f, 0.f)))
	{
		Refresh_AnimmapFiles();
	}
}

void AnimState_View::Load_Mapping(const string& fileName)
{
    if (fileName.empty() || _sourceModelPath.empty())
        return;
    fs::path rootPath = GAME->Get_AssetRoot();
    fs::path modelDir = rootPath / fs::path(_sourceModelPath).parent_path();
    fs::path loadPath = modelDir / (fileName + ".animmap.json");
    if (!fs::exists(loadPath))
        return;
    std::ifstream file(loadPath);
    if (!file.is_open())
        return;
    json root;
    file >> root;
    
	_animationMapping.clear();

	// 세이브에 저장된 EnumType이 있다면 자동 로드 (이름 기반 검색)
	if (root.contains("EnumType"))
	{
		string savedEnumType = root["EnumType"].get<string>();
		
		vector<EnumSearchResult> results;
		Search_EnumProperties(savedEnumType, results);
		for (const auto& result : results)
		{
			const string label = result.ownerClassName + "::" + result.propertyDisplayName;
			if (label == savedEnumType)
			{
				// 찾았다면 자동 바인딩
				Apply_EnumFromProperty(label, result.enumNames, result.enumValues);
				break;
			}
		}
	}

    if (!root.contains("Mappings"))
        return;

	// 기존에 State enum 구조가 바뀌었을 수 있으므로
	// 현재 로드된 _stateNames에 존재하는 것만 맵핑하고, 없으면 날려버림
    for (const auto& entry : root["Mappings"])
    {
        string stateName = entry["State"].get<string>();
        string animName = entry["Anim"].get<string>();
        
		auto it = std::find(_stateNames.begin(), _stateNames.end(), stateName);
        if (it != _stateNames.end())
            _animationMapping[stateName] = animName;
    }
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
    desc.farPlane = 500.f;
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

void AnimState_View::Search_EnumProperties(const string &searchStr, vector<EnumSearchResult> &outResults) const
{
	outResults.clear();

	string searchLower = searchStr;
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

	// 1. 모든 등록된 GameObject 타입의 Reflection 정보를 순회
	GAME->Visit_GameObjectTypes([&](const Engine::TypeRegistry::GameObjectTypeInfo &typeInfo)
	{
		if (typeInfo.reflectionInfo == nullptr)
			return;

		for (const auto &prop : typeInfo.reflectionInfo->properties)
		{
			if (prop.type != Engine::PropertyType::Enum)
				continue;

			if (prop.enumNames.empty())
				continue;

			// displayName 또는 name으로 검색
			string propNameLower = prop.displayName;
			std::transform(propNameLower.begin(), propNameLower.end(), propNameLower.begin(), ::tolower);

			string propMemberLower = prop.name;
			std::transform(propMemberLower.begin(), propMemberLower.end(), propMemberLower.begin(), ::tolower);

			if (propNameLower.find(searchLower) != string::npos ||
				propMemberLower.find(searchLower) != string::npos)
			{
				EnumSearchResult result;
				result.ownerClassName = typeInfo.className;
				result.propertyDisplayName = prop.displayName;
				result.enumNames = prop.enumNames;
				result.enumValues = prop.enumValues;
				outResults.push_back(std::move(result));
			}
		}
	});

	// 2. 엔진에 전역으로 등록된 Global Enum 순회
	GAME->Visit_GlobalEnums([&](const Engine::TypeRegistry::GlobalEnumInfo &info)
	{
		if (info.enumNames.empty())
			return;

		string enumNameLower = info.displayName;
		std::transform(enumNameLower.begin(), enumNameLower.end(), enumNameLower.begin(), ::tolower);

		if (enumNameLower.find(searchLower) != string::npos)
		{
			EnumSearchResult result;
			result.ownerClassName = "GlobalEnum";
			result.propertyDisplayName = info.displayName;
			result.enumNames = info.enumNames;
			result.enumValues = info.enumValues;
			outResults.push_back(std::move(result));
		}
	});
}

void AnimState_View::Apply_EnumFromProperty(
	const string &enumDisplayName,
	const vector<string> &names,
	const vector<int> &values)
{
	_enumTypeName = enumDisplayName;
	_stateNames.clear();
	_stateIndices.clear();
	_animationMapping.clear();

	for (size_t i = 0; i < names.size(); ++i)
	{
		_stateNames.push_back(names[i]);
		_stateIndices.push_back(i < values.size() ? values[i] : static_cast<int>(i));
	}

	// 매핑 파일 이름이 비어있다면, 현재 Enum 이름을 기반으로 기본값을 채워줌
	if (strlen(_mappingSaveLoadBuffer) == 0)
	{
		string defaultName = enumDisplayName;
		size_t pos = defaultName.rfind("::");
		if (pos != string::npos)
		{
			defaultName = defaultName.substr(pos + 2);
		}
		strncpy_s(_mappingSaveLoadBuffer, defaultName.c_str(), _TRUNCATE);
	}
}

bool AnimState_View::Refresh_BoundEnumBeforeSave()
{
    if (_enumTypeName.empty())
        return false;

    EnumSearchResult matchedResult{};
    bool found = false;

    GAME->Visit_GameObjectTypes([&](const Engine::TypeRegistry::GameObjectTypeInfo& typeInfo)
    {
        if (found || typeInfo.reflectionInfo == nullptr)
            return;

        for (const auto& prop : typeInfo.reflectionInfo->properties)
        {
            if (prop.type != Engine::PropertyType::Enum || prop.enumNames.empty())
                continue;

            const string label = typeInfo.className + "::" + prop.displayName;
            if (label != _enumTypeName)
                continue;

            matchedResult.ownerClassName = typeInfo.className;
            matchedResult.propertyDisplayName = prop.displayName;
            matchedResult.enumNames = prop.enumNames;
            matchedResult.enumValues = prop.enumValues;
            found = true;
            return;
        }
    });

    if (!found)
    {
        GAME->Visit_GlobalEnums([&](const Engine::TypeRegistry::GlobalEnumInfo& info)
        {
            if (found || info.enumNames.empty())
                return;

            const string label = string("GlobalEnum::") + info.displayName;
            if (label != _enumTypeName)
                return;

            matchedResult.ownerClassName = "GlobalEnum";
            matchedResult.propertyDisplayName = info.displayName;
            matchedResult.enumNames = info.enumNames;
            matchedResult.enumValues = info.enumValues;
            found = true;
        });
    }

    if (!found)
        return false;

    map<string, string> previousMapping = _animationMapping;

    _stateNames.clear();
    _stateIndices.clear();
    _animationMapping.clear();

    for (size_t i = 0; i < matchedResult.enumNames.size(); ++i)
    {
        const string& stateName = matchedResult.enumNames[i];
        _stateNames.push_back(stateName);
        _stateIndices.push_back(i < matchedResult.enumValues.size() ? matchedResult.enumValues[i] : static_cast<int32>(i));

        // enum 값이 추가/이동되어도 기존 애니 매핑은 State 이름 기준으로 유지한다.
        auto mapped = previousMapping.find(stateName);
        if (mapped != previousMapping.end())
            _animationMapping[stateName] = mapped->second;
    }

    return true;
}

Shared<AnimState_View> AnimState_View::Create()
{
    return make_shared<AnimState_View>();
}
