#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Graphic_Device final : public Base
{
public:
    Graphic_Device();
    virtual ~Graphic_Device() = default;

	HRESULT Initialize(HWND hWnd, WinMode isWindowed, uint32 winSizeX, uint32 winSizeY,
		ComPtr<Device>& device, ComPtr<Context>& context);

	HRESULT Clear_BackBuffer_View(const Vec4& clearColor);
	HRESULT Clear_DepthStencil_View();
	HRESULT Present();

    void    Bind_BackBuffer();
    HRESULT Resize(uint32 width, uint32 height);

	ComPtr<Device>  Get_Device() const  { return _device; }
	ComPtr<Context> Get_Context() const { return _deviceContext; }

    uint32 Get_ViewportWidth() const { return _viewportWidth; }
    uint32 Get_ViewportHeight() const { return _viewportHeight; }

    ComPtr<IDXGISwapChain> Get_SwapChain() const { return _swapChain; }


private:
	ComPtr<Device>              _device = nullptr;
	ComPtr<Context>             _deviceContext = nullptr;
	ComPtr<SwapChain>           _swapChain = nullptr;
	ComPtr<RenderTargetView>    _backBufferRTV = nullptr;
	ComPtr<DepthStencilView>    _depthStencilView = nullptr;

private:
	HRESULT Ready_SwapChain(HWND hWnd, WinMode isWindowed, uint32 windowWidth, uint32 windowHeight);
	HRESULT Ready_BackBufferRenderTargetView();
	HRESULT Ready_DepthStencilView(uint32 windowWidth, uint32 windowHeight);

private:
    uint32 _viewportWidth = 0;
    uint32 _viewportHeight = 0;


public:
	static Unique<Graphic_Device> Create(HWND hWnd, WinMode isWindowed, uint32 windowSizeX, uint32 windowSizeY, ComPtr<Device>& device, ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END
