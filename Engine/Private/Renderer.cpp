#include "Renderer.h"

#include "GameInstance.h"
#include "GameObject.h"

Renderer::Renderer(ComPtr<Device> device, ComPtr<Context> context)
    : _device { device }
	, _context { context }
{
}

HRESULT Renderer::Initialize()
{
	return S_OK;
}

void Renderer::Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject)
{
    CHECK_NULL(gameObject);

    if (renderGroup < RenderGroup::Priority || renderGroup >= RenderGroup::END)
        return;

    _renderGroups[ETOI(renderGroup)].push_back(gameObject);
}

void Renderer::Draw()
{
	Render_Priority();

	Render_NonBlend();

	Render_Blend();

	Render_UI();
}

void Renderer::Render_Priority()
{
	for (auto& pRenderObject : _renderGroups[ETOI(RenderGroup::Priority)])
	{
		Shared<GameObject> pGameObject = pRenderObject.lock();
		if (nullptr != pGameObject)
			pGameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::Priority)].clear();
}

void Renderer::Render_NonBlend()
{
	for (auto& pRenderObject : _renderGroups[ETOI(RenderGroup::NonBlend)])
	{
		Shared<GameObject> pGameObject = pRenderObject.lock();
		if (nullptr != pGameObject)
			pGameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::NonBlend)].clear();
}

void Renderer::Render_Blend()
{
	for (auto& pRenderObject : _renderGroups[ETOI(RenderGroup::Blend)])
	{
		Shared<GameObject> pGameObject = pRenderObject.lock();
		if (nullptr != pGameObject)
			pGameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::Blend)].clear();
}

void Renderer::Render_UI()
{
	for (auto& pRenderObject : _renderGroups[ETOI(RenderGroup::UI)])
	{
		Shared<GameObject> pGameObject = pRenderObject.lock();
		if (nullptr != pGameObject)
			pGameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::UI)].clear();
}

Unique<Renderer> Renderer::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Unique<Renderer> pInstance{ new Renderer(device, context) };

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : Renderer");
		pInstance->Free();
		return nullptr;
	}
	return pInstance;
}

void Renderer::Free()
{
	__super::Free();

	for (size_t i = 0; i < ETOI(RenderGroup::END); i++)
	{

		_renderGroups[i].clear();
	}
}





