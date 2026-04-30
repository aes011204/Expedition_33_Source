#pragma once

#include "Editor_Defines.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)

class Editor_Window;
class Editor_Manager;
class ImGui_Manager;

class EditorInstance final
{
	DECLARE_SINGLETON(EditorInstance)

private:
	EditorInstance();
	~EditorInstance();

public:
	HRESULT Initialize_Editor(const EDITOR_DESC& EditorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context);

	void Update_Editor(float timeDelta);
	void Render_Editor();
	void Release();

public:
	Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
	void Render_RuntimeViewport();

	void Request_ViewportSize(EDITOR_VIEWPORT_TARGET targetType, uint32 width, uint32 height);
	ID3D11ShaderResourceView* Get_SharedViewportSRV() const;

public:
	void Play();
	void Pause();
	void Stop();
	void Resume();

	bool IsPlaying() const;
	bool IsPaused() const;

	void Set_RuntimeTimeScale(float fTimeScale);
	void Reset_RuntimeTimeScale();

	float Get_RuntimeTimeScale() const;

public:
	Shared<Editor_Window> Get_Window(const wstring& strWindowName) const;

	HWND Get_WindowHandle() const;

private:
	static float Clamp_RuntimeTimeScale(float fTimeScale);

private:
	ComPtr<Device>      _device;
	ComPtr<Context>     _context;
	EDITOR_DESC         _editorDesc = {};

private:
	Unique<Editor_Manager> _editorManager;
    Unique<ImGui_Manager>  _imguiManager;

private:
	Shared<RenderTarget> _sharedViewportRenderTarget = nullptr;

    // 에디터에서 뷰포트 창 사이즈 변경 시 
	uint32 _requestedViewportWidth  = 1;
	uint32 _requestedViewportHeight = 1;
	EDITOR_VIEWPORT_TARGET _lastViewportRequester = EDITOR_VIEWPORT_TARGET::Game;

	EDITOR_RUNTIME_STATE _runtimeState = EDITOR_RUNTIME_STATE::Edit;

    // 런타임 배율
	float _runtimeTimeScale = 1.f;
};

NS_END
