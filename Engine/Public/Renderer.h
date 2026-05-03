#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Shader;
class VIBuffer_Rect;
class GameObject;

class Renderer final : public Base
{
private:
	Renderer(const ComPtr<Device>& device, const ComPtr<Context>& context);

public:
	virtual ~Renderer() = default;

public:
	HRESULT Initialize();
	void Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject);
	void Draw();

private:
	void Render_Priority();
    void Render_Shadow();
	void Render_NonBlend();
    void Render_Lights();
    void Render_NonLight();
	void Render_Blend();
	void Render_UI();

    void Render_Combined();

    HRESULT Ready_RenderTargets();

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;

    /* Layer가 소유하는 GameObject를 렌더 프레임 동안만 참조하기 위한 약한 참조 목록. */
	list<Weak<GameObject>>		_renderGroups[ETOI(RenderGroup::END)];

    // shadow
    ComPtr<Texture2D>               _shadowDepthTexture;
    ComPtr<DepthStencilView>        _shadowDepthDSV;
    D3D11_VIEWPORT                  _shadowViewport = {};

    Shared<Shader>                  _deferredShader;
    Shared<VIBuffer_Rect>           _viBuffer;

    float                           _shadowScale = 2.f;
    bool                            _shadowEnabled = false;
    uint32                          _shadowMapSize = 0;

    Matrix                          _deferredWorldMatrix;
    Matrix                          _deferredViewMatrix;
    Matrix                          _deferredProjMatrix;

public:
	static Unique<Renderer> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;

#ifdef _DEBUG

public :
    void Add_DebugRenderGroup(Shared<Component> debugComponent);

private:
    void Render_Debug();

private :
    list<shared_ptr<Component>>     _debugComponents;

#endif
};

NS_END


