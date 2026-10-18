#include "Graphic_Device.h"

namespace Internal
{
bool Is_FlipSwapEffect(DXGI_SWAP_EFFECT swapEffect)
{
    return swapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD ||
           swapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
}

uint32 Resolve_BackBufferCount(DXGI_SWAP_EFFECT swapEffect)
{
    return Is_FlipSwapEffect(swapEffect) ? 2u : 1u;
}
}

Graphic_Device::Graphic_Device()
{
}

HRESULT Graphic_Device::Initialize(HWND hWnd, WinMode isWindowed, uint32 winSizeX, uint32 winSizeY, DXGI_SWAP_EFFECT swapEffect, ComPtr<Device>& device, ComPtr<Context>& context)
{
	uint32 flag = 0;

#if defined(_DEBUG) && ENGINE_ENABLE_DX11_DEBUG_LAYER
	flag = D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL FeatureLevel = {};

	if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, flag, nullptr, 0, D3D11_SDK_VERSION, _device.GetAddressOf(), &FeatureLevel, _deviceContext.GetAddressOf())))
		return E_FAIL;

	if (FAILED(Ready_SwapChain(hWnd, isWindowed, winSizeX, winSizeY, swapEffect)))
		return E_FAIL;

	if (FAILED(Ready_BackBufferRenderTargetView()))
		return E_FAIL;

	if (FAILED(Ready_DepthStencilView(winSizeX, winSizeY)))
		return E_FAIL;

    _viewportWidth = winSizeX;
    _viewportHeight = winSizeY;

	// 초기화가 끝난 직후 백버퍼로 세팅
    Bind_BackBuffer();

	device = _device;
	context = _deviceContext;

	return S_OK;
}

HRESULT Graphic_Device::Clear_BackBuffer_View(const Vec4& clearColor)
{
	if (nullptr == _deviceContext || nullptr == _backBufferRTV)
		return E_FAIL;

	_deviceContext->ClearRenderTargetView(_backBufferRTV.Get(), reinterpret_cast<const float*>(&clearColor));
	return S_OK;
}

HRESULT Graphic_Device::Clear_DepthStencil_View()
{
	if (nullptr == _deviceContext || nullptr == _depthStencilView)
		return E_FAIL;

	_deviceContext->ClearDepthStencilView(_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
	return S_OK;
}

HRESULT Graphic_Device::Present()
{
	if (nullptr == _swapChain)
		return E_FAIL;

	return _swapChain->Present(0, 0);
}

void Graphic_Device::Bind_BackBuffer()
{
    if (nullptr == _deviceContext || nullptr == _backBufferRTV)
        return;

    ID3D11RenderTargetView* rtvs[] = { _backBufferRTV.Get() };
	_deviceContext->OMSetRenderTargets(1, rtvs, _depthStencilView.Get());

	D3D11_VIEWPORT ViewPortDesc = {};
	ViewPortDesc.TopLeftX = 0;
	ViewPortDesc.TopLeftY = 0;
	ViewPortDesc.Width = static_cast<float>(_viewportWidth);
	ViewPortDesc.Height = static_cast<float>(_viewportHeight);
	ViewPortDesc.MinDepth = 0.f;
	ViewPortDesc.MaxDepth = 1.f;

	_deviceContext->RSSetViewports(1, &ViewPortDesc);
}

HRESULT Graphic_Device::Resize(uint32 width, uint32 height)
{
    if (nullptr == _swapChain || 0 == width || 0 == height)
        return E_FAIL;

    _viewportWidth = width;
    _viewportHeight = height;

    _deviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    _backBufferRTV.Reset();
    _depthStencilView.Reset();

    if (FAILED(_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
        return E_FAIL;

    if (FAILED(Ready_BackBufferRenderTargetView()))
        return E_FAIL;

    if (FAILED(Ready_DepthStencilView(width, height)))
        return E_FAIL;

    Bind_BackBuffer();

    return S_OK;
}

HRESULT Graphic_Device::Ready_SwapChain(HWND hWnd, WinMode isWindowed, uint32 windowWidth, uint32 windowHeight, DXGI_SWAP_EFFECT swapEffect)
{
	ComPtr<IDXGIDevice> dxgiDevice = nullptr;
	if (FAILED(_device.As(&dxgiDevice)))
		return E_FAIL;

	ComPtr<IDXGIAdapter> adapter = nullptr;
	if (FAILED(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(adapter.GetAddressOf()))))
		return E_FAIL;

	ComPtr<IDXGIFactory> factory = nullptr;
	if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(factory.GetAddressOf()))))
		return E_FAIL;

	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	SwapChainDesc.BufferDesc.Width = windowWidth;
	SwapChainDesc.BufferDesc.Height = windowHeight;
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = Internal::Resolve_BackBufferCount(swapEffect);
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60; 
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.OutputWindow = hWnd;
	SwapChainDesc.Windowed = static_cast<BOOL>(isWindowed);
	SwapChainDesc.SwapEffect = swapEffect;

	if (FAILED(factory->CreateSwapChain(_device.Get(), &SwapChainDesc, _swapChain.GetAddressOf())))
		return E_FAIL;

	return S_OK;
}

HRESULT Graphic_Device::Ready_BackBufferRenderTargetView()
{
	if (nullptr == _device || nullptr == _swapChain)
		return E_FAIL;

	ComPtr<ID3D11Texture2D> backBufferTexture = nullptr;
	if (FAILED(_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBufferTexture.GetAddressOf()))))
		return E_FAIL;

	if (FAILED(_device->CreateRenderTargetView(backBufferTexture.Get(), nullptr, _backBufferRTV.GetAddressOf())))
		return E_FAIL;

#ifdef _DEBUG
    static const wchar_t* debugName = L"BackBufferRTV";
    _backBufferRTV->SetPrivateData(
        WKPDID_D3DDebugObjectNameW,
        static_cast<UINT>((wcslen(debugName) + 1) * sizeof(wchar_t)),
        debugName);
#endif

	return S_OK;
}

HRESULT Graphic_Device::Ready_DepthStencilView(uint32 windowWidth, uint32 windowHeight)
{
	if (nullptr == _device)
		return E_FAIL;

	ComPtr<ID3D11Texture2D> depthStencilTexture = nullptr;
	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = windowWidth;
	TextureDesc.Height = windowHeight;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	if (FAILED(_device->CreateTexture2D(&TextureDesc, nullptr, depthStencilTexture.GetAddressOf())))
		return E_FAIL;

	if (FAILED(_device->CreateDepthStencilView(depthStencilTexture.Get(), nullptr, _depthStencilView.GetAddressOf())))
		return E_FAIL;

	return S_OK;
}

Unique<Graphic_Device> Graphic_Device::Create(HWND hWnd, WinMode isWindowed, uint32 windowSizeX, uint32 windowSizeY, DXGI_SWAP_EFFECT swapEffect, ComPtr<Device>& device, ComPtr<Context>& context)
{
	Unique<Graphic_Device> instance{ new Graphic_Device() };

	if (FAILED(instance->Initialize(hWnd, isWindowed, windowSizeX, windowSizeY, swapEffect, device, context)))
	{
		MSG_BOX("Failed to Created : Graphic_Device");
		instance->Free();
		return nullptr;
	}

	return instance;
}

void Graphic_Device::Free()
{
	ComPtr<ID3D11Debug> d3dDebug = nullptr;
#if defined(DEBUG) || defined(_DEBUG)
	if (nullptr != _device && SUCCEEDED(_device.As(&d3dDebug)))
	{
	}
#endif

	if (_deviceContext)
	{
		_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		_deviceContext->ClearState();
		_deviceContext->Flush();
	}

	_depthStencilView.Reset();
	_backBufferRTV.Reset();
	_swapChain.Reset();
	_deviceContext.Reset();
	_device.Reset();

#if defined(DEBUG) || defined(_DEBUG)
	if (d3dDebug)
	{
		OutputDebugStringW(L"----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- \r ");
		OutputDebugStringW(L"                                                                    D3D11 Live Object ref Count Checker \r ");
		OutputDebugStringW(L"----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- \r ");
		d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		OutputDebugStringW(L"----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- \r ");
		OutputDebugStringW(L"                                                                    D3D11 Live Object ref Count Checker END \r ");
		OutputDebugStringW(L"----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- \r ");
	}
#endif
}
