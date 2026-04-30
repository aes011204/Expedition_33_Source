#include "EditorInstance.h"

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

	_runtimeState = EDITOR_RUNTIME_STATE::EDIT;
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
	_editorManager.reset();
	_context.Reset();
	_device.Reset();
	_editorDesc = {};
	_runtimeState = EDITOR_RUNTIME_STATE::EDIT;
	_runtimeTimeScale = 1.f;
}

void EditorInstance::Play()
{
	_runtimeState = EDITOR_RUNTIME_STATE::PLAY;
}

void EditorInstance::Pause()
{
	if (EDITOR_RUNTIME_STATE::PLAY == _runtimeState)
		_runtimeState = EDITOR_RUNTIME_STATE::PAUSE;
}

void EditorInstance::Stop()
{
	_runtimeState = EDITOR_RUNTIME_STATE::EDIT;
}

void EditorInstance::Resume()
{
	if (EDITOR_RUNTIME_STATE::PAUSE == _runtimeState)
		_runtimeState = EDITOR_RUNTIME_STATE::PLAY;
}

bool EditorInstance::IsPlaying() const
{
	return EDITOR_RUNTIME_STATE::PLAY == _runtimeState;
}

bool EditorInstance::IsPaused() const
{
	return EDITOR_RUNTIME_STATE::PAUSE == _runtimeState;
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



