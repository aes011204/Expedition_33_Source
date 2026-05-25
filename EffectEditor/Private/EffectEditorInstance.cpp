#include "EffectEditorInstance.h"

#include "CommandHistory.h"
#include "Editor_Context.h"
#include "Editor_Window.h"
#include "EffectEditorCamera.h"
#include "EffectEditor_Manager.h"
#include "EffectEmitterRegistration.h"
#include "GameInstance.h"
#include "ImGui_Manager.h"
#include "Level_EffectEditor.h"
#include "Notification_Manager.h"
#include "RenderTarget.h"

NS_BEGIN(EffectEditor)

IMPLEMENT_SINGLETON(EffectEditorInstance)

namespace Internal
{
    static bool Is_PreviewCameraDragInputHeld()
    {
        const bool altHeld = GAME->KeyPress(KEY_TYPE::ALT) || GAME->KeyDown(KEY_TYPE::ALT);

        return GAME->KeyPress(KEY_TYPE::RBUTTON) ||
               (altHeld && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)));
    }
}

EffectEditorInstance::EffectEditorInstance()
{
}

EffectEditorInstance::~EffectEditorInstance()
{
    Free();
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

    // Effect emitter 등록
    Register_ClientEffectEmitters();

    _runtimeState = EffectEditorRuntimeState::Edit;
    _runtimeTimeScale = 1.f;
    _isTrailPreviewVisible = false;
    Queue_PreviewViewportSize(kDefaultPreviewViewportWidth, kDefaultPreviewViewportHeight);

    return S_OK;
}

void EffectEditorInstance::Release_Editor()
{
    End_PreviewCameraDrag();

    _sharedViewportRenderTarget.reset();
    _sceneGridBatch.reset();
    _sceneGridEffect.reset();
    _sceneGridInputLayout.Reset();
    _sceneGridDepthState.Reset();
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
    _hasPendingPreviewViewportSize = false;
    _pendingPreviewViewportWidth = kDefaultPreviewViewportWidth;
    _pendingPreviewViewportHeight = kDefaultPreviewViewportHeight;
    _appliedPreviewViewportWidth = 0;
    _appliedPreviewViewportHeight = 0;
    _sceneViewMode = SceneViewMode::Authoring;

    _runtimeState = EffectEditorRuntimeState::Edit;
    _runtimeTimeScale = 1.f;
    _isTrailPreviewVisible = false;
    _hasPendingWindowFocus = false;
    _pendingWindowFocus = EditorViewportTarget::Game;
    _pendingSingleFrameStep = false;
    _copiedObjectJson.clear();
    _copiedLayerTag.clear();
    _isSceneViewportInputEnabled = false;
    _isSceneViewportInputBlocked = false;
    _isGameViewportInputEnabled = false;
    _isPreviewCameraControlEnabled = false;
    _isPreviewCameraDragging = false;
    _hasPreviewCameraLockScreenRect = false;
    _previewCameraLockScreenRect = {};
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

void EffectEditorInstance::Render_Editor(EffectEditorRenderPerfSample* outPerfSample)
{
    chrono::steady_clock::time_point begin{};
    if (outPerfSample != nullptr)
        begin = chrono::steady_clock::now();

    _editorManager->Render();
    if (outPerfSample != nullptr)
        outPerfSample->editorWindowsMs = chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count();

    if (_notificationManager)
    {
        if (outPerfSample != nullptr)
            begin = chrono::steady_clock::now();

        _notificationManager->Render();

        if (outPerfSample != nullptr)
            outPerfSample->notificationMs = chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count();
    }

    if (outPerfSample != nullptr)
        begin = chrono::steady_clock::now();
    _imguiManager->Render();
    if (outPerfSample != nullptr)
        outPerfSample->imguiMs = chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count();
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

bool EffectEditorInstance::IsPlaying() const
{
    return EffectEditorRuntimeState::Play == _runtimeState;
}

bool EffectEditorInstance::IsPaused() const
{
    return EffectEditorRuntimeState::Pause == _runtimeState;
}

float EffectEditorInstance::Get_RuntimeTimeScale() const
{
    return _runtimeTimeScale;
}

ID3D11ShaderResourceView* EffectEditorInstance::Get_SharedViewportSRV() const
{
    if (nullptr == _sharedViewportRenderTarget)
        return nullptr;

    return _sharedViewportRenderTarget->Get_SRV();
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

void EffectEditorInstance::Play()
{
    End_PreviewCameraDrag();

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

    End_PreviewCameraDrag();

    if (!Is_PreviewCameraActive())
        Activate_PreviewCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    _runtimeState = EffectEditorRuntimeState::Pause;
    Request_WindowFocus(EditorViewportTarget::Scene);
}

void EffectEditorInstance::Stop()
{
    End_PreviewCameraDrag();

    _runtimeState = EffectEditorRuntimeState::Edit;

    const Shared<Level> currentLevel = GAME->Current_LevelType();
    const Shared<Level_EffectEditor> effectEditorLevel = dynamic_pointer_cast<Level_EffectEditor>(currentLevel);
    if (effectEditorLevel != nullptr && FAILED(effectEditorLevel->Reset_PreviewRuntime()))
        LOG_ERROR("Failed to reset EffectEditor preview runtime.");

    Activate_PreviewCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EditorViewportTarget::Scene);
}

void EffectEditorInstance::Resume()
{
    if (EffectEditorRuntimeState::Pause != _runtimeState)
        return;

    End_PreviewCameraDrag();

    _runtimeState = EffectEditorRuntimeState::Play;
    Activate_TargetCamera();

    GAME->Set_FreeCameraInputEnabled(false);
    GAME->Unlock_Mouse();

    Request_WindowFocus(EditorViewportTarget::Game);
}

void EffectEditorInstance::Set_RuntimeTimeScale(float timeScale)
{
    _runtimeTimeScale = clamp(timeScale, 0.01f, 8.f);
}

void EffectEditorInstance::Reset_RuntimeTimeScale()
{
    _runtimeTimeScale = 1.f;
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

void EffectEditorInstance::Request_RestartPreview()
{
    const Shared<Level> currentLevel = GAME->Current_LevelType();
    const Shared<Level_EffectEditor> effectEditorLevel = dynamic_pointer_cast<Level_EffectEditor>(currentLevel);
    if (nullptr == effectEditorLevel)
        return;

    if (FAILED(effectEditorLevel->Restart_PreviewFromAuthoring()))
        LOG_ERROR("Failed to restart EffectEditor preview.");
}

void EffectEditorInstance::Set_TrailPreviewVisible(bool visible)
{
    _isTrailPreviewVisible = visible;
}

void EffectEditorInstance::Set_TrailPreviewDebugRenderEnabled(bool enabled)
{
    _isTrailPreviewDebugRenderEnabled = enabled;
}

void EffectEditorInstance::Set_TrailPreviewLocalOffsets(const Vec3& baseLocalOffset, const Vec3& tipLocalOffset)
{
    _trailPreviewBaseLocalOffset = baseLocalOffset;
    _trailPreviewTipLocalOffset = tipLocalOffset;
}

void EffectEditorInstance::Reset_TrailPreviewLocalOffsets()
{
    _trailPreviewBaseLocalOffset = kDefaultTrailPreviewBaseLocalOffset;
    _trailPreviewTipLocalOffset = kDefaultTrailPreviewTipLocalOffset;
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

void EffectEditorInstance::Copy_SelectedObject()
{
    if (_editorContext == nullptr)
        return;

    const Shared<GameObject> selectedObject = _editorContext->Get_Selection().selectedObject;
    if (selectedObject == nullptr)
        return;

    wstring layerTag{};
    if (!Find_ObjectLayerTag(selectedObject, layerTag))
        return;

    _copiedObjectJson = selectedObject->To_Json().dump();
    _copiedLayerTag = layerTag;
}

Shared<GameObject> EffectEditorInstance::Paste_CopiedObject()
{
    Shared<GameObject> gameObject = Instantiate_GameObjectFromCopiedData();
    if (gameObject == nullptr)
        return nullptr;

    if (FAILED(GAME->Add_GameObject(GAME->Current_LevelIndex(), _copiedLayerTag, gameObject)))
        return nullptr;

    if (_editorContext)
        _editorContext->Set_SelectObject(gameObject);

    return gameObject;
}

Shared<GameObject> EffectEditorInstance::Duplicate_SelectedObject()
{
    Copy_SelectedObject();
    return Paste_CopiedObject();
}

bool EffectEditorInstance::Delete_SelectedObject()
{
    if (_editorContext == nullptr)
        return false;

    const Shared<GameObject> selectedObject = _editorContext->Get_Selection().selectedObject;
    if (selectedObject == nullptr)
        return false;

    if (!GAME->Remove_GameObject(GAME->Current_LevelIndex(), selectedObject))
        return false;

    _editorContext->Clear_Selection();
    return true;
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

void EffectEditorInstance::Set_PreviewCameraLockScreenRect(const RECT& lockRectScreen)
{
    _previewCameraLockScreenRect = lockRectScreen;
    _hasPreviewCameraLockScreenRect = true;

    if (_isPreviewCameraDragging)
        GAME->Set_MouseLockOverrideRect(lockRectScreen);
}

void EffectEditorInstance::Clear_PreviewCameraLockScreenRect()
{
    _hasPreviewCameraLockScreenRect = false;
    _previewCameraLockScreenRect = {};

    if (_isPreviewCameraDragging)
        GAME->Clear_MouseLockOverrideRect();
}

void EffectEditorInstance::Begin_PreviewCameraDrag()
{
    if (_isPreviewCameraDragging)
        return;

    _isPreviewCameraDragging = true;
    if (_hasPreviewCameraLockScreenRect)
        GAME->Set_MouseLockOverrideRect(_previewCameraLockScreenRect);

    GAME->Begin_EditorMouseLockSession();
    GAME->Lock_Mouse();
}

void EffectEditorInstance::End_PreviewCameraDrag()
{
    _isPreviewCameraDragging = false;
    GAME->Clear_MouseLockOverrideRect();
    GAME->End_EditorMouseLockSession();
    GAME->Unlock_Mouse();
}

void EffectEditorInstance::Apply_RuntimeInputPolicy()
{
    const Shared<Editor_Window> sceneView = Get_Window(L"Scene");
    const Shared<Editor_Window> gameView = Get_Window(L"Game");

    if (_isPreviewCameraDragging && !Internal::Is_PreviewCameraDragInputHeld())
        End_PreviewCameraDrag();

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
            {
                GAME->Begin_EditorMouseLockSession();
                GAME->Lock_Mouse();
            }
            else
                End_PreviewCameraDrag();
        }
        else
        {
            GAME->Set_PlayerInputBlocked(!gameFocused);
            GAME->Set_FreeCameraInputEnabled(false);
            _isPreviewCameraControlEnabled = false;
            End_PreviewCameraDrag();

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
    {
        GAME->Begin_EditorMouseLockSession();
        GAME->Lock_Mouse();
    }
    else
        End_PreviewCameraDrag();
}

Shared<RenderTarget> EffectEditorInstance::Prepare_SharedViewportRenderTarget(uint32 width, uint32 height)
{
    if (width == 0)
        width = 1;

    if (height == 0)
        height = 1;

    if (nullptr == _sharedViewportRenderTarget)
    {
        _sharedViewportRenderTarget = RenderTarget::Create(
            L"RenderTarget",
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

void EffectEditorInstance::Set_SceneViewMode(SceneViewMode viewMode)
{
    _sceneViewMode = viewMode;
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

void EffectEditorInstance::Ensure_SceneGridResources()
{
    if (_sceneGridBatch && _sceneGridEffect && _sceneGridInputLayout && _sceneGridDepthState)
        return;

    static constexpr D3D11_INPUT_ELEMENT_DESC sceneGridInputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    _sceneGridBatch = make_shared<PrimitiveBatch<VertexPositionColor>>(_context.Get());
    _sceneGridEffect = make_shared<BasicEffect>(_device.Get());

    if (nullptr == _sceneGridBatch || nullptr == _sceneGridEffect)
    {
        _sceneGridBatch.reset();
        _sceneGridEffect.reset();
        return;
    }

    _sceneGridEffect->SetVertexColorEnabled(true);

    const void* shaderByteCode = nullptr;
    size_t shaderByteCodeLength = 0;
    _sceneGridEffect->GetVertexShaderBytecode(&shaderByteCode, &shaderByteCodeLength);

    if (FAILED(
        _device->CreateInputLayout(
            sceneGridInputElements,
            static_cast<UINT>(_countof(sceneGridInputElements)),
            shaderByteCode,
            shaderByteCodeLength,
            _sceneGridInputLayout.GetAddressOf())
    ))
    {
        _sceneGridBatch.reset();
        _sceneGridEffect.reset();
        _sceneGridInputLayout.Reset();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    if (FAILED(_device->CreateDepthStencilState(&depthDesc, _sceneGridDepthState.GetAddressOf())))
    {
        _sceneGridBatch.reset();
        _sceneGridEffect.reset();
        _sceneGridInputLayout.Reset();
        _sceneGridDepthState.Reset();
    }
}

void EffectEditorInstance::Render_SceneGrid()
{
    if (!_device || !_context)
        return;

    const Matrix* viewMatrix = GAME->Get_Transform(D3DTS::View);
    const Matrix* projMatrix = GAME->Get_Transform(D3DTS::Proj);
    if (nullptr == viewMatrix || nullptr == projMatrix)
        return;

    Ensure_SceneGridResources();
    if (!_sceneGridBatch || !_sceneGridEffect || !_sceneGridInputLayout || !_sceneGridDepthState)
        return;

    _sceneGridEffect->SetWorld(Matrix::Identity);
    _sceneGridEffect->SetView(*viewMatrix);
    _sceneGridEffect->SetProjection(*projMatrix);

    ID3D11GeometryShader* nullGeometryShader = nullptr;
    _context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullShaderResourceViews[1] = { nullptr };
    _context->PSSetShaderResources(0, 1, nullShaderResourceViews);

    _context->RSSetState(nullptr);
    _context->IASetInputLayout(_sceneGridInputLayout.Get());
    _context->OMSetDepthStencilState(_sceneGridDepthState.Get(), 0);
    _context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _sceneGridEffect->Apply(_context.Get());

    _sceneGridBatch->Begin();

    const float step = kSceneGridHalfExtent * 2.f / static_cast<float>(kSceneGridDivisions);
    const XMVECTOR gridColor = XMVectorSet(0.58f, 0.58f, 0.62f, 1.f);
    const XMVECTOR xAxisColor = XMVectorSet(0.95f, 0.32f, 0.32f, 1.f);
    const XMVECTOR zAxisColor = XMVectorSet(0.28f, 0.72f, 1.f, 1.f);

    for (int i = 0; i <= kSceneGridDivisions; ++i)
    {
        const float offset = -kSceneGridHalfExtent + step * static_cast<float>(i);
        const XMVECTOR colorX = fabsf(offset) < 0.0001f ? zAxisColor : gridColor;
        const XMVECTOR colorZ = fabsf(offset) < 0.0001f ? xAxisColor : gridColor;

        VertexPositionColor lineXStart(
            Vec3(-kSceneGridHalfExtent, kSceneGridHeight, offset),
            colorX
        );
        VertexPositionColor lineXEnd(
            Vec3(kSceneGridHalfExtent, kSceneGridHeight, offset),
            colorX
        );
        _sceneGridBatch->DrawLine(lineXStart, lineXEnd);

        VertexPositionColor lineZStart(
            Vec3(offset, kSceneGridHeight, -kSceneGridHalfExtent),
            colorZ
        );
        VertexPositionColor lineZEnd(
            Vec3(offset, kSceneGridHeight, kSceneGridHalfExtent),
            colorZ
        );
        _sceneGridBatch->DrawLine(lineZStart, lineZEnd);
    }

    _sceneGridBatch->End();
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

bool EffectEditorInstance::Find_ObjectLayerTag(const Shared<GameObject>& gameObject, wstring& outLayerTag) const
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
        }
    );

    return found;
}

Shared<GameObject> EffectEditorInstance::Instantiate_GameObjectFromCopiedData() const
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

void EffectEditorInstance::Free()
{
    Release_Editor();
    __super::Free();
}

NS_END
