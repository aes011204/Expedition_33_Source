#include "ContainerObject.h"
#include "GameInstance.h"

#include "PartObject.h"

ContainerObject::ContainerObject(ComPtr<Device> device, ComPtr<Context> context)
	: GameObject { device, context }
{
}

ContainerObject::ContainerObject(const ContainerObject& Prototype)
	: GameObject { Prototype }
{
}

HRESULT ContainerObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT ContainerObject::Initialize(void* pArg)
{
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	return S_OK;
}

void ContainerObject::Priority_Update(float fTimeDelta)
{
	for (auto& Pair : _partObjects)
		Pair.second->Priority_Update(fTimeDelta);
}

void ContainerObject::Update(float fTimeDelta)
{
	for (auto& Pair : _partObjects)
		Pair.second->Update(fTimeDelta);
}

void ContainerObject::Late_Update(float fTimeDelta)
{
	for (auto& Pair : _partObjects)
		Pair.second->Late_Update(fTimeDelta);
}

HRESULT ContainerObject::Render()
{
	return S_OK;
}


HRESULT ContainerObject::Add_PartObject(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, const wstring& strPartTag, void* pArg)
{
	if (nullptr != Find_PartObject(strPartTag))
		return E_FAIL;

	Shared<PartObject> pPartObject = static_pointer_cast<PartObject>(GAME->Clone_Prototype(Prototype::GAMEOBJECT, iPrototypeLevelIndex, strPrototypeTag, pArg));
	if (nullptr == pPartObject)
		return E_FAIL;

	_partObjects.emplace(strPartTag, pPartObject);	

	return S_OK; 
}

PartObject* ContainerObject::Find_PartObject(const wstring& strPartTag)
{
	auto	iter = _partObjects.find(strPartTag);

	if(iter == _partObjects.end())
		return nullptr;

	return iter->second.get();
}

void ContainerObject::Free()
{
	__super::Free();

	for (auto& Pair : _partObjects)
		Safe_Release(Pair.second);

	_partObjects.clear();
}

