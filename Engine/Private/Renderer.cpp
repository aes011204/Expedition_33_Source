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

void Renderer::Add_RenderGroup(RenderGroup eRenderGroup, GameObject* pGameObject)
{
	if (nullptr == pGameObject ||
		eRenderGroup >= RenderGroup::END)
	{
		MSG_BOX("Failed to Add : Renderer");
		return;
	}

	_renderGroups[ETOI(eRenderGroup)].push_back(pGameObject->GetSharedPtr<GameObject>());

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
	for (auto& pRenderObject : _renderGroups[ETOI(RenderGroup::PRIORITY)])
	{
		Shared<GameObject> pGameObject = pRenderObject.lock();
		if (nullptr != pGameObject)
			pGameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::PRIORITY)].clear();
}

void Renderer::Render_NonBlend()
{
	for (auto& pRenderObject : _renderGroups[ETOI(RenderGroup::NONBLEND)])
	{
		Shared<GameObject> pGameObject = pRenderObject.lock();
		if (nullptr != pGameObject)
			pGameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::NONBLEND)].clear();
}

void Renderer::Render_Blend()
{
	for (auto& pRenderObject : _renderGroups[ETOI(RenderGroup::BLEND)])
	{
		Shared<GameObject> pGameObject = pRenderObject.lock();
		if (nullptr != pGameObject)
			pGameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::BLEND)].clear();
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





