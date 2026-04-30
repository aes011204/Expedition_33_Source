#include "Prototype_Manager.h"

#include "Component.h"
#include "GameObject.h"

Prototype_Manager::Prototype_Manager()
{
}

HRESULT Prototype_Manager::Initialize(uint32 iNumLevels)
{
	_numLevels = iNumLevels;
	_prototypes.resize(iNumLevels);
	return S_OK;
}

HRESULT Prototype_Manager::Add_Prototype(uint32 iLevelIndex, const wstring& strPrototypeTag, Shared<Base> pPrototype)
{
	if (nullptr != Find_Prototype(iLevelIndex, strPrototypeTag))
	{
		MSG_BOX("An object with the same key already exists.");
		return E_FAIL;
	}

	_prototypes[iLevelIndex].emplace(strPrototypeTag, pPrototype);
	return S_OK;
}

Shared<Base> Prototype_Manager::Clone_Prototype(Prototype ePrototype, uint32 iLevelIndex, const wstring& strPrototypeTag, void* pArg)
{
	Shared<Base> pPrototype = Find_Prototype(iLevelIndex, strPrototypeTag);
	if (nullptr == pPrototype)
		return nullptr;

	Shared<Base> pInstance = nullptr;
	if (Prototype::COMPONENT == ePrototype)
		pInstance = static_pointer_cast<Component>(pPrototype)->Clone(pArg);
	else
		pInstance = static_pointer_cast<GameObject>(pPrototype)->Clone(pArg);

	if (nullptr == pInstance)
		MSG_BOX("Failed to Cloned");

	return pInstance;
}

HRESULT Prototype_Manager::Clear_Prototypes(uint32 iLevelIndex)
{
	if (iLevelIndex >= _numLevels)
		return E_FAIL;

	_prototypes[iLevelIndex].clear();
	return S_OK;
}

Shared<Base> Prototype_Manager::Find_Prototype(uint32 iLevelIndex, const wstring& strPrototypeTag)
{
	if (iLevelIndex >= _numLevels)
		return nullptr;

	auto iter = _prototypes[iLevelIndex].find(strPrototypeTag);
	if (iter == _prototypes[iLevelIndex].end())
		return nullptr;

	return iter->second;
}

Unique<Prototype_Manager> Prototype_Manager::Create(uint32 iNumLevels)
{
	Unique<Prototype_Manager> pInstance{ new Prototype_Manager() };
	if (FAILED(pInstance->Initialize(iNumLevels)))
	{
		MSG_BOX("Failed to Created : Prototype_Manager");
		pInstance->Free();
		return nullptr;
	}
	return pInstance;
}

void Prototype_Manager::Free()
{
	__super::Free();
	_prototypes.clear();
}

