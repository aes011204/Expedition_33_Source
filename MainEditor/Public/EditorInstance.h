#pragma once

#include "Editor_Defines.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)

class Editor_Window;
class Editor_Manager;

class EditorInstance final
{
	DECLARE_SINGLETON(EditorInstance)

private:
	EditorInstance();
	~EditorInstance();

public:
	HRESULT Initialize_Editor(const EDITOR_DESC& EditorDesc, ComPtr<Device> device, ComPtr<Context> context);
	void Update_Editor(float fTimeDelta);

	void Render_Editor();
	void Release();

public:
	void Play();
	void Pause();
	void Stop();
	void Resume();

	bool IsPlaying() const;
	bool IsPaused() const;

	/* Scene/Game 뷰가 표시할 렌더 타겟을 요청한 크기에 맞춰 준비한다. */
	Shared<RenderTarget> Prepare_ViewportRenderTarget(EDITOR_VIEWPORT_TARGET targetType, uint32 width, uint32 height);

	/* 런타임 재생 중 적용할 시간 배율을 설정한다. */
	void Set_RuntimeTimeScale(float fTimeScale);

	/* 런타임 시간 배율을 기본값으로 되돌린다. */
	void Reset_RuntimeTimeScale();

	/* 런타임 갱신에 적용할 시간 배율을 반환한다. */
	float Get_RuntimeTimeScale() const;

public:
	/* 메뉴나 단축키에서 등록된 에디터 창을 찾을 때 호출한다. */
	Shared<Editor_Window> Get_Window(const wstring& strWindowName) const;

	/* Win32 메시지와 대화상자에 사용할 메인 호스트 윈도우를 반환한다. */
	HWND Get_WindowHandle() const;

private:
	/* 유효하지 않은 시간 배율 입력을 안정적인 범위로 제한한다. */
	static float Clamp_RuntimeTimeScale(float fTimeScale);
	Shared<RenderTarget> Prepare_RenderTarget(Shared<RenderTarget>& renderTarget, uint32 width, uint32 height, const Vec4& clearColor);

private:
	EDITOR_DESC _editorDesc = {};             /* 호스트 윈도우와 기본 뷰포트 설정. */
	ComPtr<Device>  _device;                  /* 에디터 UI와 뷰 리소스에서 사용할 D3D 장치. */
	ComPtr<Context> _context;                 /* 에디터 UI와 뷰 렌더링에 사용할 D3D 컨텍스트. */
	Unique<Editor_Manager> _editorManager;    /* 에디터 창과 도킹 공간 상태를 소유하는 매니저. */
	Shared<RenderTarget> _sceneRenderTarget;  /* Scene View가 표시할 오프스크린 렌더 타겟. */
	Shared<RenderTarget> _gameRenderTarget;   /* Game View가 표시할 오프스크린 렌더 타겟. */

    /* 현재 편집/재생/일시정지 상태. */
	EDITOR_RUNTIME_STATE _runtimeState = EDITOR_RUNTIME_STATE::Edit; 

	float _runtimeTimeScale = 1.f;            /* 런타임 재생 중 적용할 시간 배율. */
};

NS_END
