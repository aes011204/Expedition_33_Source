#include "EditorInstance.h"

#include "RenderTarget.h"
#include "Editor_Manager.h"
#include "GameInstance.h"
#include "ImGui_Manager.h"

IMPLEMENT_SINGLETON(EditorInstance)

EditorInstance::EditorInstance()
{
}

EditorInstance::~EditorInstance()
{
    Release();
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

    // 처음에는 Edit 모드로, 1배속으로 시작
    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    _runtimeTimeScale = 1.f;

    return S_OK;
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

void EditorInstance::Release()
{
    _sharedViewportRenderTarget.reset();
    _imguiManager.reset();
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

void EditorInstance::Play()
{
    _runtimeState = EDITOR_RUNTIME_STATE::Play;
}

void EditorInstance::Pause()
{
    if (EDITOR_RUNTIME_STATE::Play == _runtimeState)
        _runtimeState = EDITOR_RUNTIME_STATE::Pause;
}

void EditorInstance::Stop()
{
    _runtimeState = EDITOR_RUNTIME_STATE::Edit;
}

void EditorInstance::Resume()
{
    if (EDITOR_RUNTIME_STATE::Pause == _runtimeState)
        _runtimeState = EDITOR_RUNTIME_STATE::Play;
}

bool EditorInstance::IsPlaying() const
{
    return EDITOR_RUNTIME_STATE::Play == _runtimeState;
}

bool EditorInstance::IsPaused() const
{
    return EDITOR_RUNTIME_STATE::Pause == _runtimeState;
}

void EditorInstance::Set_RuntimeTimeScale(float fTimeScale)
{
    _runtimeTimeScale = Clamp_RuntimeTimeScale(fTimeScale);
}

void EditorInstance::Reset_RuntimeTimeScale()
{
    _runtimeTimeScale = 1.f;
}

float EditorInstance::Get_RuntimeTimeScale() const
{
    return _runtimeTimeScale;
}

Shared<Editor_Window> EditorInstance::Get_Window(const wstring& strWindowName) const
{
    if (nullptr == _editorManager)
        return nullptr;

    return _editorManager->Find_Window(strWindowName);
}

HWND EditorInstance::Get_WindowHandle() const
{
    return _editorDesc.hWnd;
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

float EditorInstance::Clamp_RuntimeTimeScale(float fTimeScale)
{
    if (fTimeScale < 0.01f)
        return 0.01f;

    if (fTimeScale > 8.f)
        return 8.f;

    return fTimeScale;
}
