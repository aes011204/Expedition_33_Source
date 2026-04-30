#include "Layer.h"
#include "GameObject.h"

Layer::Layer()
{
}

HRESULT Layer::Add_GameObject(Shared<GameObject> pGameObject)
{
	if (nullptr == pGameObject)
	{
		MSG_BOX("Failed to Add to Layer : GameObject");
		return E_FAIL;
	}

	_gameObjects.push_back(pGameObject);
	return S_OK;
}

void Layer::Priority_Update(float fTimeDelta)
{
	for (auto& pGameObject : _gameObjects)
		if (nullptr != pGameObject)
			pGameObject->Priority_Update(fTimeDelta);
}

void Layer::Update(float fTimeDelta)
{
	for (auto& pGameObject : _gameObjects)
		if (nullptr != pGameObject)
			pGameObject->Update(fTimeDelta);
}

void Layer::Late_Update(float fTimeDelta)
{
	for (auto& pGameObject : _gameObjects)
		if (nullptr != pGameObject)
			pGameObject->Late_Update(fTimeDelta);
}

Shared<Layer> Layer::Create()
{
	return MakeSharedFromRaw(new Layer());
}

void Layer::Free()
{
	__super::Free();
	_gameObjects.clear();
}

