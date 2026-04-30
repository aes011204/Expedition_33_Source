#include "EditorInstance.h"

#include "RenderTarget.h"
#include "Editor_Manager.h"

IMPLEMENT_SINGLETON(EditorInstance)

EditorInstance::EditorInstance()
{
}

EditorInstance::~EditorInstance()
{
	Release();
}

HRESULT EditorInstance::Initialize_Editor(const EDITOR_DESC& EditorDesc, ComPtr<Device> device, ComPtr<Context> context)
{
	_editorDesc = EditorDesc;
	_device = device;
	_context = context;

	if (nullptr == _editorDesc.hWnd || nullptr == _device || nullptr == _context)
		return E_FAIL;

	_editorManager = Editor_Manager::Create();
	if (nullptr == _editorManager)
		return E_FAIL;

	_runtimeState = EDITOR_RUNTIME_STATE::Edit;
	_runtimeTimeScale = 1.f;

	return S_OK;
}

void EditorInstance::Update_Editor(float fTimeDelta)
{
	if (nullptr != _editorManager)
		_editorManager->Update(fTimeDelta);
}

void EditorInstance::Render_Editor()
{
	if (nullptr != _editorManager)
		_editorManager->Render();
}

void EditorInstance::Release()
{
	_sceneRenderTarget.reset();
	_gameRenderTarget.reset();
	_editorManager.reset();
	_context.Reset();
	_device.Reset();
	_editorDesc = {};
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

Shared<RenderTarget> EditorInstance::Prepare_ViewportRenderTarget(EDITOR_VIEWPORT_TARGET targetType, uint32 width, uint32 height)
{
	switch (targetType)
	{
	case EDITOR_VIEWPORT_TARGET::Scene:
		return Prepare_RenderTarget(_sceneRenderTarget, width, height, Vec4(0.03f, 0.035f, 0.04f, 1.f));
	case EDITOR_VIEWPORT_TARGET::Game:
		return Prepare_RenderTarget(_gameRenderTarget, width, height, Vec4(0.f, 0.f, 1.f, 1.f));
	default:
		return nullptr;
	}
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

Shared<EditorWindow> EditorInstance::Get_Window(const wstring& strWindowName) const
{
	if (nullptr == _editorManager)
		return nullptr;

	return _editorManager->Find_Window(strWindowName);
}

HWND EditorInstance::Get_WindowHandle() const
{
	return _editorDesc.hWnd;
}

float EditorInstance::Clamp_RuntimeTimeScale(float fTimeScale)
{
	if (fTimeScale < 0.01f)
		return 0.01f;

	if (fTimeScale > 8.f)
		return 8.f;

	return fTimeScale;
}

Shared<RenderTarget> EditorInstance::Prepare_RenderTarget(Shared<RenderTarget>& renderTarget, uint32 width, uint32 height, const Vec4& clearColor)
{
	if (0 == width)
		width = 1;

	if (0 == height)
		height = 1;

	if (nullptr == renderTarget)
	{
		renderTarget = RenderTarget::Create(_device, _context, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
	}
	else if (FAILED(renderTarget->Resize(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor)))
	{
		return nullptr;
	}

	if (nullptr != renderTarget)
		renderTarget->Begin();

	return renderTarget;
}



