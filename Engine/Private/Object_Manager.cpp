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
	Shared<GameObject> gameObject = nullptr;

    return Add_GameObject(prototypeLevelIndex, prototypeTag, layerLevelIndex, layerTag, gameObject, arg);
}

HRESULT Object_Manager::Add_GameObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, uint32 layerLevelIndex,
    const wstring& layerTag, Shared<GameObject>& outGameObject, void* arg)
{
    outGameObject = nullptr;

    if (layerLevelIndex >= _numLevels)
	{
		MSG_BOX("Failed To Added : CloneGameObject");
		return E_FAIL;
	}

	outGameObject = static_pointer_cast<GameObject>(GAME->Clone_Prototype(Prototype::GameObject, prototypeLevelIndex, prototypeTag, arg));
	if (nullptr == outGameObject)
	{
		MSG_BOX("Failed To Added : CloneGameObject");
		return E_FAIL;
	}

	Shared<Layer> layer = Find_Layer(layerLevelIndex, layerTag);
	if (nullptr == layer)
	{
		layer = Layer::Create();
        if (layer == nullptr)
        {
            outGameObject = nullptr;
            return E_FAIL;
        }

		layer->Add_GameObject(outGameObject);
		_layers[layerLevelIndex].emplace(layerTag, layer);
	}
	else
	{
		layer->Add_GameObject(outGameObject);
	}

    ++_objectRevision;
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
    ++_objectRevision;

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

void Object_Manager::Visit_LevelObjects(
    const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const
{
    if (!visitor)
        return;

    const uint32 levelIndex = GAME->Current_LevelIndex();
    if (levelIndex >= _numLevels)
        return;

    const auto& layers = _layers[levelIndex];

    for (const auto& [layerTag, layer] : layers)
    {
        if (!layer)
            continue;

        for (const auto& gameObject : layer->Get_GameObjects())
        {
            if (!gameObject)
                continue;

            visitor(layerTag, gameObject);
        }
    }

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



