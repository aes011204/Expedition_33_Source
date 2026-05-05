#include "EffectEditorInstance.h"

#include "RenderTarget.h"
#include "EffectEditor_Manager.h"
#include "GameInstance.h"
#include "ImGui_Manager.h"
#include "Editor_Window.h"
#include "Editor_Context.h"
#include "CommandHistory.h"
#include "Notification_Manager.h"

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

    return S_OK;
}

void EffectEditorInstance::Release_Editor()
{
    _sharedViewportRenderTarget.reset();

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
    _useManualPreviewViewport = false;

    _runtimeState = EffectEditorRuntimeState::Edit;
    _runtimeTimeScale = 1.f;
    _hasPendingWindowFocus = false;
    _pendingWindowFocus = EditorViewportTarget::Game;
    _pendingSingleFrameStep = false;
}

void EffectEditorInstance::Update_Editor(float timeDelta)
{
    Sync_PreviewViewportMode();

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

    if (!GAME->Is_ActiveCameraType(ObjectType::FreeCamera))
        GAME->SwitchTo_NextCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    _runtimeState = EffectEditorRuntimeState::Pause;
    Request_WindowFocus(EditorViewportTarget::Scene);
}

void EffectEditorInstance::Stop()
{
    _runtimeState = EffectEditorRuntimeState::Edit;

    Activate_FreeCamera();

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
    GAME->Activate_CameraByType(ObjectType::TargetCamera);
}

void EffectEditorInstance::Activate_FreeCamera()
{
    GAME->Activate_CameraByType(ObjectType::FreeCamera);
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

ID3D11ShaderResourceView* EffectEditorInstance::Get_SharedViewportSRV() const
{
    if (nullptr == _sharedViewportRenderTarget)
        return nullptr;

    return _sharedViewportRenderTarget->Get_SRV();
}

void EffectEditorInstance::Capture_BackBuffer_ToViewport()
{
    Shared<RenderTarget> renderTarget =
        Prepare_SharedViewportRenderTarget(
            GAME->Get_BackBufferWidth(),
            GAME->Get_BackBufferHeight());

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

    _context->CopyResource(viewportTexture.Get(), backBufferTexture.Get());
}

void EffectEditorInstance::Set_ManualPreviewViewportSize(uint32 width, uint32 height)
{
    GAME->Set_ViewportSize(max(1u, width), max(1u, height));
    _useManualPreviewViewport = true;
}

void EffectEditorInstance::Enable_FitPreviewViewport()
{
    _useManualPreviewViewport = false;

    GAME->Set_ViewportSize(
        GAME->Get_BackBufferWidth(),
        GAME->Get_BackBufferHeight());
}

void EffectEditorInstance::Sync_PreviewViewportMode()
{
    if (_useManualPreviewViewport)
        return;

    GAME->Set_ViewportSize(
        GAME->Get_BackBufferWidth(),
        GAME->Get_BackBufferHeight());
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

void EffectEditorInstance::Apply_RuntimeInputPolicy()
{
    Shared<Editor_Window> sceneView = Get_Window(L"Scene");
    Shared<Editor_Window> gameView = Get_Window(L"Game");

    if (_runtimeState != EffectEditorRuntimeState::Play &&
        !GAME->Is_ActiveCameraType(ObjectType::FreeCamera))
    {
        Activate_FreeCamera();
    }

    const bool sceneInteractive =
        sceneView && sceneView->Is_Open() &&
        (sceneView->Is_Focused() || sceneView->Is_Hovered());

    const bool gameFocused =
        gameView && gameView->Is_Open() && gameView->Is_Focused();

    const bool isFreeCamera =
        GAME->Is_ActiveCameraType(ObjectType::FreeCamera);

    if (_runtimeState == EffectEditorRuntimeState::Play)
    {
        if (isFreeCamera)
        {
            const bool freeCameraControl =
                gameFocused && GAME->KeyPress(KEY_TYPE::RBUTTON);

            GAME->Set_PlayerInputBlocked(true);
            GAME->Set_FreeCameraInputEnabled(freeCameraControl);

            if (freeCameraControl)
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
    Base::Free();
}
