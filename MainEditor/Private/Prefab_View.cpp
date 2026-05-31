#include "Prefab_View.h"
#include "Notification_Manager.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Component.h"
#include "ContainerObject.h"
#include "PartObject.h"
#include "Editor_Camera_Free.h"
#include "Inspector.h"
#include "Helper_String.h"
#include "Animation_View.h"
#include "ColliderCom.h"
#include "ModelCom.h"
#include "Monster_WorldBase.h"
#include "RenderTarget.h"
#include "Transform_Inspector.h"
#include "Weapon.h"
#include "PartObject.h"
#include "imgui_internal.h"
#include "AnimState_View.h"

namespace
{
ImGuizmo::MODE Resolve_GizmoModeFromTransformSpace()
{
    return MainEditor::Transform_Inspector::Get_Space() == MainEditor::Transform_Inspector::ETransformSpace::World
        ? ImGuizmo::WORLD
        : ImGuizmo::LOCAL;
}

void Sync_TransformSpaceFromGizmoMode(ImGuizmo::MODE gizmoMode)
{
    MainEditor::Transform_Inspector::Set_Space(
        gizmoMode == ImGuizmo::WORLD
            ? MainEditor::Transform_Inspector::ETransformSpace::World
            : MainEditor::Transform_Inspector::ETransformSpace::Local);
}
}

Prefab_View::Prefab_View()
    : Editor_Window(L"Prefab")
{
    Set_Open(false);
}

void Prefab_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);
    Handle_GizmoShortcut();

    if (EDITOR->IsPlaying())
        return;

    if (!Is_Visible())
        return;

    if (Should_DeferToCompanionPreview())
        return;

    if (_isPreviewHovered && !_isGizmoHovered && !_isGizmoUsing && _previewCamera)
        _previewCamera->Priority_Update(timeDelta);
}

void Prefab_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (EDITOR->IsPlaying())
        return;

    if (!Is_Visible())
        return;

    if (Should_DeferToCompanionPreview())
        return;

    if (_previewObject == nullptr || _previewCamera == nullptr)
        return;

    Ensure_PreviewGridResources();

    if (!_previewHasBegunPlay)
    {
        _previewObject->Try_BeginPlay();
        _previewHasBegunPlay = true;
    }

    const Matrix savedView = *GAME->Get_Transform(D3DTS::View);
    const Matrix savedProj = *GAME->Get_Transform(D3DTS::Proj);
    const Shared<Camera> savedActiveCamera = GAME->Get_ActiveCamera();
    const uint32 savedViewportWidth = GAME->Get_ViewportWidth();
    const uint32 savedViewportHeight = GAME->Get_ViewportHeight();

    vector<LIGHT_DESC> savedLightDescs{};
    GAME->Get_LightDescs(savedLightDescs);
    const bool wasPlayerInputBlocked = GAME->Is_PlayerInputBlocked();

    EDITOR->Request_PreviewViewportSize(_previewWidth, _previewHeight);

    GAME->Bind_BackBuffer();
    if (FAILED(GAME->Clear_Buffers(Vec4(0.f, 0.f, 0.f, 1.f))))
    {
        EDITOR->Request_PreviewViewportSize(savedViewportWidth, savedViewportHeight);
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

    const float dt = ImGui::GetIO().DeltaTime;
    GAME->Set_PlayerInputBlocked(true);

    _previewObject->Priority_Update(dt);
    _previewObject->Update(dt);
    GAME->Backup_RenderGroup();
    _previewObject->Late_Update(dt);
    Lock_PreviewObjectToOrigin();

    if (_requestResetPreviewCamera)
    {
        Reset_PreviewCameraPose();
        _requestResetPreviewCamera = false;
    }

    GAME->Draw_RendererOnly();
    GAME->Restore_RenderGroup();

    if (_showPrefabCollider)
        Draw_PreviewColliders(_previewObject);

    Draw_PreviewGrid();
    Draw_MonsterBattleSpawnPreview();
    Draw_SelectedWeaponTrailAnchorPreview();
    EDITOR->Capture_BackBuffer_ToPreview();

    GAME->Set_Lights(savedLightDescs);
    GAME->Set_PlayerInputBlocked(wasPlayerInputBlocked);

    GAME->Bind_BackBuffer();

    if (savedActiveCamera != nullptr)
        GAME->Change_ActiveCamera(savedActiveCamera);

    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
    EDITOR->Request_PreviewViewportSize(savedViewportWidth, savedViewportHeight);
}

void Prefab_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    string title = windowName;
    if (!_prefabName.empty())
        title += " - " + _prefabName;

    if (Is_Dirty())
        title += " *";

    if (_openAnimationViewWithPrefab)
        Prepare_AnimationViewSiblingDock();

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (_isGizmoUsing || (_isPreviewHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)))
        windowFlags |= ImGuiWindowFlags_NoMove;

    if (_pendingDockId != 0)
        ImGui::SetNextWindowDockID(_pendingDockId, ImGuiCond_Always);

    if (_focusPrefabFrames > 0)
        ImGui::SetNextWindowFocus();

    if (_applyDefaultWindowLayout && _pendingDockId == 0)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 defaultSize = ImVec2(1500.f, 920.f);

        ImGui::SetNextWindowSize(defaultSize, ImGuiCond_Always);

        if (viewport)
        {
            ImGui::SetNextWindowPos(
                viewport->GetCenter(),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
        }
    }

    _isFocused = false;
    _isHovered = false;
    _isVisible = false;
    _isGizmoHovered = false;
    _isGizmoUsing = false;

    if (ImGui::Begin(title.c_str(), &isOpen, windowFlags))
    {
        _isVisible = true;
        _applyDefaultWindowLayout = false;
        if (_pendingDockId != 0 || _focusPrefabFrames > 0)
            ImGui::SetWindowFocus();

        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        _dockId = ImGui::GetWindowDockID();
        _pendingDockId = 0;
        if (_focusPrefabFrames > 0)
            --_focusPrefabFrames;

        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

        Draw_Header();
        ImGui::Separator();

        if (ImGui::BeginTable("PrefabViewLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Structure", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 2.2f);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 1.4f);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Draw_StructurePanel();

            ImGui::TableSetColumnIndex(1);
            Draw_PreviewPanel();

            ImGui::TableSetColumnIndex(2);
            Draw_InspectorPanel();

            ImGui::EndTable();
        }

    }

    Set_Open(isOpen);
    ImGui::End();

    if (_focusPrefabFrames > 0)
        ImGui::SetWindowFocus(title.c_str());

    if (_pendingOpenAnimationView)
        Open_PendingAnimationView();

    if (_pendingOpenAnimStateView)
        Open_PendingAnimStateView();
}

void Prefab_View::Handle_GizmoShortcut()
{
    if (!_isFocused)
        return;

    if (GAME->KeyPress(KEY_TYPE::RBUTTON))
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        _gizmoOperation = ImGuizmo::TRANSLATE;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_E, false))
    {
        _gizmoOperation = ImGuizmo::ROTATE;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        _gizmoOperation = ImGuizmo::SCALE;
        return;
    }
}

bool Prefab_View::CanSave() const
{
    return _previewObject != nullptr && !_prefabPath.empty();
}

void Prefab_View::Save()
{
    if (!CanSave())
        return;

    if (SUCCEEDED(GAME->Save_GameObjectJson(_prefabPath, _previewObject)))
    {
        Save_SelectedWeaponTrailAnchor();

        Shared<Editor_Window> animationWindow = EDITOR->Get_Window(L"Animation");
        Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(animationWindow);
        if (animationView != nullptr)
            animationView->Refresh_PreviewTrailAnchorAsset();

        ClearDirty();

        EDITOR->Apply_SavedPrefabToCurrentLevel(_prefabName, _prefabPath);

        if (EDITOR->Get_Notification())
            EDITOR->Get_Notification()->Add_Notification("프리펩 저장: {}", _prefabName);
    }
    else if (EDITOR->Get_Notification())
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(NotifyType::Warning, "프리펩 저장 실패: {}", _prefabName);
    }
}

void Prefab_View::Open_Prefab(const string& prefabName, const fs::path& prefabPath)
{
    if (_previewObject != nullptr)
        Close_Prefab();

    _prefabName = prefabName;
    _prefabPath = prefabPath;
    _sourceStaticClass.clear();
    _applyDefaultWindowLayout = true;
    _openAnimationViewWithPrefab = true;

    _previewObject = GAME->Instantiate_GameObjectJson(prefabPath);
    _previewCamera = nullptr;
    _previewHasBegunPlay = false;
    _requestResetPreviewCamera = false;

    if (_previewObject)
    {
        Disable_PreviewAnimNotifyExecution(_previewObject);
        _sourceStaticClass = _previewObject->Get_ReflectionInfo().className;

        if (_sourceStaticClass.empty())
            _sourceStaticClass = String::ToString(_previewObject->Get_Name());

        _previewCamera = Editor_Camera_Free::Create(GAME->Get_Device(), GAME->Get_Context());

        if (_previewCamera)
        {
            Editor_Camera_Free::EDITOR_CAMERA_DESC desc = Build_PreviewCameraDesc(_previewObject);

            if (FAILED(_previewCamera->Initialize(&desc)))
                _previewCamera = nullptr;
            else
            {
                _previewCamera->Apply_EditorDesc(desc);
                Apply_PreviewCameraTransform(_previewCamera);
            }
        }

        Lock_PreviewObjectToOrigin();
        Ensure_PreviewGridResources();
        _requestResetPreviewCamera = true;
    }

    _selectionType = SelectionType::GameObject;
    _selectedObjectKey = L"Root";
    _selectedComponentKey.clear();
    _selectedWeaponTrailModelGuid.clear();
    _selectedWeaponTrailAnchor = {};
    _showPrefabCollider = false;
    Rebuild_MonsterBattlePrefabEntries();

    ClearDirty();
    Set_Open(true);
}

void Prefab_View::Close_Prefab()
{
    Shared<Editor_Window> animationWindow = EDITOR->Get_Window(L"Animation");
    Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(animationWindow);
    if (animationView != nullptr)
        animationView->Clear_LivePreviewBinding(_previewObject);

    Shared<Editor_Window> animStateWindow = EDITOR->Get_Window(L"AnimState");
    Shared<AnimState_View> animStateView = dynamic_pointer_cast<AnimState_View>(animStateWindow);
    if (animStateView != nullptr)
        animStateView->Clear_LivePreviewBinding(_previewObject);

    _prefabName.clear();
    _prefabPath.clear();
    _sourceStaticClass.clear();

    _previewObject = nullptr;
    _previewCamera = nullptr;
    _previewRenderTarget = nullptr;
    _previewGridBatch.reset();
    _previewGridEffect.reset();
    _previewGridInputLayout.Reset();
    _previewGridDepthState.Reset();
    _previewOverlayDepthState.Reset();

    _previewWidth = 512;
    _previewHeight = 512;
    _previewHasBegunPlay = false;
    _requestResetPreviewCamera = false;
    _isPreviewHovered = false;
    _isPreviewActive = false;
    _isGizmoHovered = false;
    _isGizmoUsing = false;
    _applyDefaultWindowLayout = false;
    _openAnimationViewWithPrefab = false;
    _pendingOpenAnimationView = false;
    _pendingOpenAnimStateView = false;
    _dockId = 0;
    _pendingDockId = 0;
    _pendingAnimationDockId = 0;
    _pendingAnimStateDockId = 0;
    _focusPrefabFrames = 0;

    _selectionType = SelectionType::None;
    _selectedObjectKey.clear();
    _selectedComponentKey.clear();
    _selectedWeaponTrailModelGuid.clear();
    _selectedWeaponTrailAnchor = {};
    _monsterBattlePrefabEntries.clear();
    _showPrefabCollider = false;

    ClearDirty();
    Set_Open(false);
}

bool Prefab_View::Is_PreviewPrefab(const string& prefabName) const
{
    return Is_Open() && _prefabName == prefabName;
}

Shared<ModelCom> Prefab_View::Find_FirstModelComponent() const
{
    if (Shared<ContainerObject> rootContainer = dynamic_pointer_cast<ContainerObject>(_previewObject))
    {
        if (PartObject* bodyPart = rootContainer->Get_PartObject(L"Part_Body"))
        {
            if (Shared<ModelCom> bodyModel = bodyPart->Get_Component<ModelCom>())
                return bodyModel;
        }
    }

    function<Shared<ModelCom>(const Shared<GameObject>&)> findRecursive =
        [&](const Shared<GameObject>& gameObject) -> Shared<ModelCom>
    {
        if (gameObject == nullptr)
            return nullptr;

        Shared<ModelCom> foundModel = nullptr;
        gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
        {
            Shared<ModelCom> modelCom = dynamic_pointer_cast<ModelCom>(component);
            if (modelCom == nullptr)
                return;

            // 액세서리/비애니메이션 모델보다 실제 재생 가능한 모델을 우선 잡는다.
            if (modelCom->Get_NumAnimations() > 0)
            {
                foundModel = modelCom;
                return;
            }

            if (foundModel == nullptr)
                foundModel = modelCom;
        });

        if (foundModel != nullptr)
            return foundModel;

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return nullptr;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);

            if (Shared<ModelCom> partModel = findRecursive(partObject))
                return partModel;
        }

        return nullptr;
    };

    return findRecursive(_previewObject);
}

void Prefab_View::Prepare_AnimationViewSiblingDock()
{
    _openAnimationViewWithPrefab = false;

    Shared<ModelCom> modelCom = Find_FirstModelComponent();
    if (modelCom == nullptr)
        return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 defaultSize = ImVec2(1500.f, 920.f);
    const ImVec2 defaultPos = viewport
        ? ImVec2(viewport->GetCenter().x - defaultSize.x * 0.5f, viewport->GetCenter().y - defaultSize.y * 0.5f)
        : ImVec2(80.f, 80.f);

    const ImGuiID dockId = ImHashStr(("PrefabAnimationDock_" + _prefabName).c_str());
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_None);
    ImGui::DockBuilderSetNodePos(dockId, defaultPos);
    ImGui::DockBuilderSetNodeSize(dockId, defaultSize);
    ImGui::DockBuilderFinish(dockId);

    _dockId = dockId;
    _pendingDockId = dockId;
    _pendingAnimationDockId = dockId;
    _pendingAnimStateDockId = dockId;
    _pendingOpenAnimationView = true;
    _pendingOpenAnimStateView = true;
    _focusPrefabFrames = 10;
    _applyDefaultWindowLayout = false;
}

void Prefab_View::Open_PendingAnimationView()
{
    _pendingOpenAnimationView = false;

    Shared<ModelCom> modelCom = Find_FirstModelComponent();
    if (modelCom == nullptr)
        return;

    Shared<Editor_Window> animationWindow = EDITOR->Get_Window(L"Animation");
    Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(animationWindow);
    if (animationView == nullptr)
        return;

    string preferredAnimation{};
    if (modelCom->Get_NumAnimations() > 0)
    {
        if (const char* animationName = modelCom->Get_AnimationName(0))
            preferredAnimation = animationName;
    }

    animationView->Open_FromModelComponent(modelCom, preferredAnimation, _pendingAnimationDockId, false);
}

void Prefab_View::Open_PendingAnimStateView()
{
    _pendingOpenAnimStateView = false;

    Shared<ModelCom> modelCom = Find_FirstModelComponent();
    if (modelCom == nullptr)
        return;

    Shared<Editor_Window> window = EDITOR->Get_Window(L"AnimState");
    Shared<AnimState_View> animStateView = dynamic_pointer_cast<AnimState_View>(window);
    if (animStateView == nullptr)
        return;

    string preferredAnimation{};
    if (modelCom->Get_NumAnimations() > 0)
    {
        if (const char* animationName = modelCom->Get_AnimationName(0))
            preferredAnimation = animationName;
    }

    animStateView->Open_FromModelComponent(
        modelCom,
        preferredAnimation,
        _pendingAnimStateDockId,
        false);
}

void Prefab_View::Draw_Header()
{
    if (_prefabName.empty())
    {
        ImGui::TextDisabled("No prefab opened.");
        return;
    }

    ImGui::Text("Prefab: %s", _prefabName.c_str());
    ImGui::SameLine();

    if (ImGui::Button("Save", ImVec2(90.f, 0.f)))
        Save();

    ImGui::SameLine();

    if (ImGui::Button("Close", ImVec2(90.f, 0.f)))
        Close_Prefab();

    ImGui::SameLine();

    ImGui::Checkbox("Show Prefab Collider", &_showPrefabCollider);

    ImGui::SameLine();

    ImGui::Checkbox("Show Grid", &_showPreviewGrid);

    if (!_sourceStaticClass.empty())
        ImGui::TextDisabled("Static Class: %s", _sourceStaticClass.c_str());

    ImGui::TextDisabled("%s", _prefabPath.string().c_str());
}

void Prefab_View::Draw_StructurePanel()
{
    ImGui::BeginChild("PrefabStructurePanel", ImVec2(0.f, 0.f), false);

    ImGui::TextUnformatted("Structure");
    ImGui::Separator();

    if (_prefabName.empty())
    {
        ImGui::TextDisabled("Open a prefab from Content Browser.");
        ImGui::EndChild();
        return;
    }

    if (_previewObject == nullptr)
    {
        ImGui::TextDisabled("Failed to load preview object.");
        ImGui::EndChild();
        return;
    }

    Draw_GameObjectTree(_previewObject, L"Root");

    ImGui::EndChild();
}


void Prefab_View::Draw_PreviewPanel()
{
    ImGui::BeginChild("PrefabPreviewPanel", ImVec2(0.f, 0.f), false);

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
    _previewCanvasMin = previewPos;
    _previewCanvasMax = ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y);

    ImGui::InvisibleButton(
        "PrefabPreviewCanvas",
        previewSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    _isPreviewHovered = ImGui::IsItemHovered();
    _isPreviewActive = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ID3D11ShaderResourceView* previewSRV = EDITOR->Get_SharedPreviewSRV();
    if (previewSRV != nullptr)
    {
        ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSRV));

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
            "Preview RenderTarget is not ready.");
    }

    if (_previewCanvasMax.x > _previewCanvasMin.x && _previewCanvasMax.y > _previewCanvasMin.y)
    {
        ImDrawList* windowDrawList = ImGui::GetWindowDrawList();
        // 프리뷰 rect 밖 입력과 섞이지 않게 gizmo 표시/입력을 preview 캔버스 안으로 제한한다.
        windowDrawList->PushClipRect(_previewCanvasMin, _previewCanvasMax, true);
        ImGuizmo::SetDrawlist(windowDrawList);
        Render_SelectedObjectGizmo();
        Render_GizmoOverlay();
        windowDrawList->PopClipRect();
    }

    ImGui::EndChild();
}

void Prefab_View::Render_SelectedObjectGizmo()
{
    _gizmoMode = Resolve_GizmoModeFromTransformSpace();

    // 우클릭 카메라 조작 중에는 gizmo가 입력을 소비하지 않게 막는다.
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
        return;

    if (_previewObject == nullptr || _previewCamera == nullptr)
        return;

    Shared<GameObject> selectedObject = Find_GameObjectByKey(_previewObject, _selectedObjectKey);
    if (selectedObject == nullptr)
        selectedObject = _previewObject;

    Shared<TransformCom> transform = selectedObject->Get_Transform();
    if (transform == nullptr)
        return;

    const float viewportWidth = _previewCanvasMax.x - _previewCanvasMin.x;
    const float viewportHeight = _previewCanvasMax.y - _previewCanvasMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    const Matrix viewMatrix = _previewCamera->Get_ViewMatrix();
    const Matrix projMatrix = XMMatrixPerspectiveFovLH(
        _previewCamera->Get_FovY(),
        viewportWidth / viewportHeight,
        _previewCamera->Get_Near(),
        _previewCamera->Get_Far());

    Shared<PartObject> selectedPartObject = dynamic_pointer_cast<PartObject>(selectedObject);
    Matrix worldMatrix = selectedPartObject ? selectedPartObject->Get_GizmoWorldMatrix() : transform->Get_WorldMatrix();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(_previewCanvasMin.x, _previewCanvasMin.y, viewportWidth, viewportHeight);

    const bool manipulated = ImGuizmo::Manipulate(
        reinterpret_cast<float*>(const_cast<Matrix*>(&viewMatrix)),
        reinterpret_cast<float*>(const_cast<Matrix*>(&projMatrix)),
        _gizmoOperation,
        _gizmoMode,
        reinterpret_cast<float*>(&worldMatrix));

    _isGizmoHovered = ImGuizmo::IsOver();
    _isGizmoUsing = ImGuizmo::IsUsing();

    if (!manipulated)
        return;

    if (selectedPartObject != nullptr)
    {
        if (!selectedPartObject->Apply_GizmoWorldMatrix(worldMatrix))
            return;
    }
    else
    {
        Vec3 scale = Vec3::One;
        Quat rotation = Quat::Identity;
        Vec3 translation = Vec3::Zero;

        if (!worldMatrix.Decompose(scale, rotation, translation))
            return;

        transform->Set_WorldPosition(translation);
        transform->Set_RotationQuaternion(rotation);
        transform->Set_Scale(scale);
    }

    MarkDirty();
}

void Prefab_View::Render_GizmoOverlay()
{
    _gizmoMode = Resolve_GizmoModeFromTransformSpace();

    const float viewportWidth = _previewCanvasMax.x - _previewCanvasMin.x;
    const float viewportHeight = _previewCanvasMax.y - _previewCanvasMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImVec2 panelSize = ImVec2(180.f, 34.f);
    const ImVec2 panelMin = ImVec2(_previewCanvasMax.x - panelSize.x - 16.f, _previewCanvasMin.y + 16.f);
    const ImVec2 panelMax = ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y);

    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(36, 38, 42, 220), 6.f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(92, 96, 104, 255), 6.f);

    ImGui::SetCursorScreenPos(ImVec2(panelMin.x + 8.f, panelMin.y + 5.f));
    ImGui::PushID("PrefabGizmoOverlay");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 3.f));

    const ImVec4 activeButtonColor = ImVec4(0.79f, 0.56f, 0.18f, 1.f);
    const ImVec4 activeHoveredColor = ImVec4(0.88f, 0.64f, 0.23f, 1.f);
    const ImVec4 activeTextColor = ImVec4(0.09f, 0.09f, 0.09f, 1.f);

    const auto drawModeButton = [&](const char* label, const ImVec2& size, bool active)
        {
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
                ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
            }

            const bool clicked = ImGui::Button(label, size);

            if (active)
                ImGui::PopStyleColor(3);

            return clicked;
        };

    if (drawModeButton("W", ImVec2(24.f, 22.f), _gizmoOperation == ImGuizmo::TRANSLATE))
        _gizmoOperation = ImGuizmo::TRANSLATE;

    ImGui::SameLine();

    if (drawModeButton("E", ImVec2(24.f, 22.f), _gizmoOperation == ImGuizmo::ROTATE))
        _gizmoOperation = ImGuizmo::ROTATE;

    ImGui::SameLine();

    if (drawModeButton("R", ImVec2(24.f, 22.f), _gizmoOperation == ImGuizmo::SCALE))
        _gizmoOperation = ImGuizmo::SCALE;

    ImGui::SameLine(0.f, 8.f);

    if (drawModeButton("Local", ImVec2(56.f, 22.f), _gizmoMode == ImGuizmo::LOCAL))
    {
        _gizmoMode = ImGuizmo::LOCAL;
        Sync_TransformSpaceFromGizmoMode(_gizmoMode);
    }

    ImGui::SameLine();

    if (drawModeButton("World", ImVec2(56.f, 22.f), _gizmoMode == ImGuizmo::WORLD))
    {
        _gizmoMode = ImGuizmo::WORLD;
        Sync_TransformSpaceFromGizmoMode(_gizmoMode);
    }

    ImGui::PopStyleVar();
    ImGui::PopID();
}

void Prefab_View::Draw_InspectorPanel()
{
    ImGui::BeginChild("PrefabInspectorPanel", ImVec2(0.f, 0.f), false);

    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();

    if (_prefabName.empty())
    {
        ImGui::TextDisabled("No prefab opened.");
        ImGui::EndChild();
        return;
    }

    if (_previewObject == nullptr)
    {
        ImGui::TextDisabled("Preview object is not loaded.");
        ImGui::EndChild();
        return;
    }

    Shared<GameObject> selectedObject = Find_GameObjectByKey(_previewObject, _selectedObjectKey);
    if (selectedObject == nullptr)
        selectedObject = _previewObject;

    switch (_selectionType)
    {
    case SelectionType::GameObject:
    {
        const json beforeJson = selectedObject->To_Json();

        if (Draw_MonsterBattlePrefabList(selectedObject))
            MarkDirty();

        const Engine::ClassReflectionInfo& info = selectedObject->Get_ReflectionInfo();

        if (info.properties.empty())
        {
            ImGui::TextDisabled("No GameObject reflection properties.");
        }
        else
        {
            Inspector::Draw_ReflectionSection(info.className, selectedObject.get(), info);
        }

        if (beforeJson != selectedObject->To_Json())
            MarkDirty();

        Draw_SelectedWeaponTrailAnchorPanel(selectedObject);
        break;
    }

    case SelectionType::Component:
    {
        Shared<Component> selectedComponent = nullptr;

        selectedObject->Visit_Components([&](const wstring& tag, const Shared<Component>& component)
        {
            const wstring componentKey = _selectedObjectKey + L"::" + tag;
            if (componentKey == _selectedComponentKey)
                selectedComponent = component;
        });

        if (selectedComponent == nullptr)
        {
            ImGui::TextDisabled("Selected component not found.");
            break;
        }

        const json beforeJson = selectedComponent->To_Json();

        if (Inspector::Draw_ComponentOverride(selectedComponent))
        {
            if (beforeJson != selectedComponent->To_Json())
                MarkDirty();
            break;
        }

        const Engine::ClassReflectionInfo& info = selectedComponent->Get_ReflectionInfo();
        if (info.properties.empty())
        {
            ImGui::TextDisabled("No reflection properties.");
            break;
        }

        Inspector::Draw_ReflectionSection(info.className, selectedComponent.get(), info);

        if (beforeJson != selectedComponent->To_Json())
            MarkDirty();

        Draw_SelectedWeaponTrailAnchorPanel(selectedObject);
        break;
    }

    default:
        ImGui::TextDisabled("Nothing selected.");
        break;
    }

    ImGui::EndChild();
}

void Prefab_View::Draw_SelectedWeaponTrailAnchorPanel(const Shared<GameObject>& selectedObject)
{
    Shared<ModelCom> modelCom = nullptr;
    string modelGuid{};
    if (!Try_GetSelectedWeaponModel(selectedObject, modelCom, modelGuid))
        return;

    if (_selectedWeaponTrailModelGuid != modelGuid)
        Load_SelectedWeaponTrailAnchor(modelGuid, modelCom);

    ImGui::Spacing();
    ImGui::SeparatorText("Trail Anchors");

    ImGui::Text("Model GUID");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", modelGuid.c_str());

    ImGui::Text("Base Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _selectedWeaponTrailAnchor.base.boneName.empty() ? "(None)" : _selectedWeaponTrailAnchor.base.boneName.c_str());

    ImGui::Text("Tip Bone");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _selectedWeaponTrailAnchor.tip.boneName.empty() ? "(None)" : _selectedWeaponTrailAnchor.tip.boneName.c_str());

    Vec3 baseLocal = _selectedWeaponTrailAnchor.base.localPosition;
    if (ImGui::DragFloat3("Base Local", &baseLocal.x, 0.01f))
    {
        _selectedWeaponTrailAnchor.base.localPosition = baseLocal;
        MarkDirty();
    }

    Vec3 tipLocal = _selectedWeaponTrailAnchor.tip.localPosition;
    if (ImGui::DragFloat3("Tip Local", &tipLocal.x, 0.01f))
    {
        _selectedWeaponTrailAnchor.tip.localPosition = tipLocal;
        MarkDirty();
    }
}

bool Prefab_View::Draw_MonsterBattlePrefabList(const Shared<GameObject>& selectedObject)
{
    Shared<Client::Monster_WorldBase> monster = dynamic_pointer_cast<Client::Monster_WorldBase>(selectedObject);
    if (monster == nullptr)
        return false;

    if (_monsterBattlePrefabEntries.empty())
        Rebuild_MonsterBattlePrefabEntries();

    bool changed = false;
    vector<Client::BattleMonsterSpawnDesc>& spawnDescs = monster->Get_BattleMonsterSpawnDescs_ForEdit();

    // 프리팹에 저장되는 전투 소환 목록이다.
    if (!ImGui::CollapsingHeader("Battle Monster Prefabs", ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    ImGui::PushID("BattleMonsterPrefabs");

    constexpr float BattlePrefabLabelWidth = 110.f;

    int32 removeIndex = -1;
    for (int32 i = 0; i < static_cast<int32>(spawnDescs.size()); ++i)
    {
        ImGui::PushID(i);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Slot %d", i);
        ImGui::SameLine(BattlePrefabLabelWidth);

        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x - 74.f));
        const char* previewLabel =
            spawnDescs[i].prefabName.empty() ? "(select prefab)" : spawnDescs[i].prefabName.c_str();
        if (ImGui::BeginCombo("##BattleMonsterPrefabName", previewLabel))
        {
            bool hasCurrentPrefab = spawnDescs[i].prefabName.empty();
            for (const auto& entry : _monsterBattlePrefabEntries)
            {
                const bool isSelected = (spawnDescs[i].prefabName == entry.prefabName);
                if (isSelected)
                    hasCurrentPrefab = true;

                const string label = entry.prefabName + "##" + entry.className;
                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    if (spawnDescs[i].prefabName != entry.prefabName)
                    {
                        spawnDescs[i].prefabName = entry.prefabName; // 저장값은 실제 prefab 이름으로 유지한다.
                        changed = true;
                    }
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            if (!hasCurrentPrefab && !spawnDescs[i].prefabName.empty())
            {
                ImGui::Separator();
                ImGui::TextDisabled("Missing: %s", spawnDescs[i].prefabName.c_str());
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove", ImVec2(68.f, 0.f)))
            removeIndex = i;

        Vec3 offset = spawnDescs[i].offset;
        ImGui::Text("Offset");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat3("##BattleMonsterOffset", &offset.x, 0.05f))
        {
            spawnDescs[i].offset = offset;
            changed = true;
        }

        Vec3 rotation = spawnDescs[i].rotation;
        ImGui::Text("Rotation");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat3("##BattleMonsterRotation", &rotation.x, 0.25f))
        {
            spawnDescs[i].rotation = rotation;
            changed = true;
        }

        ImGui::PopID();
    }

    if (removeIndex >= 0 && removeIndex < static_cast<int32>(spawnDescs.size()))
    {
        spawnDescs.erase(spawnDescs.begin() + removeIndex);
        changed = true;
    }

    if (ImGui::Button("Add", ImVec2(70.f, 0.f)))
    {
        if (!_monsterBattlePrefabEntries.empty())
            spawnDescs.push_back({ _monsterBattlePrefabEntries.front().prefabName });
        else
            spawnDescs.emplace_back();
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(70.f, 0.f)))
    {
        spawnDescs.clear();
        changed = true;
    }

    ImGui::PopID();
    ImGui::Spacing();

    return changed;
}

void Prefab_View::Rebuild_MonsterBattlePrefabEntries()
{
    _monsterBattlePrefabEntries.clear();

    fs::path prefabsDir = _prefabPath.empty()
        ? fs::path(L"../../../Client/Bin/Resources/Data/json/Prefabs")
        : _prefabPath.parent_path();

    if (!fs::exists(prefabsDir) || !fs::is_directory(prefabsDir))
        return;

    static const string kPrefabSuffix = ".prefab.json";

    for (const fs::directory_entry& entry : fs::directory_iterator(prefabsDir))
    {
        if (!entry.is_regular_file())
            continue;

        const string fileName = String::ToString(entry.path().filename().wstring());
        if (fileName.size() <= kPrefabSuffix.size() ||
            fileName.compare(fileName.size() - kPrefabSuffix.size(), kPrefabSuffix.size(), kPrefabSuffix) != 0)
        {
            continue;
        }

        const json root = GAME->Load_JsonFile(entry.path());
        if (!root.is_object())
            continue;

        const string staticClass = root.value("static_class", string{});
        if (staticClass.empty())
            continue;

        const TypeRegistry::GameObjectTypeInfo* typeInfo = GAME->Find_GameObjectType(staticClass);
        if (typeInfo == nullptr || typeInfo->reflectionInfo == nullptr)
            continue;

        if (typeInfo->reflectionInfo->category != "Monster")
            continue; // 컨텐츠 브라우저의 Monster category 기준으로만 보여준다.

        const string prefabName = fileName.substr(0, fileName.size() - kPrefabSuffix.size());
        _monsterBattlePrefabEntries.push_back({ prefabName, staticClass });
    }

    sort(_monsterBattlePrefabEntries.begin(), _monsterBattlePrefabEntries.end(),
        [](const FMonsterBattlePrefabEntry& a, const FMonsterBattlePrefabEntry& b)
        {
            return a.prefabName < b.prefabName;
        });
}

void Prefab_View::Draw_GameObjectTree(const Shared<GameObject>& gameObject, const wstring& objectKey)
{
    CHECK_NULL(gameObject);

    string label = "GameObject";
    const ClassReflectionInfo& info = gameObject->Get_ReflectionInfo();

    if (!info.displayName.empty())
        label = info.displayName;
    else if (!info.className.empty())
        label = info.className;
    else
        label = String::ToString(gameObject->Get_Name());

    const bool isPartObject = dynamic_pointer_cast<PartObject>(gameObject) != nullptr;
    if (isPartObject)
        label = "Part - " + label;

    const bool isSelected =
        (_selectionType == SelectionType::GameObject && _selectedObjectKey == objectKey);

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
    if (isSelected)
        nodeFlags |= ImGuiTreeNodeFlags_Selected;

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    const bool hasChildren = container && !container->Get_PartObjects().empty();
    if (!hasChildren)
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    if (isPartObject)
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(244, 206, 120, 255));

    const bool isOpen = ImGui::TreeNodeEx(String::ToString(objectKey).c_str(), nodeFlags, "%s", label.c_str());

    if (isPartObject)
        ImGui::PopStyleColor();

    if (ImGui::IsItemClicked())
    {
        _selectionType = SelectionType::GameObject;
        _selectedObjectKey = objectKey;
        _selectedComponentKey.clear();
    }

    if (isOpen)
    {
        Draw_ComponentEntries(gameObject, objectKey);

        if (container)
        {
            if (!container->Get_PartObjects().empty())
                ImGui::SeparatorText("Parts");

            for (const auto& [partTag, partObject] : container->Get_PartObjects())
            {
                if (partObject == nullptr)
                    continue;

                const wstring childKey = objectKey + L"/" + partTag;
                Draw_GameObjectTree(partObject, childKey);
            }
        }

        ImGui::TreePop();
    }
}

void Prefab_View::Draw_ComponentEntries(const Shared<GameObject>& gameObject, const wstring& objectKey)
{
    CHECK_NULL(gameObject);

    bool hasComponents = false;
    gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
    {
        if (component != nullptr)
            hasComponents = true;
    });

    if (hasComponents)
        ImGui::SeparatorText("Components");

    gameObject->Visit_Components([&](const wstring& tag, const Shared<Component>& component)
    {
        CHECK_NULL(component);

        const wstring componentKey = objectKey + L"::" + tag;

        const bool isSelected =
            (_selectionType == SelectionType::Component && _selectedComponentKey == componentKey);

        string label = String::ToString(tag);
        const ClassReflectionInfo& info = component->Get_ReflectionInfo();

        if (!info.displayName.empty())
            label = info.displayName + " (" + label + ")";
        else if (!info.className.empty())
            label = info.className + " (" + label + ")";

        ImGui::Indent();

        if (ImGui::Selectable(label.c_str(), isSelected))
        {
            _selectionType = SelectionType::Component;
            _selectedObjectKey = objectKey;
            _selectedComponentKey = componentKey;
        }

        ImGui::Unindent();
    });
}

Shared<GameObject> Prefab_View::Find_GameObjectByKey(const Shared<GameObject>& rootObject, const wstring& objectKey) const
{
    if (rootObject == nullptr)
        return nullptr;

    if (objectKey.empty() || objectKey == L"Root")
        return rootObject;

    function<Shared<GameObject>(const Shared<GameObject>&, const wstring&)> findRecursive =
        [&](const Shared<GameObject>& currentObject, const wstring& currentKey) -> Shared<GameObject>
    {
        if (currentObject == nullptr)
            return nullptr;

        if (currentKey == objectKey)
            return currentObject;

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(currentObject);
        if (container == nullptr)
            return nullptr;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            if (partObject == nullptr)
                continue;

            const wstring childKey = currentKey + L"/" + partTag;
            Shared<GameObject> foundObject = findRecursive(partObject, childKey);
            if (foundObject != nullptr)
                return foundObject;
        }

        return nullptr;
    };

    return findRecursive(rootObject, L"Root");
}

Editor_Camera_Free::EDITOR_CAMERA_DESC Prefab_View::Build_PreviewCameraDesc(const Shared<GameObject>& gameObject)
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

void Prefab_View::Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera)
{
    if (camera == nullptr)
        return;

    Shared<TransformCom> cameraTransform = camera->Get_Transform();
    if (cameraTransform == nullptr)
        return;

    cameraTransform->Set_WorldPosition(Vec3(-1.75f, 2.35f, 6.5f));
    cameraTransform->LookAt(Vec3(-0.35f, -3.65f, -14.8f));
}

void Prefab_View::Disable_PreviewAnimNotifyExecution(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return;

    gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
    {
        Shared<ModelCom> modelCom = dynamic_pointer_cast<ModelCom>(component);
        if (modelCom != nullptr)
            modelCom->Set_AnimNotifyExecutionEnabled(false);
    });

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    if (container == nullptr)
        return;

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);
        Disable_PreviewAnimNotifyExecution(partObject);
    }
}

bool Prefab_View::Should_DeferToCompanionPreview()
{
    return !EDITOR->Is_ActivePreviewOwner(PreviewOwner::Prefab);
}

void Prefab_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
    width = max(1u, width);
    height = max(1u, height);

    // 처음 열었을 때
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

void Prefab_View::Ensure_PreviewGridResources()
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

void Prefab_View::Reset_PreviewCameraPose()
{
    if (_previewObject == nullptr || _previewCamera == nullptr)
        return;

    _previewCamera->Apply_EditorDesc(Build_PreviewCameraDesc(_previewObject));
    Apply_PreviewCameraTransform(_previewCamera);
}

void Prefab_View::Lock_PreviewObjectToOrigin()
{
    if (_previewObject == nullptr)
        return;

    Shared<TransformCom> previewTransform = _previewObject->Get_Transform();
    if (previewTransform == nullptr)
        return;

    previewTransform->Set_WorldPosition(Vec3::Zero);
}

void Prefab_View::Draw_PreviewColliders(const Shared<GameObject>& gameObject) const
{
#ifdef _DEBUG
    if (gameObject == nullptr)
        return;

    Shared<TransformCom> transformCom = gameObject->Get_Transform();

    gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
    {
        Shared<ColliderCom> colliderCom = dynamic_pointer_cast<ColliderCom>(component);
        if (colliderCom == nullptr)
            return;

        if (transformCom != nullptr)
            colliderCom->Update_Collider(transformCom->Get_WorldMatrix());

        colliderCom->Render_Debug();
    });

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    if (container == nullptr)
        return;

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);
        Draw_PreviewColliders(partObject);
    }
#else
    UNREFERENCED_PARAMETER(gameObject);
#endif
}

void Prefab_View::Draw_PreviewGrid() const
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

    // GS particle/effect pass가 남긴 상태를 정리해야 한다.
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

void Prefab_View::Draw_SelectedWeaponTrailAnchorPreview() const
{
    Shared<GameObject> selectedObject = Find_GameObjectByKey(_previewObject, _selectedObjectKey);
    if (selectedObject == nullptr)
        return;

    if (dynamic_pointer_cast<Client::Weapon>(selectedObject) == nullptr)
        return;

    if (_selectedWeaponTrailModelGuid.empty())
        return;

    Vec3 baseWorld{};
    Vec3 tipWorld{};
    if (!Try_ResolveTrailAnchorWorldPosition(selectedObject, _selectedWeaponTrailAnchor.base, baseWorld))
        return;
    if (!Try_ResolveTrailAnchorWorldPosition(selectedObject, _selectedWeaponTrailAnchor.tip, tipWorld))
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
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

void Prefab_View::Draw_MonsterBattleSpawnPreview() const
{
    Shared<GameObject> selectedObject = Find_GameObjectByKey(_previewObject, _selectedObjectKey);
    if (selectedObject == nullptr)
        selectedObject = _previewObject;

    Shared<Client::Monster_WorldBase> monster = dynamic_pointer_cast<Client::Monster_WorldBase>(selectedObject);
    if (monster == nullptr)
        return;

    const vector<Client::BattleMonsterSpawnDesc>& spawnDescs = monster->Get_BattleMonsterSpawnDescs();
    if (spawnDescs.empty())
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewOverlayDepthState)
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

    const Vec3 previewOrigin = Vec3(22.f, 2.5f, -10.24f);
    const Vec3 enemyBasePos[3] =
    {
        { -3.f, 0.f, 10.f },
        {  0.f, 0.f, 10.f },
        {  3.f, 0.f, 10.f },
    };

    constexpr float markerHalfSize = 0.3f;
    constexpr float markerHeight = 0.2f;

    const XMVECTOR markerColors[3] =
    {
        XMVectorSet(1.f, 0.35f, 0.35f, 1.f),
        XMVectorSet(1.f, 0.82f, 0.25f, 1.f),
        XMVectorSet(0.3f, 0.85f, 1.f, 1.f),
    };

    _previewGridBatch->Begin();

    for (size_t i = 0; i < spawnDescs.size() && i < 3; ++i)
    {
        const Vec3 markerPos = previewOrigin + enemyBasePos[i] + spawnDescs[i].offset;
        const XMVECTOR color = markerColors[i % 3];

        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(markerPos + Vec3(-markerHalfSize, markerHeight, 0.f), color),
            DirectX::VertexPositionColor(markerPos + Vec3(markerHalfSize, markerHeight, 0.f), color));
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(markerPos + Vec3(0.f, markerHeight, -markerHalfSize), color),
            DirectX::VertexPositionColor(markerPos + Vec3(0.f, markerHeight, markerHalfSize), color));
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(markerPos, color),
            DirectX::VertexPositionColor(markerPos + Vec3(0.f, 0.9f, 0.f), color));
    }

    _previewGridBatch->End();
}

void Prefab_View::Load_SelectedWeaponTrailAnchor(const string& modelGuid, const Shared<ModelCom>& modelCom)
{
    _selectedWeaponTrailModelGuid = modelGuid;
    _selectedWeaponTrailAnchor = {};
    _selectedWeaponTrailAnchor.modelGuid = modelGuid;

    if (modelGuid.empty())
        return;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(modelGuid);
    (void)Client::TrailAnchor_Parser::Load_FromFile(filePath.wstring(), _selectedWeaponTrailAnchor);

    if (_selectedWeaponTrailAnchor.modelGuid.empty())
        _selectedWeaponTrailAnchor.modelGuid = modelGuid;

    if (modelCom == nullptr || modelCom->Get_NumBones() == 0)
        return;

    const string& defaultBoneName = modelCom->Get_BoneName(0);
    if (_selectedWeaponTrailAnchor.base.boneName.empty())
        _selectedWeaponTrailAnchor.base.boneName = defaultBoneName;
    if (_selectedWeaponTrailAnchor.tip.boneName.empty())
        _selectedWeaponTrailAnchor.tip.boneName = defaultBoneName;
}

void Prefab_View::Save_SelectedWeaponTrailAnchor() const
{
    if (_selectedWeaponTrailModelGuid.empty())
        return;

    Client::TrailAnchorAsset asset = _selectedWeaponTrailAnchor;
    asset.modelGuid = _selectedWeaponTrailModelGuid;

    const fs::path filePath = Client::TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(_selectedWeaponTrailModelGuid);
    (void)Client::TrailAnchor_Parser::Save_ToFile(filePath.wstring(), asset);
}

bool Prefab_View::Try_GetSelectedWeaponModel(const Shared<GameObject>& selectedObject, Shared<ModelCom>& outModel, string& outModelGuid) const
{
    outModel = nullptr;
    outModelGuid.clear();

    Shared<Client::Weapon> weaponObject = dynamic_pointer_cast<Client::Weapon>(selectedObject);
    if (weaponObject == nullptr)
        return false;

    outModel = weaponObject->Get_Model();
    if (outModel == nullptr)
        return false;

    const wstring& sourceModelPath = outModel->Get_SourceModelPath();
    if (sourceModelPath.empty())
        return false;

    outModelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
    return !outModelGuid.empty();
}

bool Prefab_View::Try_ResolveTrailAnchorWorldPosition(
    const Shared<GameObject>& selectedObject,
    const Client::TrailAnchorPointDesc& anchor,
    Vec3& outWorldPosition) const
{
    Shared<Client::Weapon> weaponObject = dynamic_pointer_cast<Client::Weapon>(selectedObject);
    if (weaponObject == nullptr || anchor.boneName.empty())
        return false;

    Shared<ModelCom> modelCom = weaponObject->Get_Model();
    if (modelCom == nullptr)
        return false;

    const int32 boneIndex = modelCom->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = modelCom->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorWorldMatrix = *boneMatrix * weaponObject->Get_CombinedWorldMatrix();
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

Shared<Prefab_View> Prefab_View::Create()
{
    return make_shared<Prefab_View>();
}
