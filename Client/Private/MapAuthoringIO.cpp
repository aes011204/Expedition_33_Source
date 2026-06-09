#include "MapAuthoringIO.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Monster_WorldBase.h"
#include "Spawn_Prefab.h"

#include <fstream>

NS_BEGIN(Client)

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
        return false; // 에디터가 관리하는 authored 프리펩만 저장한다.

    if (layerTag == TEXT("Layer_Player"))
        return false; // 기본 파티 스폰은 authored 데이터로 다시 저장하지 않는다.

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
            entry.instanceData = gameObject->To_Json(); // 인스턴스별 authored 상태를 그대로 저장한다.

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

bool MapAuthoringIO::Save_PrefabInstances_ToMapJson(
    const wstring& filePath,
    uint32 levelIndex)
{
    json root = GAME->Load_JsonFile(filePath);
    if (root.is_null() || !root.is_object())
        return false;

    json& mapData = root["MapData"];
    if (!mapData.is_object())
        mapData = json::object();

    vector<PrefabInstanceDesc> entries{};
    Collect_PrefabInstances(levelIndex, entries);

    json prefabInstances = json::array();
    for (const PrefabInstanceDesc& entry : entries)
    {
        json node = json::object();
        node["PrefabName"] = entry.prefabName;
        node["LayerTag"] = String::ToString(entry.layerTag);
        node["InstanceData"] = entry.instanceData;
        prefabInstances.push_back(std::move(node));
    }

    mapData["PrefabInstances"] = std::move(prefabInstances);

    std::ofstream file(filePath);
    if (!file.is_open())
        return false;

    file << root.dump(4);
    file.close();

    return true;
}

bool MapAuthoringIO::Load_PrefabInstances_FromMapData(
    const json& mapData,
    uint32 levelIndex,
    vector<Shared<GameObject>>* spawnedObjects)
{
    if (!mapData.is_object())
        return false;

    if (!mapData.contains("PrefabInstances") || !mapData["PrefabInstances"].is_array())
        return true; // 이 섹션이 없는 기존 맵도 정상 로드한다.

    for (const json& node : mapData["PrefabInstances"])
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
            continue;

        gameObject->Set_RuntimeAuthoringLoaded(true); // 맵 JSON에서 복원된 authored 프리펩임을 런타임에만 표시한다.

        if (node.contains("InstanceData") && node["InstanceData"].is_object())
            gameObject->From_Json(node["InstanceData"]); // prefab 기본값 위에 authored 인스턴스 상태를 덮는다.

        if (spawnedObjects != nullptr)
            spawnedObjects->push_back(gameObject);
    }

    return true;
}

NS_END
