#include "EditorInstance.h"

#include "Camera.h"
#include "RenderTarget.h"
#include "Editor_Manager.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ImGui_Manager.h"
#include "Editor_Context.h"
#include "CommandHistory.h"
#include "Inspector_Factory.h"
#include "Notification_Manager.h"
#include "Helper_String.h"
#include "Profile_View.h"
#include <fstream>

IMPLEMENT_SINGLETON(EditorInstance)

namespace
{
constexpr uint32 kDefaultPreviewViewportWidth = 1600;
constexpr uint32 kDefaultPreviewViewportHeight = 900;
}

EditorInstance::EditorInstance()
{
}

HRESULT EditorInstance::Initialize_Editor(const EDITOR_DESC& EditorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    _editorDesc = EditorDesc;
    _device = device;
    _context = context;

    if (nullptr == _editorDesc.hWnd || nullptr == _device || nullptr == _context)
        return E_FAIL;

    _imguiManager = ImGui_Manager::Create(EditorDesc.hWnd, _device, _context);
    CHECK_NULL(_imguiManager, E_FAIL);

    _editorManager = Editor_Manager::Create();
    CHECK_NULL(_editorManager, E_FAIL);

    _editorContext = Editor_Context::Create();
    CHECK_NULL(_editorContext, E_FAIL);

    _commandHistory = CommandHistory::Create();
    CHECK_NULL(_commandHistory, E_FAIL);

    _notificationManager = Notification_Manager::Create();
    CHECK_NULL(_notificationManager, E_FAIL);

    _inspectorFactory = Inspector_Factory::Create();
    CHECK_NULL(_inspectorFactory, E_FAIL);

    // 처음에는 Edit 모드로, 1배속으로 시작
    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    _runtimeTimeScale = 1.f;
    _previewViewportMode = PreviewViewportMode::Manual;
    Queue_PreviewViewportSize(kDefaultPreviewViewportWidth, kDefaultPreviewViewportHeight);

    Sync_ViewportWindowState();

    return S_OK;
}

void EditorInstance::Release_Editor()
{
    auto Release_Manager = [](auto& manager)
    {
        if (manager)
        {
            manager->Free();
            manager.reset();
        }
    };

    _sharedViewportRenderTarget.reset();

    Release_Manager(_imguiManager);
    Release_Manager(_inspectorFactory);

    _notificationManager.reset();
    _commandHistory.reset();
    _editorContext.reset();
    _editorManager.reset();

    _context.Reset();
    _device.Reset();

    _editorDesc = {};
    _requestedViewportWidth = 1;
    _requestedViewportHeight = 1;
    _lastViewportRequester = EDITOR_VIEWPORT_TARGET::Game;
    _previewViewportMode = PreviewViewportMode::Manual;
    _hasPendingPreviewViewportSize = false;
    _pendingPreviewViewportWidth = kDefaultPreviewViewportWidth;
    _pendingPreviewViewportHeight = kDefaultPreviewViewportHeight;
    _appliedPreviewViewportWidth = 0;
    _appliedPreviewViewportHeight = 0;

    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    _runtimeTimeScale = 1.f;
    _hasPendingWindowFocus = false;
    _pendingWindowFocus = EDITOR_VIEWPORT_TARGET::Game;
    _pendingSingleFrameStep = false;
}

void EditorInstance::Update_Editor(float timeDelta)
{
    Sync_PreviewViewportMode();
    Apply_PendingPreviewViewportSize();

    _imguiManager->Update(timeDelta);
    _editorManager->Update(timeDelta);
    _notificationManager->Update(timeDelta);
}

void EditorInstance::Render_Editor()
{
    {
        Profile_View::Scope scope("Editor::EditorManager");
        _editorManager->Render();
    }

    {
        Profile_View::Scope scope("Editor::Notification");
        _notificationManager->Render();
    }

    {
        Profile_View::Scope scope("Editor::ImGui");
        _imguiManager->Render();
    }
}

void EditorInstance::Play()
{
    const bool isStartingFromEdit = _runtimeState == EDITOR_RUNTIME_STATE::Edit;

    if (isStartingFromEdit)
        Capture_PlayStartSnapshot();

    _runtimeState = EDITOR_RUNTIME_STATE::Play;
    Sync_ViewportWindowState();
    Activate_TargetCamera();

    if (isStartingFromEdit)
        Capture_ActiveCameraPose(_playStartCameraPose);

    GAME->Get_DelegateHub().OnEditorPlayRuntime.Broadcast();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Game);
}

void EditorInstance::Pause()
{
    if (EDITOR_RUNTIME_STATE::Play != _runtimeState)
        return;

    Activate_FreeCamera();

    GAME->Get_DelegateHub().OnEditorPauseRuntime.Broadcast();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    _runtimeState = EDITOR_RUNTIME_STATE::Pause;
    Sync_ViewportWindowState();
    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
}

void EditorInstance::Stop()
{
    if (_runtimeState == EDITOR_RUNTIME_STATE::Play ||
        _runtimeState == EDITOR_RUNTIME_STATE::Pause)
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Broadcast();
        Restore_PlayStartSnapshot();
    }

    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    Sync_ViewportWindowState();

    Activate_FreeCamera();
    Apply_CameraPoseToCameraType(ObjectType::FreeCamera, _playStartCameraPose);

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
}

void EditorInstance::Resume()
{
    if (EDITOR_RUNTIME_STATE::Pause != _runtimeState)
        return;

    _runtimeState = EDITOR_RUNTIME_STATE::Play;
    Sync_ViewportWindowState();
    Activate_TargetCamera();

    GAME->Get_DelegateHub().OnEditorResumeRuntime.Broadcast();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Game);
}

bool EditorInstance::IsPlaying() const
{
    return EDITOR_RUNTIME_STATE::Play == _runtimeState;
}

bool EditorInstance::IsPaused() const
{
    return EDITOR_RUNTIME_STATE::Pause == _runtimeState;
}

void EditorInstance::Set_RuntimeTimeScale(float timeScale)
{
    _runtimeTimeScale = ::clamp(timeScale, 0.01f, 8.f);
}

void EditorInstance::Reset_RuntimeTimeScale()
{
    _runtimeTimeScale = 1.f;
}

float EditorInstance::Get_RuntimeTimeScale() const
{
    return _runtimeTimeScale;
}

void EditorInstance::Request_WindowFocus(EDITOR_VIEWPORT_TARGET target)
{
    _pendingWindowFocus = target;
    _hasPendingWindowFocus = true;
}

bool EditorInstance::Consume_WindowFocusRequest(EDITOR_VIEWPORT_TARGET target)
{
    if (!_hasPendingWindowFocus)
        return false;

    if (_pendingWindowFocus != target)
        return false;

    _hasPendingWindowFocus = false;

    return true;
}

Shared<GameObject> EditorInstance::Selected_Object() const
{
    return _editorContext->Get_Selection().selectedObject;
}

void EditorInstance::Select_Object(const Shared<GameObject>& gameObject)
{
    return _editorContext->Set_SelectObject(gameObject);
}

void EditorInstance::Clear_SelectedObject()
{
    return _editorContext->Clear_Selection();
}

uint32 EditorInstance::Selection_Revision() const
{
    return _editorContext->Get_SelectionRevision();
}

void EditorInstance::Copy_SelectedObject()
{
    Shared<GameObject> selectedObject = Selected_Object();
    if (selectedObject == nullptr)
        return;

    wstring layerTag;
    if (!Find_ObjectLayerTag(selectedObject, layerTag))
        return;

    _copiedObjectJson = selectedObject->To_Json().dump();
    _copiedLayerTag = layerTag;
}

Shared<GameObject> EditorInstance::Paste_CopiedObject()
{
    Shared<GameObject> gameObject = Instantiate_GameObjectFromCopiedData();
    if (gameObject == nullptr)
        return nullptr;

    if (FAILED(GAME->Add_GameObject(GAME->Current_LevelIndex(), _copiedLayerTag, gameObject)))
        return nullptr;

    Select_Object(gameObject);
    return gameObject;
}

Shared<GameObject> EditorInstance::Duplicate_SelectedObject()
{
    Copy_SelectedObject();
    return Paste_CopiedObject();
}

bool EditorInstance::Delete_SelectedObject()
{
    Shared<GameObject> selectedObject = Selected_Object();
    if (selectedObject == nullptr)
        return false;

    if (!GAME->Remove_GameObject(GAME->Current_LevelIndex(), selectedObject))
        return false;

    Clear_SelectedObject();
    return true;
}

void EditorInstance::Request_FocusObject(const Shared<GameObject>& gameObject)
{
    return _editorContext->Request_FocusObject(gameObject);
}

Shared<GameObject> EditorInstance::Consume_FocusObjectRequest()
{
    return _editorContext->Consume_FocusObjectRequest();
}

HRESULT EditorInstance::Save_CurrentLevelSnapShot(const fs::path& path) const
{
    int32 selectedIndex = -1;
    vector<EditorWorldObjectSnapshot> snapshots = Capture_CurrentLevelObjects(&selectedIndex);

    json root = json::object();
    root["level_index"] = GAME->Current_LevelIndex();
    root["selected_index"] = selectedIndex;
    root["objects"] = json::array();

    for (const EditorWorldObjectSnapshot& snapshot : snapshots)
    {
        json entry = json::object();
        entry["layer_tag"] = String::ToString(snapshot.layerTag);
        entry["object"] = snapshot.objectJson;
        root["objects"].emplace_back(entry);
    }

    if (!path.has_parent_path())
        return E_FAIL;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    ofstream file(path);
    if (!file.is_open())
        return E_FAIL;

    file << root.dump(4);
    file.close();

    return S_OK;
}

HRESULT EditorInstance::Load_CurrentLevelSnapShot(const fs::path& path)
{
    json root = GAME->Load_JsonFile(path);
    if (root.is_null() || !root.is_object())
        return E_FAIL;

    if (!root.contains("objects") || !root["objects"].is_array())
        return E_FAIL;

    vector<EditorWorldObjectSnapshot> snapshots;
    for (const json& entry : root["objects"])
    {
        if (!entry.is_object())
            continue;

        if (!entry.contains("layer_tag") || !entry.contains("object"))
            continue;

        EditorWorldObjectSnapshot snapshot{};
        snapshot.layerTag = String::ToWString(entry["layer_tag"].get<string>());
        snapshot.objectJson = entry["object"];
        snapshots.emplace_back(snapshot);
    }

    const int32 selectedIndex = root.value("selected_index", -1);
    return Restore_CurrentLevelObjects(snapshots, selectedIndex);
}

void EditorInstance::Apply_SavedPrefabToCurrentLevel(const string& prefabName, const fs::path& prefabPath)
{
    if (prefabName.empty())
        return;

    json prefabRoot = GAME->Load_JsonFile(prefabPath);
    if (prefabRoot.is_null() || !prefabRoot.is_object())
        return;

    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);

            if (gameObject == nullptr)
                return;

            if (gameObject->Get_SourcePrefabName() != prefabName)
                return;

            Shared<TransformCom> transform = gameObject->Get_Transform();

            Vec3 worldPosition{};
            Vec3 worldRotation{};
            Vec3 scale{ 1.f, 1.f, 1.f };

            if (transform != nullptr)
            {
                worldPosition = transform->Get_WorldPosition();
                worldRotation = transform->Get_WorldRotationEuler();
                scale = transform->Get_Scale();
            }

            gameObject->From_Json(prefabRoot);
            gameObject->Set_SourcePrefabName(prefabName);

            if (transform != nullptr)
            {
                transform->Set_WorldPosition(worldPosition);
                transform->Set_WorldRotationEuler(worldRotation);
                transform->Set_Scale(scale);
            }
        });
}

Shared<Component_Inspector> EditorInstance::Find_Inspector(const string& key) const
{
    return _inspectorFactory->Get_Inspector(key);
}

Shared<Editor_Window> EditorInstance::Get_Window(const wstring& strWindowName) const
{
    if (nullptr == _editorManager)
        return nullptr;

    return _editorManager->Find_Window(strWindowName);
}

void EditorInstance::Execute_Command(const Shared<ICommand>& command)
{
    if (_commandHistory && command)
        _commandHistory->Execute(command);
}

void EditorInstance::Undo()
{
    return _commandHistory->Undo();
}

void EditorInstance::Redo()
{
    return _commandHistory->Redo();
}

void EditorInstance::Clear_CommandHistory()
{
    return _commandHistory->Clear();
}

void EditorInstance::Activate_TargetCamera()
{
    GAME->Activate_CameraByType(ObjectType::TargetCamera);
}

void EditorInstance::Activate_FreeCamera()
{
    CameraPoseSnapshot activeCameraPose{};
    const bool hasActiveCameraPose = Capture_ActiveCameraPose(activeCameraPose);

    GAME->Activate_CameraByType(ObjectType::FreeCamera);

    if (hasActiveCameraPose)
        Apply_CameraPoseToCameraType(ObjectType::FreeCamera, activeCameraPose);
}

bool EditorInstance::Capture_ActiveCameraPose(CameraPoseSnapshot& outPose) const
{
    Shared<Camera> camera = GAME->Get_ActiveCamera();
    if (camera == nullptr)
        return false;

    Shared<TransformCom> transform = camera->Get_Transform();
    if (transform == nullptr)
        return false;

    outPose.isValid = true;
    outPose.position = transform->Get_WorldPosition();
    outPose.rotation = transform->Get_WorldRotationEuler();
    outPose.yaw = camera->Get_Yaw();
    outPose.pitch = camera->Get_Pitch();
    return true;
}

bool EditorInstance::Apply_CameraPoseToCameraType(ObjectType objectType, const CameraPoseSnapshot& pose) const
{
    if (!pose.isValid)
        return false;

    const int cameraCount = GAME->Get_CameraCount();
    for (int cameraIndex = 0; cameraIndex < cameraCount; ++cameraIndex)
    {
        Shared<Camera> camera = GAME->Get_Camera(cameraIndex);
        if (camera == nullptr || camera->Get_ObjectType() != objectType)
            continue;

        Shared<TransformCom> transform = camera->Get_Transform();
        if (transform == nullptr)
            return false;

        transform->Set_WorldPosition(pose.position);
        transform->Set_WorldRotationEuler(pose.rotation);
        camera->Set_Yaw(pose.yaw);
        camera->Set_Pitch(pose.pitch);
        return true;
    }

    return false;
}

void EditorInstance::Request_ViewportSize(EDITOR_VIEWPORT_TARGET targetType, uint32 width, uint32 height)
{
    _lastViewportRequester = targetType;
    _requestedViewportWidth = max(1u, width);
    _requestedViewportHeight = max(1u, height);
}

Shared<RenderTarget> EditorInstance::Prepare_SharedViewportRenderTarget(uint32 width, uint32 height)
{
    if (0 == width)
        width = 1;

    if (0 == height)
        height = 1;

    if (nullptr == _sharedViewportRenderTarget)
    {
        _sharedViewportRenderTarget = RenderTarget::Create(
            _device,
            _context,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4(0.f, 0.f, 0.f, 1.f));
    }
    else if (FAILED(_sharedViewportRenderTarget->Resize(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4(0.f, 0.f, 0.f, 1.f))))
    {
        return nullptr;
    }

    return _sharedViewportRenderTarget;
}

ID3D11ShaderResourceView* EditorInstance::Get_SharedViewportSRV() const
{
    if (nullptr == _sharedViewportRenderTarget)
        return nullptr;

    return _sharedViewportRenderTarget->Get_SRV();
}

void EditorInstance::Capture_BackBuffer_ToViewport()
{
    const uint32 copyWidth = min(max(1u, GAME->Get_ViewportWidth()), max(1u, GAME->Get_BackBufferWidth()));
    const uint32 copyHeight = min(max(1u, GAME->Get_ViewportHeight()), max(1u, GAME->Get_BackBufferHeight()));

    Shared<RenderTarget> renderTarget = nullptr;
    {
        Profile_View::Scope scope("Capture::PrepareRT");
        renderTarget = Prepare_SharedViewportRenderTarget(copyWidth, copyHeight);
    }

    if (nullptr == renderTarget || nullptr == _context)
        return;

    ID3D11Texture2D* backBufferTexture = nullptr;
    {
        Profile_View::Scope scope("Capture::GetBackBuffer");
        backBufferTexture = Get_CachedBackBufferTexture();
    }

    if (nullptr == backBufferTexture)
        return;

    ComPtr<ID3D11Texture2D> viewportTexture = renderTarget->Get_Texture();
    if (nullptr == viewportTexture)
        return;

    D3D11_BOX sourceBox{};
    sourceBox.left = 0;
    sourceBox.top = 0;
    sourceBox.front = 0;
    sourceBox.right = copyWidth;
    sourceBox.bottom = copyHeight;
    sourceBox.back = 1;

    {
        Profile_View::Scope scope("Capture::CopySubresource");
        _context->CopySubresourceRegion(
            viewportTexture.Get(),
            0,
            0,
            0,
            0,
            backBufferTexture,
            0,
            &sourceBox);
    }
}

void EditorInstance::Invalidate_BackBufferCache()
{
    _cachedBackBufferTexture.Reset();
    _cachedBackBufferWidth = 0;
    _cachedBackBufferHeight = 0;
}

void EditorInstance::EndImGuiFrameIfBegun()
{
    if (_imguiManager)
        _imguiManager->EndFrameIfBegun();
}

ID3D11Texture2D* EditorInstance::Get_CachedBackBufferTexture()
{
    const uint32 width = GAME->Get_BackBufferWidth();
    const uint32 height = GAME->Get_BackBufferHeight();

    if (_cachedBackBufferTexture &&
        _cachedBackBufferWidth == width &&
        _cachedBackBufferHeight == height)
    {
        return _cachedBackBufferTexture.Get();
    }

    ComPtr<IDXGISwapChain> swapChain = GAME->Get_SwapChain();
    if (nullptr == swapChain)
        return nullptr;

    ComPtr<ID3D11Texture2D> backBufferTexture = nullptr;
    if (FAILED(swapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(backBufferTexture.GetAddressOf()))))
    {
        return nullptr;
    }

    _cachedBackBufferTexture = backBufferTexture;
    _cachedBackBufferWidth = width;
    _cachedBackBufferHeight = height;

    return _cachedBackBufferTexture.Get();
}

void EditorInstance::Capture_PlayStartSnapshot()
{
    _playStartWorldSnapshot = Capture_CurrentLevelObjects(&_playStartSelectedObjectIndex);
}

void EditorInstance::Restore_PlayStartSnapshot()
{
    Restore_CurrentLevelObjects(_playStartWorldSnapshot, _playStartSelectedObjectIndex);

    _playStartWorldSnapshot.clear();
    _playStartSelectedObjectIndex = -1;
}

bool EditorInstance::Is_SnapshotExcludedObject(const Shared<GameObject>& gameObject) const
{
    if (gameObject == nullptr)
        return true;

    return gameObject->Get_ObjectType() == ObjectType::FreeCamera ||
        gameObject->Get_ObjectType() == ObjectType::TargetCamera;
}

vector<EditorInstance::EditorWorldObjectSnapshot> EditorInstance::Capture_CurrentLevelObjects(
    int32* outSelectedIndex) const
{
    vector<EditorWorldObjectSnapshot> snapshots;
    Shared<GameObject> selectedObject = Selected_Object();

    if (outSelectedIndex != nullptr)
        *outSelectedIndex = -1;

    // 오브젝트들 긁어와서
    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (Is_SnapshotExcludedObject(gameObject))
                return;

            if (outSelectedIndex != nullptr && gameObject == selectedObject)
                *outSelectedIndex = static_cast<int32>(snapshots.size());

            // 저장
            EditorWorldObjectSnapshot snapshot{};
            snapshot.layerTag = layerTag;
            snapshot.objectJson = gameObject->To_Json();
            snapshot.originalObject = gameObject;
            snapshots.emplace_back(snapshot);
        });

    return snapshots;
}

HRESULT EditorInstance::Restore_CurrentLevelObjects(const vector<EditorWorldObjectSnapshot>& snapshots,
    int32 selectedIndex)
{
    Clear_SelectedObject();

    const uint32 levelIndex = GAME->Current_LevelIndex();

    vector<Shared<GameObject>> snapshotObjects;
    for (const EditorWorldObjectSnapshot& snapshot : snapshots)
    {
        Shared<GameObject> originalObject = snapshot.originalObject.lock();
        if (originalObject != nullptr)
            snapshotObjects.emplace_back(originalObject);
    }

    vector<Shared<GameObject>> objectsToRemove;
    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);

            if (Is_SnapshotExcludedObject(gameObject))
                return;

            const auto iter = find(snapshotObjects.begin(), snapshotObjects.end(), gameObject);
            if (iter == snapshotObjects.end())
                objectsToRemove.emplace_back(gameObject);
        });

    for (const Shared<GameObject>& gameObject : objectsToRemove)
        GAME->Remove_GameObject(levelIndex, gameObject);

    Shared<GameObject> restoredSelectedObject = nullptr;

    for (size_t i = 0; i < snapshots.size(); i++)
    {
        const EditorWorldObjectSnapshot& snapshot = snapshots[i];

        Shared<GameObject> gameObject = snapshot.originalObject.lock();

        if (gameObject != nullptr)
        {
            gameObject->From_Json(snapshot.objectJson);
        }
        else
        {
            gameObject = GAME->Instantiate_GameObjectJson(snapshot.objectJson);
        }

        if (gameObject == nullptr)
            continue;

        if (snapshot.originalObject.expired())
        {
            if (FAILED(GAME->Add_GameObject(levelIndex, snapshot.layerTag, gameObject)))
                continue;
        }

        if (static_cast<int32>(i) == selectedIndex)
            restoredSelectedObject = gameObject;
    }

    if (restoredSelectedObject != nullptr)
        Select_Object(restoredSelectedObject);

    return S_OK;
}

void EditorInstance::Request_PreviewViewportSize(uint32 width, uint32 height)
{
    (void)width;
    (void)height;
}

void EditorInstance::Enable_AutoPreviewViewport()
{
    _previewViewportMode = PreviewViewportMode::Auto;
    Queue_PreviewViewportSize(
        max(1u, _requestedViewportWidth),
        max(1u, _requestedViewportHeight));
}

void EditorInstance::Enable_ManualPreviewViewport()
{
    _previewViewportMode = PreviewViewportMode::Manual;
}

void EditorInstance::Set_ManualPreviewViewportSize(uint32 width, uint32 height)
{
    _previewViewportMode = PreviewViewportMode::Manual;
    Queue_PreviewViewportSize(width, height);
}

void EditorInstance::Sync_PreviewViewportMode()
{
    if (_previewViewportMode == PreviewViewportMode::Auto)
    {
        Queue_PreviewViewportSize(
            max(1u, _requestedViewportWidth),
            max(1u, _requestedViewportHeight));
        return;
    }

    if (_appliedPreviewViewportWidth == 0 || _appliedPreviewViewportHeight == 0)
        Queue_PreviewViewportSize(kDefaultPreviewViewportWidth, kDefaultPreviewViewportHeight);
}

float EditorInstance::Get_EffectiveViewportAspect() const
{
    const uint32 viewportWidth = GAME->Get_ViewportWidth();
    const uint32 viewportHeight = max(1u, GAME->Get_ViewportHeight());
    if (viewportWidth == 0)
        return 0.f;

    return static_cast<float>(viewportWidth) /
           static_cast<float>(viewportHeight);
}

void EditorInstance::Focus_GameView()
{
    ImGui::SetWindowFocus("Game");
}

void EditorInstance::Focus_SceneView()
{
    ImGui::SetWindowFocus("Scene");
}

bool EditorInstance::Find_ObjectLayerTag(const Shared<GameObject>& gameObject, wstring& outLayerTag) const
{
    if (gameObject == nullptr)
        return false;

    bool found = false;

    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& currentObject)
        {
            if (found || currentObject != gameObject)
                return;

            outLayerTag = layerTag;
            found = true;
        });

    return found;
}

Shared<GameObject> EditorInstance::Instantiate_GameObjectFromCopiedData() const
{
    if (_copiedObjectJson.empty())
        return nullptr;

    json root = json::parse(_copiedObjectJson, nullptr, false);
    if (root.is_discarded() || !root.is_object())
        return nullptr;

    return GAME->Instantiate_GameObjectJson(root);
}

void EditorInstance::Sync_ViewportWindowState()
{
    Shared<Editor_Window> sceneView = Get_Window(L"Scene");
    Shared<Editor_Window> gameView = Get_Window(L"Game");

    const bool showGameView = (_runtimeState == EDITOR_RUNTIME_STATE::Play);
    const bool showSceneView = !showGameView;

    if (sceneView)
        sceneView->Set_Open(showSceneView);

    if (gameView)
        gameView->Set_Open(showGameView);

}

void EditorInstance::Queue_PreviewViewportSize(uint32 width, uint32 height)
{
    _pendingPreviewViewportWidth = max(1u, width);
    _pendingPreviewViewportHeight = max(1u, height);
    _hasPendingPreviewViewportSize = true;
}

void EditorInstance::Apply_PendingPreviewViewportSize()
{
    if (!_hasPendingPreviewViewportSize)
        return;

    _hasPendingPreviewViewportSize = false;

    const uint32 width = min(
        max(1u, _pendingPreviewViewportWidth),
        max(1u, GAME->Get_BackBufferWidth()));
    const uint32 height = min(
        max(1u, _pendingPreviewViewportHeight),
        max(1u, GAME->Get_BackBufferHeight()));

    if (_appliedPreviewViewportWidth == width &&
        _appliedPreviewViewportHeight == height)
        return;

    if (FAILED(GAME->Apply_LogicalViewportSize(width, height)))
        return;

    _appliedPreviewViewportWidth = GAME->Get_ViewportWidth();
    _appliedPreviewViewportHeight = GAME->Get_ViewportHeight();
}

void EditorInstance::Apply_RuntimeInputPolicy()
{
    Shared<Editor_Window> sceneView = Get_Window(L"Scene");
    Shared<Editor_Window> gameView = Get_Window(L"Game");

    //if (_runtimeState != EDITOR_RUNTIME_STATE::Play &&
    //    !GAME->Is_ActiveCameraType(ObjectType::FreeCamera))
    //{
    //    Activate_FreeCamera();
    //}

    const bool sceneInteractive =
        sceneView && sceneView->Is_Open() &&
        (sceneView->Is_Focused() || sceneView->Is_Hovered());

    const bool gameFocused =
        gameView && gameView->Is_Open() && gameView->Is_Focused();

    const bool isFreeCamera =
        GAME->Is_ActiveCameraType(ObjectType::FreeCamera);

    const bool isTargetCamera =
    GAME->Is_ActiveCameraType(ObjectType::TargetCamera);

    if (_runtimeState == EDITOR_RUNTIME_STATE::Play)
    {
        if (GAME->Is_GameplayPaused())
        {
            GAME->Set_PlayerInputBlocked(true);
            GAME->Set_FreeCameraInputEnabled(false);
            GAME->Unlock_Mouse();
            return;
        }

        if (isFreeCamera)
        {
            const bool freeCameraControl =
                gameFocused && GAME->KeyPress(KEY_TYPE::RBUTTON);

            GAME->Set_PlayerInputBlocked(true);
            GAME->Set_FreeCameraInputEnabled(freeCameraControl);

            if (freeCameraControl||isTargetCamera)
                GAME->Lock_Mouse();
            else
                GAME->Unlock_Mouse();
        }
        else
        {
            GAME->Set_PlayerInputBlocked(!gameFocused);
            GAME->Set_FreeCameraInputEnabled(false);

            if (gameFocused)
                GAME->Lock_Mouse();
            else
                GAME->Unlock_Mouse();
        }

        return;
    }

    const bool freeCameraControl =
        sceneInteractive && GAME->KeyPress(KEY_TYPE::RBUTTON);

    GAME->Set_PlayerInputBlocked(true);
    GAME->Set_FreeCameraInputEnabled(freeCameraControl);

    if (freeCameraControl)
        GAME->Lock_Mouse();
    else
        GAME->Unlock_Mouse();
}


void EditorInstance::Request_FrameStep()
{
    _pendingSingleFrameStep = true;
}

bool EditorInstance::Consume_FrameStep()
{
    if (!_pendingSingleFrameStep)
        return false;

    _pendingSingleFrameStep = false;

    return true;
}

void EditorInstance::Free()
{
    Release_Editor();

    Base::Free();
}
