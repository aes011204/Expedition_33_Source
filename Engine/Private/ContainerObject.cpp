#include "ContainerObject.h"
#include "GameInstance.h"

#include "PartObject.h"

ContainerObject::ContainerObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: GameObject { device, context }
{
}

ContainerObject::ContainerObject(const ContainerObject& prototype)
	: GameObject { prototype }
{
}

HRESULT ContainerObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT ContainerObject::Initialize(void* arg)
{
	if (FAILED(__super::Initialize(arg)))
		return E_FAIL;

	return S_OK;
}

void ContainerObject::BeginPlay()
{
    GameObject::BeginPlay();

    for (auto& [_, partObject] : _partObjects)
    {
        if (partObject == nullptr)
            continue;

        partObject->BeginPlay();
    }
}

void ContainerObject::Priority_Update(float timeDelta)
{
	for (auto& Pair : _partObjects)
		Pair.second->Priority_Update(timeDelta);
}

void ContainerObject::Update(float timeDelta)
{
	for (auto& Pair : _partObjects)
		Pair.second->Update(timeDelta);
}

void ContainerObject::Late_Update(float timeDelta)
{
	for (auto& Pair : _partObjects)
		Pair.second->Late_Update(timeDelta);
}

HRESULT ContainerObject::Render()
{
	return S_OK;
}


HRESULT ContainerObject::Add_PartObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, const wstring& partTag, void* arg)
{
	if (nullptr != Find_PartObject(partTag))
		return E_FAIL;

	Shared<PartObject> partObject = static_pointer_cast<PartObject>(GAME->Clone_Prototype(Prototype::GameObject, prototypeLevelIndex, prototypeTag, arg));
	if (nullptr == partObject)
		return E_FAIL;

    partObject->Set_Owner(this->GetSharedPtr<GameObject>());

	_partObjects.emplace(partTag, partObject);	

	return S_OK; 
}

PartObject* ContainerObject::Find_PartObject(const wstring& partTag)
{
	auto	iter = _partObjects.find(partTag);

	if(iter == _partObjects.end())
		return nullptr;

	return iter->second.get();
}

PartObject* ContainerObject::Get_PartObject(const wstring& partTag) const
{
    auto iter = _partObjects.find(partTag);
    if (iter == _partObjects.end())
        return nullptr;

    return iter->second.get();
}

void ContainerObject::Free()
{
	__super::Free();

	_partObjects.clear();
}

