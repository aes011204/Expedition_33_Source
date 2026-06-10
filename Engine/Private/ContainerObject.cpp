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

    for (auto& partObject : _partObjectUpdateOrder)
    {
        if (partObject)
            partObject->BeginPlay();
    }
}

json ContainerObject::To_Json() const
{
    json root = GameObject::To_Json();
    json parts = json::array();

    for (const auto& [partTag, partObject] : _partObjects)
    {
        if (partObject == nullptr)
            continue;

        json partJson = json::object();
        partJson["part_tag"] = String::ToString(partTag);
        partJson["object"] = partObject->To_Json();

        parts.emplace_back(partJson);
    }

    root["parts"] = parts;

    return root;
}

void ContainerObject::From_Json(const json& data)
{
    GameObject::From_Json(data);

    if (!data.contains("parts") || !data["parts"].is_array())
        return;

    for (const json& partJson : data["parts"])
    {
        if (!partJson.is_object())
            continue;

        if (!partJson.contains("part_tag") || !partJson.contains("object"))
            continue;

        const string partTagString = partJson["part_tag"].get<string>();
        const wstring partTag = String::ToWString(partTagString);

        PartObject* partObject = Get_PartObject(partTag);
        if (partObject == nullptr)
            continue;

        partObject->From_Json(partJson["object"]);
    }
}

void ContainerObject::Priority_Update(float timeDelta)
{
	for (auto& partObject : _partObjectUpdateOrder)
        partObject->Priority_Update(timeDelta);
}

void ContainerObject::Update(float timeDelta)
{
	for (auto& partObject : _partObjectUpdateOrder)
        partObject->Update(timeDelta);
}

void ContainerObject::Late_Update(float timeDelta)
{
	for (auto& partObject : _partObjectUpdateOrder)
        partObject->Late_Update(timeDelta);
}

HRESULT ContainerObject::Render()
{
	return S_OK;
}


HRESULT ContainerObject::Add_PartObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, const wstring& partTag, void* arg)
{
	if (nullptr != Find_PartObject(partTag))
		return E_FAIL;

	Shared<PartObject> partObject = static_pointer_cast<PartObject>(
        GAME->Clone_Prototype(Prototype::GameObject, prototypeLevelIndex, prototypeTag, arg));

	if (nullptr == partObject)
		return E_FAIL;

    partObject->Set_Owner(this->GetSharedPtr<GameObject>());

	_partObjects.emplace(partTag, partObject);
    _partObjectUpdateOrder.push_back(partObject);

    if (_hasBegunPlay)
        CHECK_FAILED(partObject->Try_BeginPlay(), E_FAIL);

	return S_OK; 
}

PartObject* ContainerObject::Find_PartObject(const wstring& partTag)
{
	auto	iter = _partObjects.find(partTag);

	if(iter == _partObjects.end())
		return nullptr;

	return iter->second.get();
}

bool ContainerObject::Remove_PartObject(const wstring& partTag)
{
    auto iter = _partObjects.find(partTag);
    if (iter == _partObjects.end())
        return false;

    Shared<PartObject> removedPart = iter->second;
    _partObjects.erase(iter);

    for (auto orderIter = _partObjectUpdateOrder.begin(); orderIter != _partObjectUpdateOrder.end();)
    {
        if (*orderIter == nullptr || *orderIter == removedPart)
        {
            orderIter = _partObjectUpdateOrder.erase(orderIter);
        }
        else
        {
            ++orderIter;
        }
    }

    if (removedPart)
    {
        removedPart->Free();
    }

    return true;
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
    for (const Shared<PartObject>& partObject : _partObjectUpdateOrder)
    {
        if (partObject)
            partObject->Free();
    }

	__super::Free();

    _partObjectUpdateOrder.clear();
	_partObjects.clear();
}

