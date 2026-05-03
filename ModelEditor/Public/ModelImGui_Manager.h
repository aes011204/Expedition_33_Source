#pragma once

#include "Engine_Define.h"

NS_BEGIN(ModelEditor)

class LogWindow;
class ModelConverter;

class ModelImGui_Manager final
{
private:
	ModelImGui_Manager() = default;

public:
	~ModelImGui_Manager();

public:
	HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
	void Update(float timeDelta);
	void Render();

private:
	void Free();
	void ImGuiStyleSetting();
	string Get_ClientResourcePath(const wchar_t* relativePath);

private:
    Shared<LogWindow>           _logWindow;
    Shared<ModelConverter>      _modelConverter;

public:
	static Unique<ModelImGui_Manager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
};

NS_END
