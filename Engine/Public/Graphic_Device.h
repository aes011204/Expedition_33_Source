#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Graphic_Device final : public Base
{
private:
    Graphic_Device();

public:
    virtual ~Graphic_Device() = default;
	HRESULT Initialize(HWND hWnd, WinMode isWindowed, uint32 iWinSizeX, uint32 iWinSizeY,
		ComPtr<Device>& device, ComPtr<Context>& context);
	HRESULT Clear_BackBuffer_View(const Vec4& clearColor);
	HRESULT Clear_DepthStencil_View();
	HRESULT Present();

	ComPtr<Device> Get_Device() const { return _device; }
	ComPtr<Context> Get_Context() const { return _deviceContext; }

private:
	ComPtr<Device> _device = nullptr;
	ComPtr<Context> _deviceContext = nullptr;
	ComPtr<IDXGISwapChain> _swapChain = nullptr;
	ComPtr<ID3D11RenderTargetView> _backBufferRTV = nullptr;
	ComPtr<ID3D11DepthStencilView> _depthStencilView = nullptr;

private:
	HRESULT Ready_SwapChain(HWND hWnd, WinMode isWindowed, uint32 iWinCX, uint32 iWinCY);
	HRESULT Ready_BackBufferRenderTargetView();
	HRESULT Ready_DepthStencilView(uint32 iWinCX, uint32 iWinCY);

public:
	static Unique<Graphic_Device> Create(HWND hWnd, WinMode isWindowed, uint32 iWinSizeX, uint32 iWinSizeY, ComPtr<Device>& device, ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END
