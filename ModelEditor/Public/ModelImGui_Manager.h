#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(ModelEditor)

class LogWindow;
class ModelConverter;
class ModelInspector;
class ModelSceneWindow;

class ModelImGui_Manager final 
{
    DECLARE_SINGLETON(ModelImGui_Manager)
public:
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

public:
	void Request_ViewportSize(uint32 width, uint32 height);
	Shared<::Engine::RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
	ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
	void Capture_BackBuffer_ToViewport();

private:
	ComPtr<Device> _device = nullptr;
	ComPtr<Context> _context = nullptr;
	Shared<::Engine::RenderTarget> _sharedViewportRenderTarget = nullptr;
	uint32 _requestedViewportWidth = 1;
	uint32 _requestedViewportHeight = 1;


public:
    Shared<GameObject>      Get_CurrentModelObject() const { return _modelObject; }
    void                    Set_CurrentModelObject(const Shared<GameObject>& modelObject) { _modelObject = modelObject; }

private:
    Shared<GameObject>          _modelObject{ nullptr };

private:
    Shared<LogWindow>           _logWindow;
    Shared<ModelConverter>      _modelConverter;
    Shared<ModelInspector>      _modelInspector;
    Shared<ModelSceneWindow>    _modelSceneWindow;

public:
	static Unique<ModelImGui_Manager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
};

NS_END
