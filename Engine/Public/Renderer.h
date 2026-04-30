#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class GameObject;

class Renderer final : public Base
{
private:
	Renderer(ComPtr<Device> device, ComPtr<Context> context);

public:
	virtual ~Renderer() = default;

public:
	HRESULT Initialize();
	void Add_RenderGroup(RenderGroup eRenderGroup, GameObject* pGameObject);
	void Draw();

private:
	ComPtr<Device>        _device;
	ComPtr<Context> _context;

    /* Layer가 소유하는 GameObject를 렌더 프레임 동안만 참조하기 위한 약한 참조 목록. */
	list<Weak<GameObject>>		_renderGroups[ETOI(RenderGroup::END)];

private:
	void Render_Priority();
	void Render_NonBlend();
	void Render_Blend();
	void Render_UI();


public:
	static Unique<Renderer> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual void Free() override;
};

NS_END


