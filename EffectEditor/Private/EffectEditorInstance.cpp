#include "EffectEditorInstance.h"

#include "CommandHistory.h"
#include "Editor_Context.h"
#include "Editor_Window.h"
#include "EffectEditorCamera.h"
#include "EffectEditor_Manager.h"
#include "GameInstance.h"
#include "ImGui_Manager.h"
#include "Notification_Manager.h"
#include "RenderTarget.h"

NS_BEGIN(EffectEditor)

IMPLEMENT_SINGLETON(EffectEditorInstance)

EffectEditorInstance::EffectEditorInstance()
{
}

HRESULT EffectEditorInstance::Initialize_Editor(const EffectEditorDesc& editorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    _editorDesc = editorDesc;
    _device = device;
    _context = context;

    if (nullptr == _editorDesc.hWnd || nullptr == _device || nullptr == _context)
        return E_FAIL;

    _imguiManager = ImGui_Manager::Create(editorDesc.hWnd, _device, _context);
    CHECK_NULL(_imguiManager, E_FAIL);

    _editorManager = EffectEditor_Manager::Create();
    CHECK_NULL(_editorManager, E_FAIL);

    _editorContext = Editor_Context::Create();
    CHECK_NULL(_editorContext, E_FAIL);

    _commandHistory = CommandHistory::Create();
    CHECK_NULL(_commandHistory, E_FAIL);

    _notificationManager = Notification_Manager::Create();
    CHECK_NULL(_notificationManager, E_FAIL);

    _runtimeState = EffectEditorRuntimeState::Edit;
    _runtimeTimeScale = 1.f;
    _previewViewportMode = PreviewViewportMode::Manual;
    Queue_PreviewViewportSize(defaultPreviewViewportWidth, defaultPreviewViewportHeight);

    return S_OK;
}

void EffectEditorInstance::Release_Editor()
{
    _sharedViewportRenderTarget.reset();
    _previewCamera.reset();

    if (_imguiManager)
    {
        _imguiManager->Free();
        _imguiManager.reset();
    }

    _notificationManager.reset();
    _commandHistory.reset();
    _editorContext.reset();
    _editorManager.reset();

    _context.Reset();
    _device.Reset();

    _editorDesc = {};
    _previewViewportMode = PreviewViewportMode::Manual;
    _hasPendingPreviewViewportSize = false;
    _pendingPreviewViewportWidth = defaultPreviewViewportWidth;
    _pendingPreviewViewportHeight = defaultPreviewViewportHeight;
    _appliedPreviewViewportWidth = 0;
    _appliedPreviewViewportHeight = 0;

    _runtimeState = EffectEditorRuntimeState::Edit;
    _runtimeTimeScale = 1.f;
    _hasPendingWindowFocus = false;
    _pendingWindowFocus = EditorViewportTarget::Game;
    _pendingSingleFrameStep = false;
    _isSceneViewportInputEnabled = false;
    _isSceneViewportInputBlocked = false;
    _isGameViewportInputEnabled = false;
    _isPreviewCameraControlEnabled = false;
    _isPreviewCameraDragging = false;
}

void EffectEditorInstance::Update_Editor(float timeDelta)
{
    Sync_PreviewViewportMode();
    Apply_PendingPreviewViewportSize();

    _imguiManager->Update(timeDelta);
    _editorManager->Update(timeDelta);

    if (_notificationManager)
        _notificationManager->Update(timeDelta);
}

void EffectEditorInstance::Render_Editor()
{
    _editorManager->Render();

    if (_notificationManager)
        _notificationManager->Render();

    _imguiManager->Render();
}

void EffectEditorInstance::Play()
{
    _runtimeState = EffectEditorRuntimeState::Play;
    Activate_TargetCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EditorViewportTarget::Game);
}

void EffectEditorInstance::Pause()
{
    if (EffectEditorRuntimeState::Play != _runtimeState)
        return;

    if (!Is_PreviewCameraActive())
        Activate_PreviewCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    _runtimeState = EffectEditorRuntimeState::Pause;
    Request_WindowFocus(EditorViewportTarget::Scene);
}

void EffectEditorInstance::Stop()
{
    _runtimeState = EffectEditorRuntimeState::Edit;

    Activate_PreviewCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EditorViewportTarget::Scene);
}

void EffectEditorInstance::Resume()
{
    if (EffectEditorRuntimeState::Pause != _runtimeState)
        return;

    _runtimeState = EffectEditorRuntimeState::Play;
    Activate_TargetCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EditorViewportTarget::Game);
}

bool EffectEditorInstance::IsPlaying() const
{
    return EffectEditorRuntimeState::Play == _runtimeState;
}

bool EffectEditorInstance::IsPaused() const
{
    return EffectEditorRuntimeState::Pause == _runtimeState;
}

void EffectEditorInstance::Set_RuntimeTimeScale(float timeScale)
{
    _runtimeTimeScale = clamp(timeScale, 0.01f, 8.f);
}

void EffectEditorInstance::Reset_RuntimeTimeScale()
{
    _runtimeTimeScale = 1.f;
}

float EffectEditorInstance::Get_RuntimeTimeScale() const
{
    return _runtimeTimeScale;
}

void EffectEditorInstance::Request_WindowFocus(EditorViewportTarget target)
{
    _pendingWindowFocus = target;
    _hasPendingWindowFocus = true;
}

bool EffectEditorInstance::Consume_WindowFocusRequest(EditorViewportTarget target)
{
    if (!_hasPendingWindowFocus)
        return false;

    if (_pendingWindowFocus != target)
        return false;

    _hasPendingWindowFocus = false;
    return true;
}

Shared<Editor_Window> EffectEditorInstance::Get_Window(const wstring& windowName) const
{
    if (nullptr == _editorManager)
        return nullptr;

    return _editorManager->Find_Window(windowName);
}

HWND EffectEditorInstance::Get_WindowHandle() const
{
    return _editorDesc.hWnd;
}

void EffectEditorInstance::Execute_Command(const Shared<ICommand>& command)
{
    if (_commandHistory && command)
        _commandHistory->Execute(command);
}

void EffectEditorInstance::Undo()
{
    if (_commandHistory)
        _commandHistory->Undo();
}

void EffectEditorInstance::Redo()
{
    if (_commandHistory)
        _commandHistory->Redo();
}

void EffectEditorInstance::Clear_CommandHistory()
{
    if (_commandHistory)
        _commandHistory->Clear();
}

void EffectEditorInstance::Activate_TargetCamera()
{
    //GAME->Activate_CameraByType(ObjectType::TargetCamera);

    // preview level에는 TargetCamera가 없을 수 있으므로 preview camera로 안전하게 대체한다.
    if (!GAME->Activate_CameraByType(ObjectType::TargetCamera))
        Activate_PreviewCamera();
}

void EffectEditorInstance::Activate_PreviewCamera()
{
    if (_previewCamera)
        GAME->Change_ActiveCamera(_previewCamera);
}

void EffectEditorInstance::Register_PreviewCamera(const Shared<EffectEditorCamera>& previewCamera)
{
    _previewCamera = previewCamera;
}

bool EffectEditorInstance::Is_PreviewCameraActive() const
{
    return _previewCamera && GAME->Get_ActiveCamera() == _previewCamera;
}

void EffectEditorInstance::Set_SceneViewportInputState(bool canInteract, bool blocked)
{
    _isSceneViewportInputEnabled = canInteract;
    _isSceneViewportInputBlocked = blocked;
}

void EffectEditorInstance::Set_GameViewportInputState(bool canInteract)
{
    _isGameViewportInputEnabled = canInteract;
}

Shared<RenderTarget> EffectEditorInstance::Prepare_SharedViewportRenderTarget(uint32 width, uint32 height)
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
            Vec4(0.f, 0.f, 0.f, 1.f)
        );
    }
    else
    {
        const HRESULT hr = _sharedViewportRenderTarget->Resize(
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4(0.f, 0.f, 0.f, 1.f)
        );
        CHECK_FAILED(hr, nullptr);
    }

    return _sharedViewportRenderTarget;
}

ID3D11ShaderResourceView* EffectEditorInstance::Get_SharedViewportSRV() const
{
    if (nullptr == _sharedViewportRenderTarget)
        return nullptr;

    return _sharedViewportRenderTarget->Get_SRV();
}

void EffectEditorInstance::Capture_BackBuffer_ToViewport()
{
    const uint32 viewportWidth = min(
        max(1u, GAME->Get_ViewportWidth()),
        max(1u, GAME->Get_BackBufferWidth())
    );
    const uint32 viewportHeight = min(
        max(1u, GAME->Get_ViewportHeight()),
        max(1u, GAME->Get_BackBufferHeight())
    );

    const Shared<RenderTarget> renderTarget =
        Prepare_SharedViewportRenderTarget(
            viewportWidth,
            viewportHeight
        );

    if (nullptr == renderTarget || nullptr == _context)
        return;

    const ComPtr<IDXGISwapChain> swapChain = GAME->Get_SwapChain();
    if (nullptr == swapChain)
        return;

    ComPtr<ID3D11Texture2D> backBufferTexture = nullptr;
    if (FAILED(
        swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(backBufferTexture.GetAddressOf()))
    ))
        return;

    const ComPtr<ID3D11Texture2D> viewportTexture = renderTarget->Get_Texture();
    if (nullptr == viewportTexture)
        return;

    D3D11_BOX sourceBox{};
    sourceBox.left = 0;
    sourceBox.top = 0;
    sourceBox.front = 0;
    sourceBox.right = viewportWidth;
    sourceBox.bottom = viewportHeight;
    sourceBox.back = 1;

    _context->CopySubresourceRegion(
        viewportTexture.Get(),
        0,
        0,
        0,
        0,
        backBufferTexture.Get(),
        0,
        &sourceBox
    );
}

void EffectEditorInstance::Request_PreviewViewportSize(uint32 width, uint32 height)
{
    if (_previewViewportMode != PreviewViewportMode::Auto)
        return;

    Queue_PreviewViewportSize(width, height);
}

void EffectEditorInstance::Enable_AutoPreviewViewport()
{
    _previewViewportMode = PreviewViewportMode::Auto;
}

void EffectEditorInstance::Enable_ManualPreviewViewport()
{
    _previewViewportMode = PreviewViewportMode::Manual;
}

void EffectEditorInstance::Set_ManualPreviewViewportSize(uint32 width, uint32 height)
{
    _previewViewportMode = PreviewViewportMode::Manual;
    Queue_PreviewViewportSize(width, height);
}

void EffectEditorInstance::Sync_PreviewViewportMode()
{
    if (GAME->Get_ViewportWidth() > GAME->Get_BackBufferWidth() ||
        GAME->Get_ViewportHeight() > GAME->Get_BackBufferHeight())
        Queue_PreviewViewportSize(GAME->Get_ViewportWidth(), GAME->Get_ViewportHeight());
}

float EffectEditorInstance::Get_EffectiveViewportAspect() const
{
    const uint32 viewportWidth = GAME->Get_ViewportWidth();
    const uint32 viewportHeight = max(1u, GAME->Get_ViewportHeight());
    if (viewportWidth == 0)
        return 0.f;

    return static_cast<float>(viewportWidth) /
           static_cast<float>(viewportHeight);
}

void EffectEditorInstance::Queue_PreviewViewportSize(uint32 width, uint32 height)
{
    _pendingPreviewViewportWidth = max(1u, width);
    _pendingPreviewViewportHeight = max(1u, height);
    _hasPendingPreviewViewportSize = true;
}

void EffectEditorInstance::Apply_PendingPreviewViewportSize()
{
    if (!_hasPendingPreviewViewportSize)
        return;

    _hasPendingPreviewViewportSize = false;

    const uint32 width = min(
        max(1u, _pendingPreviewViewportWidth),
        max(1u, GAME->Get_BackBufferWidth())
    );
    const uint32 height = min(
        max(1u, _pendingPreviewViewportHeight),
        max(1u, GAME->Get_BackBufferHeight())
    );

    if (_appliedPreviewViewportWidth == width &&
        _appliedPreviewViewportHeight == height)
        return;

    CHECK_FAILED(GAME->Apply_LogicalViewportSize(width, height));

    _appliedPreviewViewportWidth = GAME->Get_ViewportWidth();
    _appliedPreviewViewportHeight = GAME->Get_ViewportHeight();
}

void EffectEditorInstance::Apply_RuntimeInputPolicy()
{
    const Shared<Editor_Window> sceneView = Get_Window(L"Scene");
    const Shared<Editor_Window> gameView = Get_Window(L"Game");

    if (_runtimeState != EffectEditorRuntimeState::Play &&
        !Is_PreviewCameraActive())
        Activate_PreviewCamera();

    const bool gameFocused =
        gameView && gameView->Is_Open() && gameView->Is_Focused();

    const bool previewCameraActive = Is_PreviewCameraActive();
    const bool keepPreviewCameraControl = _isPreviewCameraDragging;

    if (_runtimeState == EffectEditorRuntimeState::Play)
    {
        if (previewCameraActive)
        {
            GAME->Set_PlayerInputBlocked(true);
            GAME->Set_FreeCameraInputEnabled(false);
            _isPreviewCameraControlEnabled =
                _isGameViewportInputEnabled ||
                (_isSceneViewportInputEnabled && !_isSceneViewportInputBlocked) ||
                keepPreviewCameraControl;

            if (_isPreviewCameraControlEnabled && _isPreviewCameraDragging)
                GAME->Lock_Mouse();
            else
            {
                GAME->Unlock_Mouse();
                _isPreviewCameraDragging = false;
            }
        }
        else
        {
            GAME->Set_PlayerInputBlocked(!gameFocused);
            GAME->Set_FreeCameraInputEnabled(false);
            _isPreviewCameraControlEnabled = false;
            _isPreviewCameraDragging = false;

            if (gameFocused)
                GAME->Lock_Mouse();
            else
                GAME->Unlock_Mouse();
        }

        return;
    }

    GAME->Set_PlayerInputBlocked(true);
    GAME->Set_FreeCameraInputEnabled(false);
    _isPreviewCameraControlEnabled =
        sceneView && sceneView->Is_Open() &&
        ((_isSceneViewportInputEnabled &&
          !_isSceneViewportInputBlocked) ||
         keepPreviewCameraControl);

    if (_isPreviewCameraControlEnabled && _isPreviewCameraDragging)
        GAME->Lock_Mouse();
    else
    {
        GAME->Unlock_Mouse();
        _isPreviewCameraDragging = false;
    }
}

void EffectEditorInstance::Request_FrameStep()
{
    _pendingSingleFrameStep = true;
}

bool EffectEditorInstance::Consume_FrameStep()
{
    if (!_pendingSingleFrameStep)
        return false;

    _pendingSingleFrameStep = false;
    return true;
}

void EffectEditorInstance::Free()
{
    Release_Editor();
    __super::Free();
}

NS_END
