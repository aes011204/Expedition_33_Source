#pragma once

#include "Engine_Define.h"

NS_BEGIN(ModelEditor)

class LogWindow;

class ImGui_Manager final
{
private:
	ImGui_Manager() = default;

public:
	~ImGui_Manager();

public:
	HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
	void Update(float timeDelta);
	void Render();

private:
	void Free();
	void ImGuiStyleSetting();
	string Get_ClientResourcePath(const wchar_t* relativePath);

private:
    Shared<LogWindow> _logWindow;

public:
	static Unique<ImGui_Manager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
};

NS_END
