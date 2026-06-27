#include "Prefab_View.h"
#include "Notification_Manager.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Component.h"
#include "ContainerObject.h"
#include "ClientInstance.h"
#include "PartObject.h"
#include "Player_BattleBase.h"
#include "Editor_Camera_Free.h"
#include "Inspector.h"
#include "Helper_String.h"
#include "Animation_View.h"
#include "BattleStartSpawner.h"
#include "ColliderCom.h"
#include "ModelCom.h"
#include "Mon_WeakPoint.h"
#include "Monster_WorldBase.h"
#include "MovementCom.h"
#include "PreviewObjectHelpers.h"
#include "RenderTarget.h"
#include "SkyBoxFlow.h"
#include "SkyCloudCard.h"
#include "Spawn_Prefab.h"
#include "Transform_Inspector.h"
#include "Weapon.h"
#include "WeaponPrefabLoader.h"
#include "Mon_Dualliste_Sword_L.h"
#include "Mon_Dualliste_Sword_R.h"
#include "Mon_Simon_Weapon.h"
#include "PartObject.h"
#include "imgui_internal.h"
#include "AnimState_View.h"
#include "BattleTurnCameraPoseUtils.h"
#include "EditorInstance.h"

namespace
{
const Vec3 kPreviewSkyCloudLocalOffset = Vec3(28.f, 34.f, 82.f);

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

void Resolve_BattlePreviewBasis(
    const Shared<Client::Monster_WorldBase>& monster,
    Vec3& outPreviewOrigin,
    Vec3& outRight,
    Vec3& outForward)
{
    outPreviewOrigin = monster && monster->Get_Transform()
        ? monster->Get_Transform()->Get_WorldPosition()
        : Vec3::Zero;
    outRight = Vec3::Right;
    outForward = Vec3::Forward;

    if (monster == nullptr)
        return;

    const int32 battleStartSpawnerId = monster->Get_BattleStartSpawnerId();
    if (battleStartSpawnerId <= 0)
        return;

    bool foundSpawner = false;
    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (foundSpawner || layerTag != TEXT("Layer_BattleStartSpawner"))
                return;

            Shared<Client::BattleStartSpawner> spawner = dynamic_pointer_cast<Client::BattleStartSpawner>(gameObject);
            if (spawner == nullptr || spawner->Get_SpawnerId() != battleStartSpawnerId)
                return;

            Shared<TransformCom> spawnerTransform = spawner->Get_Transform();
            if (spawnerTransform == nullptr)
                return;

            outRight = spawnerTransform->Get_WorldRight();
            outForward = spawnerTransform->Get_WorldForward();
            foundSpawner = true;
        });

    outRight.y = 0.f;
    if (outRight.LengthSquared() <= 0.0001f)
        outRight = Vec3::Right;
    else
        outRight.Normalize();

    outForward.y = 0.f;
    if (outForward.LengthSquared() <= 0.0001f)
        outForward = Vec3::Forward;
    else
        outForward.Normalize();
}

void Resolve_MaxWideOffsets(
    const vector<Client::BattleMonsterSpawnDesc>& spawnDescs,
    float& outBackOffset,
    float& outHeightOffset,
    float& outLookHeightOffset)
{
    auto selectDominantOffset = [](float current, float candidate)
    {
        return fabsf(candidate) > fabsf(current) ? candidate : current;
    };

    outBackOffset = 0.f;
    outHeightOffset = 0.f;
    outLookHeightOffset = 0.f;

    for (const Client::BattleMonsterSpawnDesc& spawnDesc : spawnDescs)
    {
        outBackOffset = selectDominantOffset(outBackOffset, spawnDesc.wideBackOffset);
        outHeightOffset = selectDominantOffset(outHeightOffset, spawnDesc.wideHeightOffset);
        outLookHeightOffset = selectDominantOffset(outLookHeightOffset, spawnDesc.wideLookHeightOffset);
    }
}

bool Try_EvaluateMonsterWidePreviewPose(
    const Shared<Client::Monster_WorldBase>& monster,
    const vector<Client::BattleMonsterSpawnDesc>& spawnDescs,
    Client::BattleTurnCameraEvaluatedPose& outWidePose)
{
    if (monster == nullptr || spawnDescs.empty())
        return false;

    Vec3 previewOrigin = Vec3::Zero;
    Vec3 right = Vec3::Right;
    Vec3 forward = Vec3::Forward;
    Resolve_BattlePreviewBasis(monster, previewOrigin, right, forward);

    float wideBackOffset = 0.f;
    float wideHeightOffset = 0.f;
    float wideLookHeightOffset = 0.f;
    Resolve_MaxWideOffsets(spawnDescs, wideBackOffset, wideHeightOffset, wideLookHeightOffset);

    const float yawDegrees = XMConvertToDegrees(atan2f(forward.x, forward.z));
    return Client::Evaluate_BattleTurnWidePose(
        previewOrigin,
        yawDegrees,
        XMConvertToRadians(45.f),
        wideBackOffset,
        wideHeightOffset,
        wideLookHeightOffset,
        outWidePose);
}

void Refresh_PreviewPartMatricesAfterGizmo(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return;

    // Prefab View gizmo is applied after preview capture. Re-run Update only,
    // so PartObject subclasses rebuild _combinedWorldMatrix without registering render groups.
    gameObject->Update(0.f);

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    if (container == nullptr)
        return;

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);
        Refresh_PreviewPartMatricesAfterGizmo(partObject);
    }
}

json Vec3_ToJsonArray(const Vec3& value)
{
    return json::array({ value.x, value.y, value.z });
}

bool Write_RootTransformToWeaponPrefabJson(json& rootJson, const Vec3& position, const Vec3& rotation)
{
    if (!rootJson.contains("components") || !rootJson["components"].is_array())
        return false;

    for (json& componentJson : rootJson["components"])
    {
        if (!componentJson.is_object())
            continue;

        const string prototypeTag = componentJson.value("prototype_tag", string{});
        const string className = componentJson.value("class_name", string{});
        if (prototypeTag != "Com_Transform" && className != "TransformCom")
            continue;

        componentJson["position"] = Vec3_ToJsonArray(position);
        componentJson["rotation"] = Vec3_ToJsonArray(rotation);
        return true;
    }

    return false;
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

    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);
    if (_previewRenderTarget == nullptr)
        return;

    Ensure_PreviewGridResources();
    if (_showPreviewSkyBackdrop)
        Ensure_PreviewSkyBackdrop();

    if (!_previewHasBegunPlay)
    {
        PreviewObjectHelpers::Prime_PreviewObject(_previewObject);
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

    const float dt = ImGui::GetIO().DeltaTime;
    GAME->Set_PlayerInputBlocked(true);

    if (_showPreviewSkyBackdrop)
        Update_PreviewSkyBackdrop(dt);

    if (!_previewHasBegunPlay)
    {
        PreviewObjectHelpers::Prime_PreviewObject(_previewObject);
        _previewHasBegunPlay = true;
    }

    GAME->Backup_RenderGroup();
    if (_showPreviewSkyBackdrop)
        Late_Update_PreviewSkyBackdrop(dt);

    PreviewObjectHelpers::Update_PreviewObject(_previewObject, dt);

    function<void(const Shared<GameObject>&)> forceVisibleRecursive =
        [&](const Shared<GameObject>& gameObject)
    {
        if (gameObject == nullptr)
            return;

        gameObject->Set_Visible(true); // 프리뷰는 월드 컬링 결과를 따르지 않고 항상 그리게 유지한다.

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            forceVisibleRecursive(partObject);
        }
    };

    forceVisibleRecursive(_previewObject);

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
    Draw_SelectedWeaponSourceAnchorPreview();
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

    const string imguiWindowName = title + "###PrefabWindow"; // dirty 표시가 바뀌어도 ImGui 창 ID는 고정해서 입력 포커스가 끊기지 않게 한다.

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

    if (ImGui::Begin(imguiWindowName.c_str(), &isOpen, windowFlags))
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
        ImGui::SetWindowFocus(imguiWindowName.c_str());

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

    json root = _previewObject->To_Json();
    root["prefab_name"] = _prefabName;
    root["default_layer_tag"] = String::ToString(_defaultSpawnLayerTag); // 컨텐츠 브라우저 드래그 배치 시 우선 사용할 기본 레이어다.

    std::ofstream file(_prefabPath);
    const bool saved = file.is_open();
    if (saved)
    {
        file << root.dump(4);
        file.close();
    }

    if (saved)
    {
        const uint32 savedWeaponCount = Save_PlayerBattleWeaponTransforms();
        Save_SelectedWeaponTrailAnchor();

        Shared<Editor_Window> animationWindow = EDITOR->Get_Window(L"Animation");
        Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(animationWindow);
        if (animationView != nullptr)
            animationView->Refresh_PreviewTrailAnchorAsset();

        ClearDirty();

        EDITOR->Apply_SavedPrefabToCurrentLevel(_prefabName, _prefabPath);

        if (EDITOR->Get_Notification())
            EDITOR->Get_Notification()->Add_Notification("프리펩 저장: {}", _prefabName);

        if (savedWeaponCount > 0 && EDITOR->Get_Notification())
            EDITOR->Get_Notification()->Add_Notification("무기 프리펩 위치/회전 저장: {}개", savedWeaponCount);
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
    _defaultSpawnLayerTag = TEXT("Layer_GameObject");
    _applyDefaultWindowLayout = true;
    _openAnimationViewWithPrefab = true;

    const json prefabRoot = GAME->Load_JsonFile(prefabPath);
    if (prefabRoot.is_object() && prefabRoot.contains("default_layer_tag") && prefabRoot["default_layer_tag"].is_string())
        _defaultSpawnLayerTag = String::ToWString(prefabRoot["default_layer_tag"].get<string>());

    const string defaultSpawnLayerTag = String::ToString(_defaultSpawnLayerTag);
    strncpy_s(_defaultSpawnLayerTagBuffer, defaultSpawnLayerTag.c_str(), _TRUNCATE);

    _previewObject = GAME->Instantiate_GameObjectJson(prefabPath);
    _previewCamera = nullptr;
    _previewHasBegunPlay = false;
    _requestResetPreviewCamera = false;

    if (_previewObject)
    {
        PreviewObjectHelpers::Disable_PreviewAnimNotifyExecution(_previewObject);
        PreviewObjectHelpers::Disable_PreviewBehaviorTreeExecution(_previewObject);
        PreviewObjectHelpers::Prime_PreviewObject(_previewObject);
        _previewHasBegunPlay = true;
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
    PreviewObjectHelpers::Stop_AttachedPreviewEffects(_previewObject); // 프리뷰 전용 attached effect를 먼저 반환해 닫은 뒤 월드에 잔상이 남지 않게 한다.

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
    _defaultSpawnLayerTag = TEXT("Layer_GameObject");
    strcpy_s(_defaultSpawnLayerTagBuffer, "Layer_GameObject");

    _previewObject = nullptr;
    _previewCamera = nullptr;
    _previewRenderTarget = nullptr;
    _previewSkyBox = nullptr;
    _previewSkyCloudCards.clear();
    _previewGridBatch.reset();
    _previewGridEffect.reset();
    _previewGridInputLayout.Reset();
    _previewGridDepthState.Reset();
    _previewOverlayDepthState.Reset();

    _previewWidth = 512;
    _previewHeight = 512;
    _previewHasBegunPlay = false;
    _previewSkyBackdropBegunPlay = false;
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

    animationView->Open_FromModelComponent(modelCom, preferredAnimation, _pendingAnimationDockId, true);
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

    ImGui::SameLine();

    ImGui::Checkbox("Show SkyBox", &_showPreviewSkyBackdrop);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(190.f);
    if (ImGui::InputText("Default Spawn Layer", _defaultSpawnLayerTagBuffer, IM_ARRAYSIZE(_defaultSpawnLayerTagBuffer)))
    {
        if (_defaultSpawnLayerTagBuffer[0] == '\0')
        {
            _defaultSpawnLayerTag = TEXT("Layer_GameObject");
            strcpy_s(_defaultSpawnLayerTagBuffer, "Layer_GameObject"); // 빈 값 저장을 막고 기존 기본 레이어로 되돌린다.
        }
        else
        {
            _defaultSpawnLayerTag = String::ToWString(_defaultSpawnLayerTagBuffer); // 프리펩 메타에 저장할 authored 기본 레이어다.
        }

        MarkDirty();
    }

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

    // Scene View처럼 평소 preview canvas는 입력을 선점하지 않는다.
    // InvisibleButton이 먼저 active되면 ImGuizmo가 drag 시작을 못 잡는다.
    ImGui::Dummy(previewSize);
    _isPreviewHovered = ImGui::IsItemHovered();
    _isPreviewActive = _isPreviewHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ID3D11ShaderResourceView* previewSRV =
        _previewRenderTarget != nullptr ? _previewRenderTarget->Get_SRV() : nullptr;
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

        if (Shared<MovementCom> movement = selectedObject->Get_Component<MovementCom>())
            movement->Sync_RuntimePositionToTransform();
    }

    Refresh_PreviewPartMatricesAfterGizmo(selectedObject);
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

    if (Draw_PlayerBattleWeaponSelector(_previewObject))
        MarkDirty();

    switch (_selectionType)
    {
    case SelectionType::GameObject:
    {
        const json beforeJson = selectedObject->To_Json();

        if (Draw_MonsterBattlePrefabList(selectedObject))
            MarkDirty();

        if (Draw_MonsterBattleSettings(selectedObject))
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

bool Prefab_View::Draw_PlayerBattleWeaponSelector(const Shared<GameObject>& rootObject)
{
    Shared<Client::Player_BattleBase> player = dynamic_pointer_cast<Client::Player_BattleBase>(rootObject);
    if (player == nullptr)
        return false;

    const string characterId = player->Get_WeaponCharacterId();
    const vector<Client::GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(characterId);
    if (weapons == nullptr || weapons->empty())
        return false;

    ImGui::Spacing();
    ImGui::SeparatorText("Change Weapon Preview");

    string previewLabel = player->Get_ActiveWeaponPreviewId();
    if (previewLabel.empty())
        previewLabel = "(None)";

    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::BeginCombo("Weapon##PlayerBattleWeapon", previewLabel.c_str()))
    {
        for (const Client::GameDataTable::WeaponDesc& weapon : *weapons)
        {
            string label = weapon.weaponId;
            if (!weapon.weaponName.empty())
                label += " - " + String::ToString(weapon.weaponName);

            const bool selected = weapon.weaponId == player->Get_ActiveWeaponPreviewId();
            if (ImGui::Selectable(label.c_str(), selected))
            {
                if (FAILED(player->Apply_WeaponPreviewById(weapon.weaponId)) &&
                    EDITOR->Get_Notification())
                {
                    EDITOR->Get_Notification()->Add_Notification_With_Type(
                        NotifyType::Warning,
                        "무기 프리뷰 장착 실패: {}",
                        weapon.weaponId);
                }
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextDisabled("PB 프리펩에서 DT_Weapon 무기를 임시 장착합니다. Save 시 Part_Weapon 위치/회전을 WP_*에 저장합니다.");
    return false;
}

uint32 Prefab_View::Save_PlayerBattleWeaponTransforms() const
{
    Shared<Client::Player_BattleBase> player = dynamic_pointer_cast<Client::Player_BattleBase>(_previewObject);
    if (player == nullptr)
        return 0;

    uint32 savedCount = 0;
    for (const auto& [partTag, partObject] : player->Get_PartObjects())
    {
        if (partObject == nullptr || !partTag.starts_with(TEXT("Part_Weapon")))
            continue;

        Shared<Client::Weapon> weapon = dynamic_pointer_cast<Client::Weapon>(partObject);
        if (weapon == nullptr || weapon->Get_Transform() == nullptr)
            continue;

        const string weaponPrefabName = weapon->Get_SourcePrefabName();
        if (weaponPrefabName.empty())
            continue;

        const fs::path weaponPrefabPath = Client::WeaponPrefabLoader::Resolve_WeaponPrefabPath(weaponPrefabName);
        json weaponRoot = GAME->Load_JsonFile(weaponPrefabPath);
        if (!weaponRoot.is_object())
            continue;

        if (!Write_RootTransformToWeaponPrefabJson(
            weaponRoot,
            weapon->Get_Transform()->Get_Position(),
            weapon->Get_Transform()->Get_RotationEuler()))
        {
            continue;
        }

        std::ofstream weaponFile(weaponPrefabPath);
        if (!weaponFile.is_open())
            continue;

        weaponFile << weaponRoot.dump(4);
        weaponFile.close();
        ++savedCount;
    }

    return savedCount;
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

    Draw_SelectedWeaponSourceAnchorPanel(selectedObject, modelCom);
}

void Prefab_View::Draw_SelectedWeaponSourceAnchorPanel(const Shared<GameObject>& weaponObject, const Shared<ModelCom>& modelCom)
{
    if (weaponObject == nullptr)
        return;

    ImGui::Spacing();
    ImGui::SeparatorText("Source Anchors");

    vector<Client::WeaponSourceHistoryAnchorDesc>* anchorsPtr = nullptr;
    if (Shared<Client::Weapon> weapon = dynamic_pointer_cast<Client::Weapon>(weaponObject))
        anchorsPtr = &weapon->Get_SourceHistoryAnchors();
    else if (Shared<Client::Mon_Simon_Weapon> simonWeapon = dynamic_pointer_cast<Client::Mon_Simon_Weapon>(weaponObject))
        anchorsPtr = &simonWeapon->Get_SourceHistoryAnchors();
    else if (Shared<Client::Mon_Dualliste_Sword_L> swordL = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_L>(weaponObject))
        anchorsPtr = &swordL->Get_SourceHistoryAnchors();
    else if (Shared<Client::Mon_Dualliste_Sword_R> swordR = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_R>(weaponObject))
        anchorsPtr = &swordR->Get_SourceHistoryAnchors();

    if (anchorsPtr == nullptr)
        return;

    vector<Client::WeaponSourceHistoryAnchorDesc>& anchors = *anchorsPtr;
    if (ImGui::Button("Add Source Anchor"))
    {
        Client::WeaponSourceHistoryAnchorDesc anchor{};
        anchor.name = anchors.empty() ? "Source" : "Source" + std::to_string(anchors.size() + 1);
        if (modelCom != nullptr && modelCom->Get_NumBones() > 0)
            anchor.boneName = modelCom->Get_BoneName(0);
        anchors.push_back(std::move(anchor));
        MarkDirty();
    }

    for (int32 index = 0; index < static_cast<int32>(anchors.size()); ++index)
    {
        Client::WeaponSourceHistoryAnchorDesc& anchor = anchors[index];
        ImGui::PushID(index);

        ImGui::Separator();

        char nameBuffer[128]{};
        strncpy_s(nameBuffer, anchor.name.c_str(), _TRUNCATE);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            anchor.name = nameBuffer;
            MarkDirty();
        }

        char boneBuffer[128]{};
        strncpy_s(boneBuffer, anchor.boneName.c_str(), _TRUNCATE);
        if (ImGui::InputText("Bone", boneBuffer, sizeof(boneBuffer)))
        {
            anchor.boneName = boneBuffer;
            MarkDirty();
        }

        Vec3 localPosition = anchor.localPosition;
        if (ImGui::DragFloat3("Local Position", &localPosition.x, 0.01f))
        {
            anchor.localPosition = localPosition;
            MarkDirty();
        }

        if (ImGui::Button("Delete"))
        {
            anchors.erase(anchors.begin() + index);
            MarkDirty();
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }
}

bool Prefab_View::Draw_MonsterBattleSettings(const Shared<GameObject>& selectedObject)
{
    Shared<Client::Monster_WorldBase> monster = dynamic_pointer_cast<Client::Monster_WorldBase>(selectedObject);
    if (monster == nullptr)
        return false;

    struct FSpawnerEntry
    {
        int32 spawnerId = 0;
        string label{};
    };

    vector<FSpawnerEntry> entries{};
    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (layerTag != TEXT("Layer_BattleStartSpawner"))
                return; // 현재 월드에 배치된 시작 스포너만 selector 후보로 쓴다.

            Shared<Client::BattleStartSpawner> spawner = dynamic_pointer_cast<Client::BattleStartSpawner>(gameObject);
            if (spawner == nullptr)
                return;

            const int32 spawnerId = spawner->Get_SpawnerId();
            if (spawnerId <= 0)
                return;

            const auto duplicate = find_if(entries.begin(), entries.end(),
                [&](const FSpawnerEntry& entry)
                {
                    return entry.spawnerId == spawnerId;
                });
            if (duplicate != entries.end())
                return; // 같은 id가 여러 개 배치돼 있어도 authored 값은 id 하나만 저장한다.

            FSpawnerEntry entry{};
            entry.spawnerId = spawnerId;
            entry.label = "SpawnerId " + to_string(spawnerId);
            entries.push_back(std::move(entry));
        });

    sort(entries.begin(), entries.end(),
        [](const FSpawnerEntry& a, const FSpawnerEntry& b)
        {
            return a.spawnerId < b.spawnerId;
        });

    if (!ImGui::CollapsingHeader("Monster Battle Settings", ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;
    const int32 currentSpawnerId = monster->Get_BattleStartSpawnerId();

    ImGui::PushID("MonsterBattleSettings");

    if (ImGui::Button("Reload MW From Disk"))
    {
        const string sourcePrefabName = monster->Get_SourcePrefabName();
        if (!sourcePrefabName.empty())
        {
            const fs::path prefabPath = Client::Spawn_Prefab::Resolve_PrefabPath(sourcePrefabName);
            if (SUCCEEDED(GAME->Load_GameObjectJson(prefabPath, monster)))
            {
                EDITOR->Apply_SavedPrefabToCurrentLevel(sourcePrefabName, prefabPath);
                Rebuild_MonsterBattlePrefabEntries();
                MarkDirty();
            }
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Re-read battle_monster_spawns and battle start settings from the MW prefab json.");

    constexpr float LabelWidth = 110.f;
    int32 battleFieldId = static_cast<int32>(monster->Get_BattleFieldId());
    ImGui::Text("Battle Field Id");
    ImGui::SameLine(LabelWidth);
    ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
    if (ImGui::DragInt("##BattleFieldId", &battleFieldId, 1.f, 1, 999))
    {
        monster->Set_BattleFieldId(static_cast<uint32>(battleFieldId)); // 프리팹 저장값: 조우 시 진입할 배틀 필드 ID
        changed = true;
    }

    ImGui::Text("Start Position");
    ImGui::SameLine(LabelWidth);

    string previewLabel = currentSpawnerId > 0
        ? "SpawnerId " + to_string(currentSpawnerId)
        : "(none)";

    ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
    if (ImGui::BeginCombo("##BattleStartSpawnerId", previewLabel.c_str()))
    {
        if (ImGui::Selectable("(none)", currentSpawnerId == 0))
        {
            if (currentSpawnerId != 0)
            {
                monster->Set_BattleStartSpawnerId(0); // 미지정이면 런타임은 기존 기본 위치로 fallback 한다.
                changed = true;
            }
        }

        bool hasCurrentEntry = currentSpawnerId == 0;
        for (const FSpawnerEntry& entry : entries)
        {
            const bool isSelected = currentSpawnerId == entry.spawnerId;
            if (isSelected)
                hasCurrentEntry = true;

            if (ImGui::Selectable(entry.label.c_str(), isSelected))
            {
                if (currentSpawnerId != entry.spawnerId)
                {
                    monster->Set_BattleStartSpawnerId(entry.spawnerId); // 프리팹에는 월드 좌표가 아니라 선택한 spawner id만 저장한다.
                    changed = true;
                }
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        if (!hasCurrentEntry && currentSpawnerId > 0)
        {
            ImGui::Separator();
            ImGui::TextDisabled("Missing: SpawnerId %d", currentSpawnerId);
        }

        ImGui::EndCombo();
    }

    Vec3 startOffset = monster->Get_BattleStartOffset();
    ImGui::Text("Enemy Offset");
    ImGui::SameLine(LabelWidth);
    ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
    if (ImGui::DragFloat3("##BattleStartOffset", &startOffset.x, 0.05f))
    {
        monster->Set_BattleStartOffset(startOffset); // MW 프리팹 저장값: 플레이어 기준 몬스터 시작 위치 보정
        changed = true;
    }

    ImGui::PopID();
    ImGui::Spacing();

    return changed;
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

        float wideBackOffset = spawnDescs[i].wideBackOffset;
        ImGui::Text("Wide Back");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat("##BattleMonsterWideBackOffset", &wideBackOffset, 0.05f))
        {
            spawnDescs[i].wideBackOffset = wideBackOffset;
            changed = true;
        }

        float wideHeightOffset = spawnDescs[i].wideHeightOffset;
        ImGui::Text("Wide Height");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat("##BattleMonsterWideHeightOffset", &wideHeightOffset, 0.05f))
        {
            spawnDescs[i].wideHeightOffset = wideHeightOffset;
            changed = true;
        }

        float wideLookHeightOffset = spawnDescs[i].wideLookHeightOffset;
        ImGui::Text("Wide Look Height");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat("##BattleMonsterWideLookHeightOffset", &wideLookHeightOffset, 0.05f))
        {
            spawnDescs[i].wideLookHeightOffset = wideLookHeightOffset;
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

    ImGui::SameLine();
    const bool canPreviewWideCamera = _previewCamera != nullptr && !spawnDescs.empty();
    ImGui::BeginDisabled(!canPreviewWideCamera);
    if (ImGui::Button("Preview WIDE Camera"))
    {
        Client::BattleTurnCameraEvaluatedPose widePose{};
        if (Try_EvaluateMonsterWidePreviewPose(monster, spawnDescs, widePose))
        {
            Shared<TransformCom> cameraTransform = _previewCamera->Get_Transform();
            if (cameraTransform != nullptr)
            {
                cameraTransform->Set_WorldPosition(widePose.cameraPos);
                cameraTransform->LookAt(widePose.lookTarget);
            }
        }
    }
    ImGui::EndDisabled();

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
    desc.farPlane = 500.f;
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

void Prefab_View::Ensure_PreviewSkyBackdrop()
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

void Prefab_View::Update_PreviewSkyBackdrop(float timeDelta)
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

void Prefab_View::Late_Update_PreviewSkyBackdrop(float timeDelta)
{
    if (_previewSkyBox != nullptr)
        _previewSkyBox->Late_Update(timeDelta);

    for (const Shared<Client::SkyCloudCard>& skyCloudCard : _previewSkyCloudCards)
    {
        if (skyCloudCard != nullptr)
            skyCloudCard->Late_Update(timeDelta);
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
#if defined(_DEBUG) || defined(_EDITOR)
    if (gameObject == nullptr)
        return;

    Matrix colliderWorldMatrix = Matrix::Identity;
    bool hasColliderWorldMatrix = false;

    if (const Shared<Client::Mon_WeakPoint> weakPoint = dynamic_pointer_cast<Client::Mon_WeakPoint>(gameObject))
    {
        colliderWorldMatrix = weakPoint->Get_CollisionWorldMatrix();
        hasColliderWorldMatrix = true;
    }
    else if (const Shared<PartObject> partObject = dynamic_pointer_cast<PartObject>(gameObject))
    {
        colliderWorldMatrix = partObject->Get_CombinedWorldMatrix();
        hasColliderWorldMatrix = true;
    }
    else if (const Shared<TransformCom> transformCom = gameObject->Get_Transform())
    {
        colliderWorldMatrix = transformCom->Get_WorldMatrix();
        hasColliderWorldMatrix = true;
    }

    gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
    {
        Shared<ColliderCom> colliderCom = dynamic_pointer_cast<ColliderCom>(component);
        if (colliderCom == nullptr)
            return;

        if (hasColliderWorldMatrix)
            colliderCom->Update_Collider(colliderWorldMatrix);

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

    if (dynamic_pointer_cast<Client::Weapon>(selectedObject) == nullptr &&
        dynamic_pointer_cast<Client::Mon_Dualliste_Sword_L>(selectedObject) == nullptr &&
        dynamic_pointer_cast<Client::Mon_Dualliste_Sword_R>(selectedObject) == nullptr &&
        dynamic_pointer_cast<Client::Mon_Simon_Weapon>(selectedObject) == nullptr)
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

void Prefab_View::Draw_SelectedWeaponSourceAnchorPreview() const
{
    Shared<GameObject> selectedObject = Find_GameObjectByKey(_previewObject, _selectedObjectKey);
    const vector<Client::WeaponSourceHistoryAnchorDesc>* anchors = nullptr;
    if (Shared<Client::Weapon> weapon = dynamic_pointer_cast<Client::Weapon>(selectedObject))
        anchors = &weapon->Get_SourceHistoryAnchors();
    else if (Shared<Client::Mon_Simon_Weapon> simonWeapon = dynamic_pointer_cast<Client::Mon_Simon_Weapon>(selectedObject))
        anchors = &simonWeapon->Get_SourceHistoryAnchors();
    else if (Shared<Client::Mon_Dualliste_Sword_L> swordL = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_L>(selectedObject))
        anchors = &swordL->Get_SourceHistoryAnchors();
    else if (Shared<Client::Mon_Dualliste_Sword_R> swordR = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_R>(selectedObject))
        anchors = &swordR->Get_SourceHistoryAnchors();

    if (selectedObject == nullptr || anchors == nullptr || anchors->empty())
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

    constexpr float markerSize = 0.1f;
    _previewGridBatch->Begin();

    for (const Client::WeaponSourceHistoryAnchorDesc& anchor : *anchors)
    {
        Vec3 sourceWorld{};
        if (!Try_ResolveSourceAnchorWorldPosition(selectedObject, anchor, sourceWorld))
            continue;

        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(sourceWorld + Vec3(-markerSize, 0.f, 0.f), DirectX::Colors::Lime),
            DirectX::VertexPositionColor(sourceWorld + Vec3(markerSize, 0.f, 0.f), DirectX::Colors::Lime));
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(sourceWorld + Vec3(0.f, -markerSize, 0.f), DirectX::Colors::Lime),
            DirectX::VertexPositionColor(sourceWorld + Vec3(0.f, markerSize, 0.f), DirectX::Colors::Lime));
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(sourceWorld + Vec3(0.f, 0.f, -markerSize), DirectX::Colors::Lime),
            DirectX::VertexPositionColor(sourceWorld + Vec3(0.f, 0.f, markerSize), DirectX::Colors::Lime));
    }

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

    Vec3 previewOrigin = monster->Get_Transform()->Get_WorldPosition();
    Vec3 right = Vec3::Right;
    Vec3 forward = Vec3::Forward;

    const int32 battleStartSpawnerId = monster->Get_BattleStartSpawnerId();
    if (battleStartSpawnerId > 0)
    {
        bool foundSpawner = false;

        GAME->Visit_LevelObjects(
            GAME->Current_LevelIndex(),
            [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
            {
                if (foundSpawner)
                    return;

                if (layerTag != TEXT("Layer_BattleStartSpawner"))
                    return;

                Shared<Client::BattleStartSpawner> spawner = dynamic_pointer_cast<Client::BattleStartSpawner>(gameObject);
                if (spawner == nullptr)
                    return;

                if (spawner->Get_SpawnerId() != battleStartSpawnerId)
                    return;

                Shared<TransformCom> spawnerTransform = spawner->Get_Transform();
                if (spawnerTransform == nullptr)
                    return;

                previewOrigin = spawnerTransform->Get_WorldPosition();
                right = spawnerTransform->Get_WorldRight();
                forward = spawnerTransform->Get_WorldForward();
                foundSpawner = true;
            });
    }

    right.y = 0.f;
    if (right.LengthSquared() <= 0.0001f)
        right = Vec3::Right;
    else
        right.Normalize();

    forward.y = 0.f;
    if (forward.LengthSquared() <= 0.0001f)
        forward = Vec3::Forward;
    else
        forward.Normalize();

    const Vec3 battleStartOffset = monster->Get_BattleStartOffset();
    const Vec3 enemyBasePos =
        previewOrigin +
        forward * 6.f +
        right * battleStartOffset.x +
        Vec3::Up * battleStartOffset.y +
        forward * battleStartOffset.z;

    constexpr float markerHalfSize = 0.3f;
    constexpr float markerHeight = 0.2f;

    const XMVECTOR markerColors[3] =
    {
        XMVectorSet(1.f, 0.35f, 0.35f, 1.f),
        XMVectorSet(1.f, 0.82f, 0.25f, 1.f),
        XMVectorSet(0.3f, 0.85f, 1.f, 1.f),
    };

    Client::BattleTurnCameraEvaluatedPose widePose{};
    const bool hasWidePose = Try_EvaluateMonsterWidePreviewPose(monster, spawnDescs, widePose);

    constexpr float cameraMarkerHalfSize = 0.25f;
    constexpr float lookTargetMarkerHalfSize = 0.18f;

    _previewGridBatch->Begin();

    for (size_t i = 0; i < spawnDescs.size(); ++i)
    {
        const Vec3 localOffset = spawnDescs[i].offset;
        const Vec3 markerPos =
            enemyBasePos +
            right * localOffset.x +
            Vec3::Up * localOffset.y +
            forward * localOffset.z;
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

    if (hasWidePose)
    {
        const XMVECTOR cameraColor = XMVectorSet(0.2f, 0.95f, 0.45f, 1.f);
        const XMVECTOR lookColor = XMVectorSet(0.95f, 0.95f, 0.95f, 1.f);

        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(widePose.cameraPos, cameraColor),
            DirectX::VertexPositionColor(widePose.lookTarget, lookColor));

        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(widePose.cameraPos + right * -cameraMarkerHalfSize, cameraColor),
            DirectX::VertexPositionColor(widePose.cameraPos + right * cameraMarkerHalfSize, cameraColor));
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(widePose.cameraPos + Vec3::Up * -cameraMarkerHalfSize, cameraColor),
            DirectX::VertexPositionColor(widePose.cameraPos + Vec3::Up * cameraMarkerHalfSize, cameraColor));
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(widePose.lookTarget + right * -lookTargetMarkerHalfSize, lookColor),
            DirectX::VertexPositionColor(widePose.lookTarget + right * lookTargetMarkerHalfSize, lookColor));
        _previewGridBatch->DrawLine(
            DirectX::VertexPositionColor(widePose.lookTarget + Vec3::Up * -lookTargetMarkerHalfSize, lookColor),
            DirectX::VertexPositionColor(widePose.lookTarget + Vec3::Up * lookTargetMarkerHalfSize, lookColor));
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

    if (Shared<Client::Weapon> weaponObject = dynamic_pointer_cast<Client::Weapon>(selectedObject))
        outModel = weaponObject->Get_Model();
    else if (Shared<Client::Mon_Dualliste_Sword_L> swordL = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_L>(selectedObject))
        outModel = swordL->Get_Model();
    else if (Shared<Client::Mon_Dualliste_Sword_R> swordR = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_R>(selectedObject))
        outModel = swordR->Get_Model();
    else if (Shared<Client::Mon_Simon_Weapon> simonWeapon = dynamic_pointer_cast<Client::Mon_Simon_Weapon>(selectedObject))
        outModel = simonWeapon->Get_Model();
    else
        return false;

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
    if (anchor.boneName.empty())
        return false;

    Shared<ModelCom> modelCom = nullptr;
    Matrix combinedWorldMatrix = Matrix::Identity;

    if (Shared<Client::Weapon> weaponObject = dynamic_pointer_cast<Client::Weapon>(selectedObject))
    {
        modelCom = weaponObject->Get_Model();
        combinedWorldMatrix = weaponObject->Get_CombinedWorldMatrix();
    }
    else if (Shared<Client::Mon_Dualliste_Sword_L> swordL = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_L>(selectedObject))
    {
        modelCom = swordL->Get_Model();
        combinedWorldMatrix = swordL->Get_CombinedWorldMatrix();
    }
    else if (Shared<Client::Mon_Dualliste_Sword_R> swordR = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_R>(selectedObject))
    {
        modelCom = swordR->Get_Model();
        combinedWorldMatrix = swordR->Get_CombinedWorldMatrix();
    }
    else if (Shared<Client::Mon_Simon_Weapon> simonWeapon = dynamic_pointer_cast<Client::Mon_Simon_Weapon>(selectedObject))
    {
        modelCom = simonWeapon->Get_Model();
        combinedWorldMatrix = simonWeapon->Get_CombinedWorldMatrix();
    }
    else
    {
        return false;
    }

    if (modelCom == nullptr)
        return false;

    const int32 boneIndex = modelCom->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = modelCom->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorWorldMatrix = *boneMatrix * combinedWorldMatrix;
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

bool Prefab_View::Try_ResolveSourceAnchorWorldPosition(
    const Shared<GameObject>& weaponObject,
    const Client::WeaponSourceHistoryAnchorDesc& anchor,
    Vec3& outWorldPosition) const
{
    if (weaponObject == nullptr || anchor.boneName.empty())
        return false;

    Shared<ModelCom> modelCom = nullptr;
    Matrix combinedWorldMatrix = Matrix::Identity;

    if (Shared<Client::Weapon> weapon = dynamic_pointer_cast<Client::Weapon>(weaponObject))
    {
        modelCom = weapon->Get_Model();
        combinedWorldMatrix = weapon->Get_CombinedWorldMatrix();
    }
    else if (Shared<Client::Mon_Simon_Weapon> simonWeapon = dynamic_pointer_cast<Client::Mon_Simon_Weapon>(weaponObject))
    {
        modelCom = simonWeapon->Get_Model();
        combinedWorldMatrix = simonWeapon->Get_CombinedWorldMatrix();
    }
    else if (Shared<Client::Mon_Dualliste_Sword_L> swordL = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_L>(weaponObject))
    {
        modelCom = swordL->Get_Model();
        combinedWorldMatrix = swordL->Get_CombinedWorldMatrix();
    }
    else if (Shared<Client::Mon_Dualliste_Sword_R> swordR = dynamic_pointer_cast<Client::Mon_Dualliste_Sword_R>(weaponObject))
    {
        modelCom = swordR->Get_Model();
        combinedWorldMatrix = swordR->Get_CombinedWorldMatrix();
    }
    else
    {
        return false;
    }

    if (modelCom == nullptr)
        return false;

    const int32 boneIndex = modelCom->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = modelCom->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorWorldMatrix = *boneMatrix * combinedWorldMatrix;
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

Shared<Prefab_View> Prefab_View::Create()
{
    return make_shared<Prefab_View>();
}
