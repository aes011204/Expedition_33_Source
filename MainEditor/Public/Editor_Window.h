#pragma once

#include "Editor_Defines.h"

NS_BEGIN(MainEditor)

class Editor_Window
{
public:
	explicit Editor_Window(const wstring& strWindowName);
	virtual ~Editor_Window() = default;

public:
	/* Editor_Manager가 매 프레임 창 상태를 갱신할 때 호출한다. */
	virtual void Update(float fTimeDelta);

	/* Editor_Manager가 ImGui 프레임 안에서 이 창을 그릴 때 호출한다. */
	virtual void Render();

	/* 메뉴나 툴바에서 창 표시 상태를 바꿀 때 호출한다. */
	void Set_Open(bool bOpen);

	/* 도킹 메뉴에서 창 제목이나 검색 키가 필요할 때 호출한다. */
	const wstring& Get_WindowName() const;

	/* 닫힌 창을 렌더링에서 건너뛰기 위해 호출한다. */
	bool Is_Open() const;

protected:
	wstring _window = {};                        /* 창 제목과 등록 키. */
	bool _isOpen = true;                         /* 현재 ImGui 창 열림 상태. */
};

NS_END
