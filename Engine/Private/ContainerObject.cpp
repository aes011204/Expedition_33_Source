#include "ContainerObject.h"
#include "GameInstance.h"

#include "PartObject.h"

namespace
{
#ifdef _DEBUG
struct ContainerPartUpdatePerfStats
{
    uint64 samples = 0;
    double accumulatedMs = 0.0;
    double maxMs = 0.0;
};

map<string, ContainerPartUpdatePerfStats> g_containerPartUpdatePerfStats;
chrono::steady_clock::time_point g_containerPartUpdatePerfWindowBegin{};
bool g_hasContainerPartUpdatePerfWindow = false;

string Resolve_DebugClassName(GameObject& gameObject)
{
    ClassReflectionInfo& reflectionInfo = gameObject.Get_ReflectionInfo();
    if (!reflectionInfo.className.empty())
        return reflectionInfo.className;

    return "<unreflected>";
}

string Resolve_DebugPartTag(const ContainerObject& container, const Shared<PartObject>& partObject)
{
    if (partObject == nullptr)
        return "<null>";

    for (const auto& [partTag, registeredPart] : container.Get_PartObjects())
    {
        if (registeredPart == partObject)
            return String::ToString(partTag);
    }

    return "<unregistered>";
}

string Make_ContainerPartPerfKey(ContainerObject& container, const Shared<PartObject>& partObject)
{
    string key = fmt::format(
        "owner={} owner_prefab={} part={}",
        Resolve_DebugClassName(container),
        container.Get_SourcePrefabName().empty() ? "<none>" : container.Get_SourcePrefabName(),
        Resolve_DebugPartTag(container, partObject));

    if (partObject != nullptr)
    {
        key += fmt::format(
            " part_class={} part_prefab={}",
            Resolve_DebugClassName(*partObject),
            partObject->Get_SourcePrefabName().empty() ? "<none>" : partObject->Get_SourcePrefabName());
    }

    return key;
}

void Accumulate_ContainerPartUpdatePerf(const string& key, double elapsedMs)
{
    if (!g_hasContainerPartUpdatePerfWindow)
    {
        g_containerPartUpdatePerfWindowBegin = chrono::steady_clock::now();
        g_hasContainerPartUpdatePerfWindow = true;
    }

    ContainerPartUpdatePerfStats& stats = g_containerPartUpdatePerfStats[key];
    ++stats.samples;
    stats.accumulatedMs += elapsedMs;
    stats.maxMs = max(stats.maxMs, elapsedMs);
}

void Try_Log_ContainerPartUpdatePerf()
{
    if (!g_hasContainerPartUpdatePerfWindow)
        return;

    const chrono::steady_clock::time_point now = chrono::steady_clock::now();
    const double windowSec = chrono::duration<double>(now - g_containerPartUpdatePerfWindowBegin).count();
    if (windowSec < 5.0)
        return;

    const auto topIter = max_element(
        g_containerPartUpdatePerfStats.begin(),
        g_containerPartUpdatePerfStats.end(),
        [](const auto& lhs, const auto& rhs)
        {
            return lhs.second.accumulatedMs < rhs.second.accumulatedMs;
        });

    if (topIter != g_containerPartUpdatePerfStats.end() && topIter->second.samples > 0)
    {
        const ContainerPartUpdatePerfStats& stats = topIter->second;
        const double avgMs = stats.accumulatedMs / static_cast<double>(stats.samples);

        GAME->Log_FileOnly(
            spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION },
            spdlog::level::trace,
            fmt::format(
                "[ContainerPartUpdatePerf] window_sec={:.2f} top=\"{}\" avg_ms={:.3f} max_ms={:.3f} total_ms={:.3f} samples={}",
                windowSec,
                topIter->first,
                avgMs,
                stats.maxMs,
                stats.accumulatedMs,
                stats.samples));
    }

    g_containerPartUpdatePerfStats.clear();
    g_containerPartUpdatePerfWindowBegin = now;
}
#endif
}

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
        const json& partObjectJson = partJson["object"];

        if (!Should_ApplySerializedPart(partTag, partObjectJson))
            continue;

        PartObject* partObject = Get_PartObject(partTag);
        if (partObject == nullptr)
            continue;

        partObject->From_Json(partObjectJson);
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
    {
#ifdef _DEBUG
        const string perfKey = Make_ContainerPartPerfKey(*this, partObject);
        const chrono::steady_clock::time_point updateBegin = chrono::steady_clock::now();
#endif

        partObject->Update(timeDelta);

#ifdef _DEBUG
        const double elapsedMs = chrono::duration<double, milli>(chrono::steady_clock::now() - updateBegin).count();
        Accumulate_ContainerPartUpdatePerf(perfKey, elapsedMs);
#endif
    }

#ifdef _DEBUG
    Try_Log_ContainerPartUpdatePerf();
#endif
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

