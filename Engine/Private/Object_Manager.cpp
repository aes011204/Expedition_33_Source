#include "Object_Manager.h"

#include "Debug_Profile.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Layer.h"

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
namespace
{
string Narrow_Profile_Tag(const wstring& tag)
{
    string result{};
    result.reserve(tag.size());

    for (const wchar_t ch : tag)
        result.push_back(ch <= 0x7f ? static_cast<char>(ch) : '?');

    return result;
}

string Make_Layer_Profile_Name(const char* phaseName, size_t levelIndex, const wstring& layerTag)
{
    return string{ "Object::" } + phaseName
        + "::L" + to_string(levelIndex)
        + "::" + Narrow_Profile_Tag(layerTag);
}
}
#endif

namespace
{
bool Is_PostUpdateLayer(const wstring& layerTag)
{
    // Effect는 owner/socket transform 갱신 이후 tick되어야 한다.
    return layerTag == TEXT("Layer_Effect");
}

bool Should_UpdateLevel(size_t levelIndex)
{
    constexpr size_t staticLevelIndex = 0;
    return levelIndex == staticLevelIndex ||
        levelIndex == GAME->Current_LevelIndex();
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

    // 중복등록 방지
    if (gameObject->Is_MarkDestroy())
        return true;

    gameObject->Set_MarkDestroy(true);

    PendingRemoveGameObject pending{};
    pending.levelIndex = levelIndex;
    pending.gameObject = gameObject;
    _pendingRemoveGameObjects.push_back(pending);

    return true;
}

void Object_Manager::Priority_Update(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_BASIC("Object::PriorityUpdateAll");

    for (size_t levelIndex = 0; levelIndex < _layers.size(); ++levelIndex)
    {
        if (!Should_UpdateLevel(levelIndex))
            continue;

        auto& layers = _layers[levelIndex];
        for (auto& Pair : layers)
        {
            if (Is_PostUpdateLayer(Pair.first))
                continue;

            if (nullptr != Pair.second)
            {
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
                const string profileName = Make_Layer_Profile_Name("Priority", levelIndex, Pair.first);
                ENGINE_PROFILE_SCOPE_DETAIL(profileName.c_str());
#endif
                Pair.second->Priority_Update(timeDelta);
            }
        }

        for (auto& Pair : layers)
        {
            if (!Is_PostUpdateLayer(Pair.first))
                continue;

            if (nullptr != Pair.second)
            {
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
                const string profileName = Make_Layer_Profile_Name("Priority", levelIndex, Pair.first);
                ENGINE_PROFILE_SCOPE_DETAIL(profileName.c_str());
#endif
                // owner Priority_Update가 끝난 뒤 effect Priority_Update를 수행한다.
                Pair.second->Priority_Update(timeDelta);
            }
        }
    }
}

void Object_Manager::Update(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_BASIC("Object::UpdateAll");

    for (size_t levelIndex = 0; levelIndex < _layers.size(); ++levelIndex)
    {
        if (!Should_UpdateLevel(levelIndex))
            continue;

        auto& layers = _layers[levelIndex];
        for (auto& Pair : layers)
        {
            if (Is_PostUpdateLayer(Pair.first))
                continue;

            if (nullptr != Pair.second)
            {
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
                const string profileName = Make_Layer_Profile_Name("Update", levelIndex, Pair.first);
                ENGINE_PROFILE_SCOPE_DETAIL(profileName.c_str());
#endif
                Pair.second->Update(timeDelta);
            }
        }

        for (auto& Pair : layers)
        {
            if (!Is_PostUpdateLayer(Pair.first))
                continue;

            if (nullptr != Pair.second)
            {
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
                const string profileName = Make_Layer_Profile_Name("Update", levelIndex, Pair.first);
                ENGINE_PROFILE_SCOPE_DETAIL(profileName.c_str());
#endif
                // owner update가 끝난 뒤 effect compute/update를 수행한다.
                Pair.second->Update(timeDelta);
            }
        }
    }
}

void Object_Manager::Late_Update(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_BASIC("Object::LateUpdateAll");

    for (size_t levelIndex = 0; levelIndex < _layers.size(); ++levelIndex)
    {
        if (!Should_UpdateLevel(levelIndex))
            continue;

        auto& layers = _layers[levelIndex];
        for (auto& Pair : layers)
        {
            if (Is_PostUpdateLayer(Pair.first))
                continue;

            if (nullptr != Pair.second)
            {
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
                const string profileName = Make_Layer_Profile_Name("Late", levelIndex, Pair.first);
                ENGINE_PROFILE_SCOPE_DETAIL(profileName.c_str());
#endif
                Pair.second->Late_Update(timeDelta);
            }
        }

        for (auto& Pair : layers)
        {
            if (!Is_PostUpdateLayer(Pair.first))
                continue;

            if (nullptr != Pair.second)
            {
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
                const string profileName = Make_Layer_Profile_Name("Late", levelIndex, Pair.first);
                ENGINE_PROFILE_SCOPE_DETAIL(profileName.c_str());
#endif
                // owner Late_Update 이후 최종 위치 기준으로 effect render 등록을 수행한다.
                Pair.second->Late_Update(timeDelta);
            }
        }
    }
}

HRESULT Object_Manager::Clear_Layers(uint32 levelIndex)
{
    if (levelIndex >= _numLevels)
        return E_FAIL;

    for (auto& [layerTag, layer] : _layers[levelIndex])
    {
        UNREFERENCED_PARAMETER(layerTag);

        if (layer)
            layer->Free();
    }

    _pendingRemoveGameObjects.erase(
        remove_if(
            _pendingRemoveGameObjects.begin(),
            _pendingRemoveGameObjects.end(),
            [levelIndex](const PendingRemoveGameObject& pending)
            {
                return pending.levelIndex == levelIndex;
            }),
        _pendingRemoveGameObjects.end());

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

void Object_Manager::Flush_RemovedGameObjects()
{
    if (_pendingRemoveGameObjects.empty())
        return;

    for (const auto& pending : _pendingRemoveGameObjects)
    {
        if (pending.gameObject == nullptr || pending.levelIndex >= _numLevels)
            continue;

        auto& layers = _layers[pending.levelIndex];
        for (auto iter = layers.begin(); iter != layers.end(); )
        {
            Shared<Layer>& layer = iter->second;
            bool removed = false;

            if (layer != nullptr)
                removed = layer->Remove_GameObject(pending.gameObject);

            if (layer != nullptr && layer->Get_GameObjects().empty())
                iter = layers.erase(iter);
            else
                ++iter;

            if (removed)
            {
                ++_objectRevision;
                break;
            }
        }
    }

    _pendingRemoveGameObjects.clear();
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
    for (auto& levelLayers : _layers)
    {
        for (auto& [layerTag, layer] : levelLayers)
        {
            UNREFERENCED_PARAMETER(layerTag);

            if (layer)
                layer->Free();
        }
    }

    __super::Free();
    _layers.clear();
}



