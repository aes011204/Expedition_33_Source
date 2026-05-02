#include "Layer.h"
#include "GameObject.h"

Layer::Layer()
{
}

HRESULT Layer::Add_GameObject(Shared<GameObject> gameObject)
{
	if (nullptr == gameObject)
	{
		MSG_BOX("Failed to Add to Layer : GameObject");
		return E_FAIL;
	}

	_gameObjects.push_back(gameObject);
	return S_OK;
}

void Layer::Priority_Update(float timeDelta)
{
	for (auto& gameObject : _gameObjects)
	{
	     if (nullptr == gameObject)
            continue;

        // 초기 1회 BeginPlay 실행
        gameObject->Try_BeginPlay();
        gameObject->Priority_Update(timeDelta);
	}
		
}

void Layer::Update(float timeDelta)
{
	for (auto& gameObject : _gameObjects)
		if (nullptr != gameObject)
			gameObject->Update(timeDelta);
}

void Layer::Late_Update(float timeDelta)
{
	for (auto& gameObject : _gameObjects)
		if (nullptr != gameObject)
			gameObject->Late_Update(timeDelta);
}

Shared<GameObject> Layer::Find_FirstObject()
{
    if (_gameObjects.empty())
        return nullptr;

    return _gameObjects.front();
}

Shared<Layer> Layer::Create()
{
	return make_shared<Layer>();
}

void Layer::Free()
{
	__super::Free();
	_gameObjects.clear();
}

