#pragma once

#include "Client_Defines.h"
#include "MapCollisionAdapter.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)

class MapAdapter final
{
public:
    struct ModelOverrideEntry
    {
        unordered_map<int32, string> textureSlots;
        json modelJson = json::object();
    };

    struct MaterialOverrideView
    {
        bool hasOverride = false;
        const unordered_map<int32, string>* textureSlots = nullptr;
        const json* modelJson = nullptr;
    };

    struct MaterialApplyDesc
    {
        string modelFolder = {};
        string modelFullPath = {};
        string mapResourceRoot = {};
        MaterialOverrideView overrideView = {};
    };

    struct SplitMapInstanceData
    {
        fs::path root = {};
        fs::path sidecarJson = {};
        fs::path authoringJson = {};
        string modelFolder = {};

        unordered_map<string, ModelOverrideEntry> modelOverrides = {};
        vector<json> staticMeshes = {};
        vector<json> instancedMeshes = {};
        vector<json> postProcessVolumes = {};
        vector<json> shaftVolumes = {};
        vector<json> waters = {};
        vector<json> lights;
        vector<json> prefabInstances = {};
        vector<json> authoredObjects = {};
        vector<string> preloadModelPaths = {};
        MapCollisionAdapter::CookedCollisionCache cookedCollisionCache = {};
        MapCollisionAdapter::PrimitiveCollisionCache primitiveCollisionCache = {};
    };

public:
    // Path normalization used before actual runtime object creation.
    static string ResolveMapModelPath(const string& modelFolder, const string& modelPathStr);

    // Split MapInstance and material override ingestion.
    static bool TryLoadSplitMapInstanceData(const wstring& filePath, SplitMapInstanceData& outData);
    static void AppendModelOverrides(const json& mapData, unordered_map<string, ModelOverrideEntry>& outOverrides);
    static void TryAppendModelOverridesFromJsonFile(const fs::path& jsonPath, unordered_map<string, ModelOverrideEntry>& outOverrides);
    static void AppendModelOverridesFromMaterialMapName(const string& mapName, unordered_map<string, ModelOverrideEntry>& outOverrides);
    static void AppendModelOverridesFromMaterialDirectory(const fs::path& materialDir, unordered_map<string, ModelOverrideEntry>& outOverrides);
    // Runtime material application for loaded ModelCom prototypes.
    static MaterialApplyDesc BuildMaterialApplyDesc(const string& modelFolder, const string& modelFullPath, const ModelOverrideEntry* overrideEntry);
    static void ApplyMaterial(Engine::ModelCom& model, const MaterialApplyDesc& desc);
    static void Clear_MaterialCache();
};

NS_END
