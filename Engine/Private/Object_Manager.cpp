#include "Object_Manager.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Layer.h"

Object_Manager::Object_Manager()
{
}

HRESULT Object_Manager::Initialize(uint32 numLevels)
{
	_numLevels = numLevels;
	_layers.resize(numLevels);
	return S_OK;
}

HRESULT Object_Manager::Add_GameObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, uint32 layerLevelIndex, const wstring& layerTag, void* arg)
{
	if (layerLevelIndex >= _numLevels)
	{
		MSG_BOX("Failed To Added : CloneGameObject");
		return E_FAIL;
	}

	Shared<GameObject> gameObject = static_pointer_cast<GameObject>(GAME->Clone_Prototype(Prototype::GameObject, prototypeLevelIndex, prototypeTag, arg));
	if (nullptr == gameObject)
	{
		MSG_BOX("Failed To Added : CloneGameObject");
		return E_FAIL;
	}

	Shared<Layer> layer = Find_Layer(layerLevelIndex, layerTag);
	if (nullptr == layer)
	{
		layer = Layer::Create();
		layer->Add_GameObject(gameObject);
		_layers[layerLevelIndex].emplace(layerTag, layer);
	}
	else
	{
		layer->Add_GameObject(gameObject);
	}

	return S_OK;
}

void Object_Manager::Priority_Update(float timeDelta)
{
	for (auto& layers : _layers)
		for (auto& Pair : layers)
			if (nullptr != Pair.second)
				Pair.second->Priority_Update(timeDelta);
}

void Object_Manager::Update(float timeDelta)
{
	for (auto& layers : _layers)
		for (auto& Pair : layers)
			if (nullptr != Pair.second)
				Pair.second->Update(timeDelta);
}

void Object_Manager::Late_Update(float timeDelta)
{
	for (auto& layers : _layers)
		for (auto& Pair : layers)
			if (nullptr != Pair.second)
				Pair.second->Late_Update(timeDelta);
}

HRESULT Object_Manager::Clear_Layers(uint32 levelIndex)
{
	if (levelIndex >= _numLevels)
		return E_FAIL;

	_layers[levelIndex].clear();
	return S_OK;
}

Shared<GameObject> Object_Manager::Find_FirstObject(uint32 levelIndex, const wstring& layerTag)
{
    if (levelIndex >= _numLevels)
        return nullptr;

    Shared<Layer> layer = Find_Layer(levelIndex, layerTag);
    if (layer == nullptr)
        return nullptr;

    return layer->Find_FirstObject();
}

Shared<Layer> Object_Manager::Find_Layer(uint32 layerLevelIndex, const wstring& layerTag)
{
	auto iter = _layers[layerLevelIndex].find(layerTag);
	if (iter == _layers[layerLevelIndex].end())
		return nullptr;

	return iter->second;
}

Unique<Object_Manager> Object_Manager::Create(uint32 numLevels)
{
	Unique<Object_Manager> instance{ new Object_Manager() };
	if (FAILED(instance->Initialize(numLevels)))
	{
		MSG_BOX("Failed to Created : Object_Manager");
		instance->Free();
		return nullptr;
	}
	return instance;
}

void Object_Manager::Free()
{
	__super::Free();
	_layers.clear();
}



