#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Shader;

class ENGINE_DLL RenderTarget final : public Base
{
public:
	RenderTarget(ComPtr<Device> device, ComPtr<Context> context);
	virtual ~RenderTarget() { Free(); }

public:
	HRESULT Initialize(uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil = true);
	HRESULT Resize(uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil = true);
	HRESULT Begin(bool clearRenderTarget = true, bool clearDepthStencil = true);
	HRESULT Bind_ShaderResource(Shader* shader, const char* constantName);

	void Clear_RenderTarget();
	void Clear_DepthStencil();

public:
	ID3D11RenderTargetView* Get_RTV() const { return _rtv.Get(); }
	ID3D11DepthStencilView* Get_DSV() const { return _dsv.Get(); }
	ID3D11ShaderResourceView* Get_SRV() const { return _srv.Get(); }

	uint32 Get_Width() const { return _width; }
	uint32 Get_Height() const { return _height; }

private:
	void Release_TargetResources();

private:
	ComPtr<Device> _device = nullptr;
	ComPtr<Context> _context = nullptr;
	ComPtr<Texture2D> _texture = nullptr;
	ComPtr<RenderTargetView> _rtv = nullptr;
	ComPtr<ShaderResourceView> _srv = nullptr;
	ComPtr<Texture2D> _depthStencilTexture = nullptr;
	ComPtr<DepthStencil> _dsv = nullptr;

	uint32 _width = 0;
	uint32 _height = 0;
	DXGI_FORMAT _pixelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	Vec4 _clearColor = Vec4(0.f, 0.f, 0.f, 1.f);

public:
	static Shared<RenderTarget> Create(ComPtr<Device> device, ComPtr<Context> context,
		uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil = true);

	virtual void Free() override;
};

NS_END
