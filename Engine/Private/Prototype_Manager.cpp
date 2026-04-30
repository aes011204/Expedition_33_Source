#include "Prototype_Manager.h"

#include "Component.h"
#include "GameObject.h"

Prototype_Manager::Prototype_Manager()
{
}

HRESULT Prototype_Manager::Initialize(uint32 numLevels)
{
	_numLevels = numLevels;
	_prototypes.resize(numLevels);
	return S_OK;
}

HRESULT Prototype_Manager::Add_Prototype(uint32 levelIndex, const wstring& prototypeTag, Shared<Base> prototype)
{
	if (nullptr != Find_Prototype(levelIndex, prototypeTag))
	{
		wstring message = L"Prototype already exists.\nLevel: " + to_wstring(levelIndex) + L"\nTag: " + prototypeTag;
		MSG_BOX_S(message.c_str());
		return E_FAIL;
	}

	_prototypes[levelIndex].emplace(prototypeTag, prototype);
	return S_OK;
}

Shared<Base> Prototype_Manager::Clone_Prototype(Prototype prototype, uint32 levelIndex, const wstring& prototypeTag, void* arg)
{
	Shared<Base> foundPrototype = Find_Prototype(levelIndex, prototypeTag);
	if (nullptr == foundPrototype)
		return nullptr;

	Shared<Base> instance = nullptr;
	if (Prototype::Comopnent == prototype)
		instance = static_pointer_cast<Component>(foundPrototype)->Clone(arg);
	else
		instance = static_pointer_cast<GameObject>(foundPrototype)->Clone(arg);

	if (nullptr == instance)
		MSG_BOX("Failed to Cloned");

	return instance;
}

HRESULT Prototype_Manager::Clear_Prototypes(uint32 levelIndex)
{
	if (levelIndex >= _numLevels)
		return E_FAIL;

	_prototypes[levelIndex].clear();
	return S_OK;
}

Shared<Base> Prototype_Manager::Find_Prototype(uint32 levelIndex, const wstring& prototypeTag)
{
	if (levelIndex >= _numLevels)
		return nullptr;

	auto iter = _prototypes[levelIndex].find(prototypeTag);
	if (iter == _prototypes[levelIndex].end())
		return nullptr;

	return iter->second;
}

Unique<Prototype_Manager> Prototype_Manager::Create(uint32 numLevels)
{
	Unique<Prototype_Manager> instance{ new Prototype_Manager() };
	if (FAILED(instance->Initialize(numLevels)))
	{
		MSG_BOX("Failed to Created : Prototype_Manager");
		instance->Free();
		return nullptr;
	}
	return instance;
}

void Prototype_Manager::Free()
{
	__super::Free();
	_prototypes.clear();
}

