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

    struct AuthoredObjectDesc
    {
        string className{};
        wstring layerTag{};
        json instanceData{};
    };

public:
    static bool Is_PrefabInstanceCandidate(const Shared<GameObject>& gameObject, const wstring& layerTag);
    static bool Is_AuthoredObjectCandidate(const Shared<GameObject>& gameObject, const wstring& layerTag);

    static void Collect_PrefabInstances(
        uint32 levelIndex,
        vector<PrefabInstanceDesc>& outEntries);

    static void Collect_AuthoredObjects(
        uint32 levelIndex,
        vector<AuthoredObjectDesc>& outEntries);

    static bool Save_PrefabInstances_ToMapJson(
        const wstring& filePath,
        uint32 levelIndex);

    static bool Save_PrefabInstances_ToMapInstanceAuthoring(
        const wstring& mapInstanceRoot,
        uint32 levelIndex);

    static bool Save_AuthoredObjects_ToMapJson(
        const wstring& filePath,
        uint32 levelIndex);

    static bool Save_MapState_ToMapJson(
        const wstring& filePath,
        uint32 levelIndex);

    static bool Save_Object_ToSourceMap(
        const Shared<GameObject>& gameObject);

    static bool Load_PrefabInstances_FromMapData(
        const json& mapData,
        uint32 levelIndex,
        vector<Shared<GameObject>>* spawnedObjects = nullptr,
        const wstring& sourceMapPath = {},
        size_t prefabStartIndex = 0);

    static bool Load_AuthoredObjects_FromMapData(
        const json& mapData,
        uint32 levelIndex,
        vector<Shared<GameObject>>* spawnedObjects = nullptr,
        const wstring& sourceMapPath = {},
        size_t prefabStartIndex = 0,
        size_t authoredStartIndex = 0);

private:
    static bool Is_RuntimeExcludedObject(const Shared<GameObject>& gameObject);
    static bool Apply_MapStaticMeshTransforms(
        json& mapData,
        const wstring& filePath,
        uint32 levelIndex);
};

NS_END
