#include "Object_Manager.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Layer.h"

#include <chrono>

namespace
{
    double Now_Ms()
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
    }
}

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

HRESULT Object_Manager::Add_GameObject(uint32 layerLevelIndex, const wstring& layerTag,
    const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return E_FAIL;

    if (layerLevelIndex >= _numLevels)
        return E_FAIL;

    Shared<Layer> layer = Find_Layer(layerLevelIndex, layerTag);
    if (layer == nullptr)
    {
        layer = Layer::Create();
        if (layer == nullptr)
            return E_FAIL;

        layer->Add_GameObject(gameObject);
        _layers[layerLevelIndex].emplace(layerTag, layer);
    }
    else
    {
        layer->Add_GameObject(gameObject);
    }

    ++_objectRevision;
    return S_OK;
}

bool Object_Manager::Remove_GameObject(uint32 levelIndex, const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr || levelIndex >= _numLevels)
        return false;

    auto& layers = _layers[levelIndex];
    for (auto iter = layers.begin(); iter != layers.end(); )
    {
        Shared<Layer>& layer = iter->second;
        bool removed = false;

        if (layer != nullptr)
            removed = layer->Remove_GameObject(gameObject);

        if (layer != nullptr && layer->Get_GameObjects().empty())
            iter = layers.erase(iter);
        else
            ++iter;

        if (removed)
        {
            ++_objectRevision;
            return true;
        }
    }

    return false;
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
    const double updateBeginMs = Now_Ms();
    double slowestObjectMs = 0.0;
    uint32 slowestLevelIndex = 0;
    string slowestLayerTag = {};
    string slowestObjectName = {};
    string slowestObjectClass = {};
    size_t slowestLayerObjectCount = 0;

    for (uint32 levelIndex = 0; levelIndex < _layers.size(); ++levelIndex)
    {
        auto& layers = _layers[levelIndex];
        for (auto& Pair : layers)
        {
            const Shared<Layer>& layer = Pair.second;
            if (nullptr == layer)
                continue;

            const auto& gameObjects = layer->Get_GameObjects();
            for (const Shared<GameObject>& gameObject : gameObjects)
            {
                if (gameObject == nullptr)
                    continue;

                const double objectBeginMs = Now_Ms();
                gameObject->Update(timeDelta);
                const double objectMs = Now_Ms() - objectBeginMs;

                if (objectMs > slowestObjectMs)
                {
                    slowestObjectMs = objectMs;
                    slowestLevelIndex = levelIndex;
                    slowestLayerTag = String::ToString(Pair.first);
                    slowestLayerObjectCount = gameObjects.size();
                    slowestObjectClass = gameObject->Get_ReflectionInfo().className;
                    slowestObjectName = gameObject->Get_SourcePrefabName();
                }
            }
        }
    }

    const double updateMs = Now_Ms() - updateBeginMs;
    static double nextLogMs = 0.0;
    const double nowMs = Now_Ms();
    if (updateMs >= 50.0 && nowMs >= nextLogMs)
    {
        LOG_DEBUG(
            "[ObjectManager::SlowUpdate] total={:.2f}ms slowestObject={:.2f}ms level={} layer='{}' layerObjects={} class='{}' prefab='{}'",
            updateMs,
            slowestObjectMs,
            slowestLevelIndex,
            slowestLayerTag,
            slowestLayerObjectCount,
            slowestObjectClass,
            slowestObjectName);
        nextLogMs = nowMs + 1000.0;
    }
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

void Object_Manager::Visit_LevelObjects(uint32 levelIndex,
    const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const
{
    if (!visitor)
        return;

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



