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
class ModelTestPanel;
class ModelViewerObject;
class GltfMorphBuilder;

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
	Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
	ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
	void Capture_BackBuffer_ToViewport();

private:
	ComPtr<Device> _device = nullptr;
	ComPtr<Context> _context = nullptr;
	Shared<RenderTarget> _sharedViewportRenderTarget = nullptr;
	uint32 _requestedViewportWidth = 1;
	uint32 _requestedViewportHeight = 1;


public:
    Shared<ModelViewerObject>   Get_CurrentModelObject() const { return _modelObject; }
    void                        Set_CurrentModelObject(const Shared<ModelViewerObject>& modelObject) { _modelObject = modelObject; }
    Shared<ModelSceneWindow>    Get_ModelSceneWindow() const { return _modelSceneWindow; }

private:
    Shared<ModelViewerObject>   _modelObject{ nullptr };

private:
    Shared<LogWindow>           _logWindow;
    Shared<ModelConverter>      _modelConverter;
    Shared<ModelInspector>      _modelInspector;
    Shared<ModelSceneWindow>    _modelSceneWindow;
    Shared<ModelTestPanel>      _modelTestPanel;
    Shared<GltfMorphBuilder>    _gltfMorphBuilder;

public:
	static Unique<ModelImGui_Manager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
};

NS_END
