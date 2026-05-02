#pragma once

#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Editor_Manager final
{
public:
    Editor_Manager();
	~Editor_Manager() = default;

public:
	HRESULT Initialize();
	void Update(float timeDelta);
	void Render();

    // View 추가
	void Add_Window(const Shared<Editor_Window>& window);
	Shared<Editor_Window> Find_Window(const wstring& windowName) const;

private:
	void Render_DockSpace();

private:
    /* 창 이름을 키로 보관하는 에디터 뷰 목록. */
	map<wstring, Shared<Editor_Window>> _windows = {}; 

public:
	static Unique<Editor_Manager> Create();
};

NS_END
