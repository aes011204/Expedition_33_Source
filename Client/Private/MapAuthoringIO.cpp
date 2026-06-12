#include "MapAuthoringIO.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "MapStaticMesh.h"
#include "Monster_WorldBase.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"

#include <fstream>

NS_BEGIN(Client)

namespace
{
    constexpr const char* kPrefabInstancesKey = "PrefabInstances";
    constexpr const char* kAuthoredObjectsKey = "AuthoredObjects";

    string Resolve_StaticClassName(const json& instanceData)
    {
        if (!instanceData.is_object())
            return {};

        return instanceData.value("static_class", string{});
    }

    fs::path Normalize_MapPath(const wstring& filePath)
    {
        std::error_code errorCode{};
        fs::path normalizedPath = fs::absolute(fs::path(filePath), errorCode);
        if (errorCode)
            normalizedPath = fs::path(filePath);

        return normalizedPath.lexically_normal();
    }
}

bool MapAuthoringIO::Is_RuntimeExcludedObject(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return true;

    const ObjectType objectType = gameObject->Get_ObjectType();
    return objectType == ObjectType::FreeCamera ||
        objectType == ObjectType::TargetCamera;
}

bool MapAuthoringIO::Is_PrefabInstanceCandidate(const Shared<GameObject>& gameObject, const wstring& layerTag)
{
    if (Is_RuntimeExcludedObject(gameObject))
        return false;

    if (gameObject == nullptr || gameObject->Get_SourcePrefabName().empty())
        return false;

    if (!gameObject->Is_RuntimeAuthoringLoaded())
        return false;

    if (layerTag == TEXT("Layer_Player"))
        return false;

    return true;
}

bool MapAuthoringIO::Is_AuthoredObjectCandidate(const Shared<GameObject>& gameObject, const wstring& layerTag)
{
    if (Is_RuntimeExcludedObject(gameObject))
        return false;

    if (gameObject == nullptr || !gameObject->Get_SourcePrefabName().empty())
        return false;

    if (!gameObject->Is_RuntimeAuthoringLoaded())
        return false;

    if (layerTag == TEXT("Layer_Player"))
        return false;

    return true;
}

void MapAuthoringIO::Collect_PrefabInstances(
    uint32 levelIndex,
    vector<PrefabInstanceDesc>& outEntries)
{
    outEntries.clear();

    GAME->Visit_LevelObjects(
        levelIndex,
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (!Is_PrefabInstanceCandidate(gameObject, layerTag))
                return;

            PrefabInstanceDesc entry{};
            entry.prefabName = gameObject->Get_SourcePrefabName();
            entry.layerTag = layerTag;
            entry.instanceData = gameObject->To_Json();

            if (dynamic_pointer_cast<Monster_WorldBase>(gameObject) != nullptr)
            {
                json& customProperties = entry.instanceData["custom_properties"];
                if (customProperties.is_object())
                {
                    customProperties.erase("battle_monster_spawns");
                    customProperties.erase("battle_monster_prefab_names");
                }
            }

            outEntries.push_back(std::move(entry));
        });
}

void MapAuthoringIO::Collect_AuthoredObjects(
    uint32 levelIndex,
    vector<AuthoredObjectDesc>& outEntries)
{
    outEntries.clear();

    GAME->Visit_LevelObjects(
        levelIndex,
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (!Is_AuthoredObjectCandidate(gameObject, layerTag))
                return;

            AuthoredObjectDesc entry{};
            entry.layerTag = layerTag;
            entry.instanceData = gameObject->To_Json();
            entry.className = Resolve_StaticClassName(entry.instanceData);

            if (entry.className.empty())
                return;

            outEntries.push_back(std::move(entry));
        });
}

bool MapAuthoringIO::Save_PrefabInstances_ToMapJson(
    const wstring& filePath,
    uint32 levelIndex)
{
    return Save_MapState_ToMapJson(filePath, levelIndex);
}

bool MapAuthoringIO::Save_AuthoredObjects_ToMapJson(
    const wstring& filePath,
    uint32 levelIndex)
{
    return Save_MapState_ToMapJson(filePath, levelIndex);
}

bool MapAuthoringIO::Save_MapState_ToMapJson(
    const wstring& filePath,
    uint32 levelIndex)
{
    json root = GAME->Load_JsonFile(filePath);
    if (root.is_null() || !root.is_object())
        return false;

    json& mapData = root["MapData"];
    if (!mapData.is_object())
        mapData = json::object();

    if (!Apply_MapStaticMeshTransforms(mapData, filePath, levelIndex))
        return false;

    vector<PrefabInstanceDesc> prefabEntries{};
    Collect_PrefabInstances(levelIndex, prefabEntries);

    vector<AuthoredObjectDesc> authoredEntries{};
    Collect_AuthoredObjects(levelIndex, authoredEntries);

    json prefabInstances = json::array();
    for (const PrefabInstanceDesc& entry : prefabEntries)
    {
        json node = json::object();
        node["PrefabName"] = entry.prefabName;
        node["LayerTag"] = String::ToString(entry.layerTag);
        node["InstanceData"] = entry.instanceData;
        prefabInstances.push_back(std::move(node));
    }

    json authoredObjects = json::array();
    for (const AuthoredObjectDesc& entry : authoredEntries)
    {
        json node = json::object();
        node["ClassName"] = entry.className;
        node["LayerTag"] = String::ToString(entry.layerTag);
        node["InstanceData"] = entry.instanceData;
        authoredObjects.push_back(std::move(node));
    }

    mapData[kPrefabInstancesKey] = std::move(prefabInstances);
    mapData[kAuthoredObjectsKey] = std::move(authoredObjects);

    std::ofstream file(filePath);
    if (!file.is_open())
        return false;

    file << root.dump(4);
    file.close();

    return true;
}

bool MapAuthoringIO::Apply_MapStaticMeshTransforms(
    json& mapData,
    const wstring& filePath,
    uint32 levelIndex)
{
    if (!mapData.contains("StaticMeshes") || !mapData["StaticMeshes"].is_array())
        return true;

    json& staticMeshes = mapData["StaticMeshes"];
    if (staticMeshes.empty())
        return true;

    const fs::path targetMapPath = Normalize_MapPath(filePath);
    bool isValid = true;
    size_t matchedObjectCount = 0;

    GAME->Visit_LevelObjects(
        levelIndex,
        [&](const wstring&, const Shared<GameObject>& gameObject)
        {
            const Shared<MapStaticMesh> mapStaticMesh = dynamic_pointer_cast<MapStaticMesh>(gameObject);
            if (mapStaticMesh == nullptr || !mapStaticMesh->Has_SourceStaticMeshEntry())
                return;

            if (Normalize_MapPath(mapStaticMesh->Get_SourceMapPath()) != targetMapPath)
                return;

            ++matchedObjectCount;

            const size_t sourceIndex = mapStaticMesh->Get_SourceStaticMeshIndex();
            if (sourceIndex >= staticMeshes.size())
            {
                LOG_WARN(
                    "MapStaticMesh save skipped: source index out of range. name={}, index={}, count={}",
                    String::ToString(mapStaticMesh->Get_Name()),
                    sourceIndex,
                    staticMeshes.size());
                isValid = false;
                return;
            }

            json& sourceEntry = staticMeshes[sourceIndex];
            const string sourceId = String::ToString(mapStaticMesh->Get_SourceStaticMeshId());
            const string sourceModelPath = String::ToString(mapStaticMesh->Get_SourceStaticMeshModelPath());

            if (!sourceEntry.is_object() ||
                sourceEntry.value("Id", string{}) != sourceId ||
                sourceEntry.value("ModelPath", string{}) != sourceModelPath)
            {
                LOG_WARN(
                    "MapStaticMesh save skipped: source entry mismatch. name={}, index={}",
                    String::ToString(mapStaticMesh->Get_Name()),
                    sourceIndex);
                isValid = false;
                return;
            }

            const Shared<TransformCom> transform = mapStaticMesh->Get_Transform();
            if (transform == nullptr)
            {
                isValid = false;
                return;
            }

            const Vec3 position = transform->Get_Position();
            const Quat rotation = transform->Get_RotationQuaternion();
            const Vec3 scale = transform->Get_Scale();

            sourceEntry["Position"] = { position.x, position.y, position.z };
            sourceEntry["Rotation"] = { rotation.x, rotation.y, rotation.z, rotation.w };
            sourceEntry["Scale"] = { scale.x, scale.y, scale.z };
        });

    if (matchedObjectCount == 0)
    {
        LOG_WARN(
            "MapStaticMesh save failed: no loaded objects matched map file. path={}",
            String::ToString(targetMapPath.wstring()));
        return false;
    }

    return isValid;
}

bool MapAuthoringIO::Load_PrefabInstances_FromMapData(
    const json& mapData,
    uint32 levelIndex,
    vector<Shared<GameObject>>* spawnedObjects)
{
    return Load_AuthoredObjects_FromMapData(mapData, levelIndex, spawnedObjects);
}

bool MapAuthoringIO::Load_AuthoredObjects_FromMapData(
    const json& mapData,
    uint32 levelIndex,
    vector<Shared<GameObject>>* spawnedObjects)
{
    if (!mapData.is_object())
        return false;

    if (mapData.contains(kPrefabInstancesKey) && mapData[kPrefabInstancesKey].is_array())
    {
        for (const json& node : mapData[kPrefabInstancesKey])
        {
            if (!node.is_object())
                continue;

            const string prefabName = node.value("PrefabName", string{});
            const wstring layerTag = String::ToWString(node.value("LayerTag", string{}));
            if (prefabName.empty() || layerTag.empty())
                continue;

        Shared<GameObject> gameObject = Spawn_Prefab::Prefab(prefabName)
            .AtLevel(levelIndex)
            .InLayer(layerTag)
            .Spawn();
        if (gameObject == nullptr)
        {
            LOG_WARN("MapAuthoringIO prefab instance spawn failed. prefab={}, layer={}",
                prefabName.c_str(),
                String::ToString(layerTag).c_str());
            continue;
        }

            gameObject->Set_RuntimeAuthoringLoaded(true);

            if (node.contains("InstanceData") && node["InstanceData"].is_object())
                gameObject->From_Json(node["InstanceData"]);

            if (spawnedObjects != nullptr)
                spawnedObjects->push_back(gameObject);
        }
    }

    if (mapData.contains(kAuthoredObjectsKey) && mapData[kAuthoredObjectsKey].is_array())
    {
        for (const json& node : mapData[kAuthoredObjectsKey])
        {
            if (!node.is_object())
                continue;

            const string className = node.value("ClassName", string{});
            const wstring layerTag = String::ToWString(node.value("LayerTag", string{}));
            if (className.empty() || layerTag.empty())
                continue;

            Shared<GameObject> gameObject = nullptr;
            if (node.contains("InstanceData") && node["InstanceData"].is_object())
                gameObject = GAME->Instantiate_GameObjectJson(node["InstanceData"]);
            else
                gameObject = GAME->Create_GameObjectPrototype(className);

            if (gameObject == nullptr)
                continue;

            if (FAILED(GAME->Add_GameObject(levelIndex, layerTag, gameObject)))
            {
                gameObject->Free();
                continue;
            }

            gameObject->Set_RuntimeAuthoringLoaded(true);

            if (spawnedObjects != nullptr)
                spawnedObjects->push_back(gameObject);
        }
    }

    return true;
}

NS_END
