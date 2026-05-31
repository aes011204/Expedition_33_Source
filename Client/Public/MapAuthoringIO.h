#pragma once

#include "Client_Defines.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)

class MapAuthoringIO final
{
public:
    struct PrefabInstanceDesc
    {
        string prefabName{};
        wstring layerTag{};
        json instanceData{};
    };

public:
    static bool Is_PrefabInstanceCandidate(const Shared<GameObject>& gameObject, const wstring& layerTag);

    static void Collect_PrefabInstances(
        uint32 levelIndex,
        vector<PrefabInstanceDesc>& outEntries);

    static bool Save_PrefabInstances_ToMapJson(
        const wstring& filePath,
        uint32 levelIndex);

    static bool Load_PrefabInstances_FromMapData(
        const json& mapData,
        uint32 levelIndex,
        vector<Shared<GameObject>>* spawnedObjects = nullptr);

private:
    static bool Is_RuntimeExcludedObject(const Shared<GameObject>& gameObject);
};

NS_END
