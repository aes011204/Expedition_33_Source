#include "Object_Manager.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Layer.h"

Object_Manager::Object_Manager()
{
}

HRESULT Object_Manager::Initialize(uint32 iNumLevels)
{
	_numLevels = iNumLevels;
	_layers.resize(iNumLevels);
	return S_OK;
}

HRESULT Object_Manager::Add_GameObject(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, uint32 iLayerLevelIndex, const wstring& strLayerTag, void* pArg)
{
	if (iLayerLevelIndex >= _numLevels)
	{
		MSG_BOX("Failed To Added : CloneGameObject");
		return E_FAIL;
	}

	Shared<GameObject> pGameObject = static_pointer_cast<GameObject>(GAME->Clone_Prototype(Prototype::GameObject, iPrototypeLevelIndex, strPrototypeTag, pArg));
	if (nullptr == pGameObject)
	{
		MSG_BOX("Failed To Added : CloneGameObject");
		return E_FAIL;
	}

	Shared<Layer> pLayer = Find_Layer(iLayerLevelIndex, strLayerTag);
	if (nullptr == pLayer)
	{
		pLayer = Layer::Create();
		pLayer->Add_GameObject(pGameObject);
		_layers[iLayerLevelIndex].emplace(strLayerTag, pLayer);
	}
	else
	{
		pLayer->Add_GameObject(pGameObject);
	}

	return S_OK;
}

void Object_Manager::Priority_Update(float fTimeDelta)
{
	for (auto& layers : _layers)
		for (auto& Pair : layers)
			if (nullptr != Pair.second)
				Pair.second->Priority_Update(fTimeDelta);
}

void Object_Manager::Update(float fTimeDelta)
{
	for (auto& layers : _layers)
		for (auto& Pair : layers)
			if (nullptr != Pair.second)
				Pair.second->Update(fTimeDelta);
}

void Object_Manager::Late_Update(float fTimeDelta)
{
	for (auto& layers : _layers)
		for (auto& Pair : layers)
			if (nullptr != Pair.second)
				Pair.second->Late_Update(fTimeDelta);
}

HRESULT Object_Manager::Clear_Layers(uint32 iLevelIndex)
{
	if (iLevelIndex >= _numLevels)
		return E_FAIL;

	_layers[iLevelIndex].clear();
	return S_OK;
}

Shared<Layer> Object_Manager::Find_Layer(uint32 iLayerLevelIndex, const wstring& strLayerTag)
{
	auto iter = _layers[iLayerLevelIndex].find(strLayerTag);
	if (iter == _layers[iLayerLevelIndex].end())
		return nullptr;

	return iter->second;
}

Unique<Object_Manager> Object_Manager::Create(uint32 iNumLevels)
{
	Unique<Object_Manager> pInstance{ new Object_Manager() };
	if (FAILED(pInstance->Initialize(iNumLevels)))
	{
		MSG_BOX("Failed to Created : Object_Manager");
		pInstance->Free();
		return nullptr;
	}
	return pInstance;
}

void Object_Manager::Free()
{
	__super::Free();
	_layers.clear();
}



