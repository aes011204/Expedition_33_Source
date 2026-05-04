#include "EditorInstance.h"

#include "RenderTarget.h"
#include "Editor_Manager.h"
#include "GameInstance.h"
#include "ImGui_Manager.h"
#include "Editor_Context.h"
#include "CommandHistory.h"
#include "Notification_Manager.h"

IMPLEMENT_SINGLETON(EditorInstance)

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

    // 처음에는 Edit 모드로, 1배속으로 시작
    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    _runtimeTimeScale = 1.f;

    return S_OK;
}

void EditorInstance::Release_Editor()
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
    _requestedViewportWidth = 1;
    _requestedViewportHeight = 1;
    _lastViewportRequester = EDITOR_VIEWPORT_TARGET::Game;

    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    _runtimeTimeScale = 1.f;
}

void EditorInstance::Update_Editor(float timeDelta)
{
    _imguiManager->Update(timeDelta);
    _editorManager->Update(timeDelta);
}

void EditorInstance::Render_Editor()
{
    _editorManager->Render();
    _imguiManager->Render();
}

void EditorInstance::Play()
{
    _runtimeState = EDITOR_RUNTIME_STATE::Play;
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
    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
}

void EditorInstance::Stop()
{
    _runtimeState = EDITOR_RUNTIME_STATE::Edit;

    // [수정] Stop 후 Scene View 조작은 FreeCamera 감각이어야 하므로
    // TargetCamera가 아니라 FreeCamera로 복귀시킨다.
    Activate_FreeCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
}

void EditorInstance::Resume()
{
    if (EDITOR_RUNTIME_STATE::Pause != _runtimeState)
        return;

    _runtimeState = EDITOR_RUNTIME_STATE::Play;
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

void EditorInstance::Focus_GameView()
{
    ImGui::SetWindowFocus("Game");
}

void EditorInstance::Focus_SceneView()
{
    ImGui::SetWindowFocus("Scene");
}

void EditorInstance::Apply_RuntimeInputPolicy()
{
    Shared<Editor_Window> sceneView = Get_Window(L"Scene");
    Shared<Editor_Window> gameView = Get_Window(L"Game");

    if (_runtimeState != EDITOR_RUNTIME_STATE::Play &&
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

    if (_runtimeState == EDITOR_RUNTIME_STATE::Play)
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
