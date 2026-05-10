#include "EditorInstance.h"

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
    _editorManager->Render();
    _notificationManager->Render();
    _imguiManager->Render();
}

void EditorInstance::Play()
{
    _runtimeState = EDITOR_RUNTIME_STATE::Play;
    Sync_ViewportWindowState();
    Activate_TargetCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Game);
}

void EditorInstance::Pause()
{
    if (EDITOR_RUNTIME_STATE::Play != _runtimeState)
        return;

    if (!GAME->Is_ActiveCameraType(ObjectType::FreeCamera))
        GAME->SwitchTo_NextCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    _runtimeState = EDITOR_RUNTIME_STATE::Pause;
    Sync_ViewportWindowState();
    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
}

void EditorInstance::Stop()
{
    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    Sync_ViewportWindowState();

    //Activate_FreeCamera();

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
    GAME->Activate_CameraByType(ObjectType::FreeCamera);
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
    Shared<RenderTarget> renderTarget =
        Prepare_SharedViewportRenderTarget(
            min(max(1u, GAME->Get_ViewportWidth()), max(1u, GAME->Get_BackBufferWidth())),
            min(max(1u, GAME->Get_ViewportHeight()), max(1u, GAME->Get_BackBufferHeight())));

    if (nullptr == renderTarget || nullptr == _context)
        return;

    ComPtr<IDXGISwapChain> swapChain = GAME->Get_SwapChain();
    if (nullptr == swapChain)
        return;

    ComPtr<ID3D11Texture2D> backBufferTexture = nullptr;
    if (FAILED(swapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(backBufferTexture.GetAddressOf()))))
    {
        return;
    }

    ComPtr<ID3D11Texture2D> viewportTexture = renderTarget->Get_Texture();
    if (nullptr == viewportTexture)
        return;

    D3D11_BOX sourceBox{};
    sourceBox.left = 0;
    sourceBox.top = 0;
    sourceBox.front = 0;
    sourceBox.right = min(
        max(1u, GAME->Get_ViewportWidth()),
        max(1u, GAME->Get_BackBufferWidth()));
    sourceBox.bottom = min(
        max(1u, GAME->Get_ViewportHeight()),
        max(1u, GAME->Get_BackBufferHeight()));
    sourceBox.back = 1;

    _context->CopySubresourceRegion(
        viewportTexture.Get(),
        0,
        0,
        0,
        0,
        backBufferTexture.Get(),
        0,
        &sourceBox);
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
    if (root.is_discarded() || !root.is_object() || !root.contains("static_class"))
        return nullptr;

    const string staticClass = root["static_class"].get<string>();
    if (staticClass.empty())
        return nullptr;

    Shared<GameObject> gameObject = GAME->Create_GameObjectPrototype(staticClass);
    if (gameObject == nullptr)
        return nullptr;

    if (FAILED(gameObject->Initialize(nullptr)))
        return nullptr;

    gameObject->From_Json(root);
    return gameObject;
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
