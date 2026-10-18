#include "MapAdapter.h"
#include "ModelCom.h"
#include "MaterialCB_OverrideFields.inl"

namespace
{
    using MaterialSlotBinding = pair<uint32, string>;

    constexpr const char* kMapResourceRoot = "../../../Client/Bin/Resources/Map";
    constexpr const char* kMaterialResourceRoot = "../../../Client/Bin/Resources/Materials";
    constexpr const char* kMapInstanceFolderName = "MapInstance";
    constexpr const char* kSaveJsonFolderName = "SaveJson";
    constexpr const char* kStaticMeshFolderName = "StaticMesh";
    constexpr const char* kInstanceMeshFolderName = "InstanceMesh";
    constexpr const char* kPostProcessVolumeFolderName = "PostProcessVolume";
    constexpr const char* kShaftVolumeFolderName = "ShaftVolume";
    constexpr const char* kWaterFolderName = "Water";
    constexpr const char* kETCFolderName = "ETC";

    std::mutex g_materialCacheMutex;
    umap<string, vector<MaterialSlotBinding>> g_materialBindingCache;

    // Path and key helpers
    fs::path MapResourceRootPath()
    {
        return fs::path(kMapResourceRoot);
    }

    fs::path MapInstanceRootPath()
    {
        return MapResourceRootPath() / kMapInstanceFolderName;
    }

    fs::path SaveJsonRootPath()
    {
        return MapResourceRootPath() / kSaveJsonFolderName;
    }

    fs::path MaterialResourceRootPath()
    {
        return fs::path(kMaterialResourceRoot);
    }

    string MakeMapResourcePathString(const fs::path& relativePath)
    {
        return (MapResourceRootPath() / relativePath).generic_string();
    }

    string MaterialBundleStemToModelKey(const fs::path& materialPath)
    {
        string filename = materialPath.filename().string();
        const string suffix = ".material.json";
        if (filename.size() > suffix.size() &&
            String::ToLowerCopy(filename.substr(filename.size() - suffix.size())) == suffix)
        {
            filename.resize(filename.size() - suffix.size());
            return filename + ".model";
        }

        return materialPath.stem().string() + ".model";
    }

    string ReadTexturePathValue(const json& value)
    {
        if (value.is_string())
            return value.get<string>();

        if (value.is_object())
        {
            const string path = value.value("texture_path", string{});
            if (!path.empty())
                return path;
        }

        return {};
    }

    // Split MapInstance ingestion helpers
    bool TryResolveSplitMapInstanceRoot(const wstring& filePath, fs::path& outRoot)
    {
        fs::path root(filePath);

        if (fs::exists(root) && !fs::is_directory(root))
        {
            root = MapInstanceRootPath() / root.stem();
        }
        else if (!fs::exists(root))
        {
            const string mapName = String::ToString(filePath);
            root = MapInstanceRootPath() / mapName;
        }

        if (!fs::exists(root) || !fs::is_directory(root))
            return false;

        outRoot = std::move(root);
        return true;
    }

    bool HasAnySplitObjectDirectory(const fs::path& root)
    {
        auto hasDirOrBin = [&](const char* name) {
            return fs::exists(root / name) || fs::exists(root / (string(name) + ".bin"));
            };

        return hasDirOrBin(kStaticMeshFolderName) ||
            hasDirOrBin(kInstanceMeshFolderName) ||
            hasDirOrBin(kPostProcessVolumeFolderName) ||
            hasDirOrBin(kShaftVolumeFolderName) ||
            hasDirOrBin(kWaterFolderName);
    }

    fs::path ResolveSplitSidecarJson(const wstring& filePath, const fs::path& root)
    {
        fs::path sidecarJson(filePath);
        if (!fs::exists(sidecarJson) || fs::is_directory(sidecarJson))
            sidecarJson = SaveJsonRootPath() / (root.filename().string() + ".json");

        return sidecarJson;
    }

    fs::path ResolveSplitAuthoringJson(const fs::path& root)
    {
        return root / "PrefabInstances.json";
    }

    void AppendPreloadModelPath(const string& modelPath, vector<string>& preloadModelPaths)
    {
        if (modelPath.empty())
            return;

        if (find(preloadModelPaths.begin(), preloadModelPaths.end(), modelPath) == preloadModelPaths.end())
            preloadModelPaths.push_back(modelPath);
    }

    void LoadSplitJsonObjects(const fs::path& dir, vector<json>& outArray, vector<string>& preloadModelPaths, function<bool(const json&)> filter = nullptr)
    {
        fs::path binPath = dir.parent_path() / (dir.filename().string() + ".bin");
        bool preferBin = fs::exists(binPath);
        if (preferBin && fs::exists(dir))
        {
            try
            {
                const auto binWriteTime = fs::last_write_time(binPath);
                for (const fs::directory_entry& entry : fs::directory_iterator(dir))
                {
                    if (!entry.is_regular_file() || entry.path().extension() != ".json")
                        continue;

                    if (fs::last_write_time(entry.path()) > binWriteTime)
                    {
                        preferBin = false;
                        break;
                    }
                }
            }
            catch (...)
            {
            }
        }

        if (preferBin)
        {
            ifstream ifs(binPath, ios::binary);
            if (ifs.is_open())
            {
                try
                {
                    vector<uint8_t> data((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                    json arr = json::from_msgpack(data);
                    if (arr.is_array())
                    {
                        for (auto& item : arr)
                        {
                            if (!item.is_object())
                                continue;
                            if (filter && !filter(item))
                                continue;
                            AppendPreloadModelPath(item.value("ModelPath", string{}), preloadModelPaths);
                            outArray.push_back(std::move(item));
                        }
                    }

                    LOG_DEBUG("[MapAdapter] bin loaded: '{}'", binPath.generic_string());
                    return;
                }
                catch (...) {}
            }
        }

        if (!fs::exists(dir))
            return;

        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;

            ifstream ifs(entry.path());
            if (!ifs.is_open())
                continue;

            try
            {
                json item = json::parse(ifs);
                if (!item.is_object())
                    continue;
                if (filter && !filter(item))
                    continue;

                AppendPreloadModelPath(item.value("ModelPath", string{}), preloadModelPaths);
                LOG_DEBUG("[MapAdapter] json loaded: '{}'", entry.path().generic_string());
                outArray.push_back(std::move(item));
            }
            catch (...)
            {
            }
        }
    }

    void LoadSplitAuthoringObjects(const fs::path& authoringJson, vector<json>& outPrefabInstances, vector<json>& outAuthoredObjects)
    {
        if (!fs::exists(authoringJson) || !fs::is_regular_file(authoringJson))
            return;

        ifstream ifs(authoringJson);
        if (!ifs.is_open())
            return;

        try
        {
            const json root = json::parse(ifs);
            if (!root.is_object() || !root.contains("MapData") || !root["MapData"].is_object())
                return;

            const json& mapData = root["MapData"];
            if (mapData.contains("PrefabInstances") && mapData["PrefabInstances"].is_array())
            {
                for (const json& item : mapData["PrefabInstances"])
                {
                    if (item.is_object())
                        outPrefabInstances.push_back(item);
                }
            }

            if (mapData.contains("AuthoredObjects") && mapData["AuthoredObjects"].is_array())
            {
                for (const json& item : mapData["AuthoredObjects"])
                {
                    if (item.is_object())
                        outAuthoredObjects.push_back(item);
                }
            }
        }
        catch (...)
        {
        }
    }

    // ModelOverride and Materials/<MapName> ingestion helpers
    void AppendMaterialBindingsFromSavedOverride(const json& entry, MapAdapter::ModelOverrideEntry& overrideEntry)
    {
        if (!entry.value("HasMaterialBindings", false) ||
            !entry.contains("MaterialBindings") ||
            !entry["MaterialBindings"].is_array())
        {
            return;
        }

        overrideEntry.modelJson["material_bindings"] = entry["MaterialBindings"];

        if (entry.contains("BaseBundleKey") && entry["BaseBundleKey"].is_string())
            overrideEntry.modelJson["base_bundle_key"] = entry["BaseBundleKey"].get<string>();
    }

    void AppendLegacyPassToMaterialBindings(const json& entry, MapAdapter::ModelOverrideEntry& overrideEntry)
    {
        const int32 legacyPassIndex = entry.value("PassIndex", -1);
        if (legacyPassIndex < 0)
            return;

        if (!overrideEntry.modelJson.contains("material_bindings") ||
            !overrideEntry.modelJson["material_bindings"].is_array())
        {
            overrideEntry.modelJson["material_bindings"] = json::array();
        }

        json& bindings = overrideEntry.modelJson["material_bindings"];
        if (bindings.empty())
        {
            json binding = json::object();
            binding["material_index"] = 0u;
            binding["instance_override"]["force_pass_index"] = legacyPassIndex;
            bindings.push_back(std::move(binding));
            return;
        }

        for (json& binding : bindings)
        {
            if (!binding.is_object())
                continue;

            if (!binding.contains("instance_override") || !binding["instance_override"].is_object())
                binding["instance_override"] = json::object();

            json& instJson = binding["instance_override"];
            if (!instJson.contains("force_pass_index") && !instJson.contains("forcePassIndex"))
                instJson["force_pass_index"] = legacyPassIndex;
        }
    }

    void UpsertMaterialModelJson(
        unordered_map<string, MapAdapter::ModelOverrideEntry>& outOverrides,
        const string& modelKey,
        json modelJson)
    {
        MapAdapter::ModelOverrideEntry& entry = outOverrides[modelKey];
        entry.modelJson = std::move(modelJson);
    }

    bool TryResolveModelKeyFromMaterialJson(const json& root, const fs::path& materialPath, string& outModelKey)
    {
        outModelKey.clear();

        string modelKey = root.value("editor_modelPath", string{});
        if (!modelKey.empty())
            modelKey = fs::path(modelKey).filename().string();
        else
            modelKey = MaterialBundleStemToModelKey(materialPath);

        if (modelKey.empty())
            return false;

        outModelKey = std::move(modelKey);
        return true;
    }

    bool TryAppendModelOverrideFromV2MaterialJson(
        const json& root,
        const fs::path& materialPath,
        unordered_map<string, MapAdapter::ModelOverrideEntry>& outOverrides)
    {
        if (!root.contains("material_bindings") || !root["material_bindings"].is_array())
            return false;

        string modelKey;
        if (!TryResolveModelKeyFromMaterialJson(root, materialPath, modelKey))
            return true;

        UpsertMaterialModelJson(outOverrides, modelKey, root);
        return true;
    }

    bool TryResolveLegacyMaterialModelKey(
        const json& root,
        const fs::path& materialPath,
        string& outModelKey,
        json& outInstanceJson)
    {
        outModelKey.clear();
        outInstanceJson = json{};

        if (!root.contains("instances") || !root["instances"].is_array() || root["instances"].empty())
            return false;

        const json& instJson = root["instances"][0];
        if (!instJson.is_object())
            return false;

        string modelKey = instJson.value("editor_modelPath", string{});
        if (!modelKey.empty())
            modelKey = fs::path(modelKey).filename().string();
        else
            modelKey = MaterialBundleStemToModelKey(materialPath);

        if (modelKey.empty())
            return false;

        outModelKey = std::move(modelKey);
        outInstanceJson = instJson;
        return true;
    }

    void AppendLegacyMaterialTextures(const json& instanceJson, json& instanceOverride)
    {
        if (!instanceJson.contains("textures") || !instanceJson["textures"].is_object())
            return;

        const json& texObj = instanceJson["textures"];
        json textures = json::object();

        auto parseTex = [&](const string& srcName, const string& dstName)
        {
            if (!texObj.contains(srcName))
                return;

            const string texPath = ReadTexturePathValue(texObj[srcName]);
            if (!texPath.empty())
                textures[dstName] = { {"enabled", true}, {"texture_path", texPath} };
        };

        parseTex("baseColor", "base_color");
        parseTex("normal", "normal");
        parseTex("orm", "orm");
        parseTex("emissive", "emissive");
        parseTex("opacityMask", "opacity_mask");

        if (!textures.empty())
            instanceOverride["textures"] = textures;
    }

    json BuildLegacyMaterialInstanceOverride(const json& instanceJson)
    {
        json instanceOverride = json::object();
        const int32 legacyPassIndex = instanceJson.value("forcePassIndex", instanceJson.value("force_pass_index", -1));
        if (legacyPassIndex >= 0)
            instanceOverride["force_pass_index"] = legacyPassIndex;

        if (instanceJson.contains("cb") && instanceJson["cb"].is_object())
        {
            json cbOverrides = json::object();
#define MAPADAPTER_SET_CB_OVERRIDE(_key, _member, _kind) \
            if (instanceJson["cb"].contains(_key)) cbOverrides[_key] = true;
            MATERIAL_CB_OVERRIDE_FIELDS(MAPADAPTER_SET_CB_OVERRIDE)
#undef MAPADAPTER_SET_CB_OVERRIDE

            if (!cbOverrides.empty())
            {
                instanceOverride["cb_overrides"] = cbOverrides;
                instanceOverride["cb"] = instanceJson["cb"];
            }
        }

        AppendLegacyMaterialTextures(instanceJson, instanceOverride);
        return instanceOverride;
    }

    void AppendLegacyMaterialBindings(const json& root, const json& instanceOverride, json& outMaterialBindings)
    {
        if (!root.contains("mappings") || !root["mappings"].is_array())
            return;

        for (const json& mapping : root["mappings"])
        {
            if (!mapping.is_object())
                continue;

            const uint32 materialIndex = mapping.value("materialIndex", UINT32_MAX);
            if (materialIndex == UINT32_MAX)
                continue;

            json binding = json::object();
            binding["material_index"] = materialIndex;
            binding["instance_override"] = instanceOverride;
            outMaterialBindings.push_back(std::move(binding));
        }
    }

    bool TryAppendModelOverrideFromLegacyMaterialJson(
        const json& root,
        const fs::path& materialPath,
        unordered_map<string, MapAdapter::ModelOverrideEntry>& outOverrides)
    {
        string modelKey;
        json instanceJson;
        if (!TryResolveLegacyMaterialModelKey(root, materialPath, modelKey, instanceJson))
            return false;

        const json instanceOverride = BuildLegacyMaterialInstanceOverride(instanceJson);
        json materialBindings = json::array();
        AppendLegacyMaterialBindings(root, instanceOverride, materialBindings);

        if (materialBindings.empty())
        {
            json binding = json::object();
            binding["material_index"] = 0u;
            binding["instance_override"] = instanceOverride;
            materialBindings.push_back(std::move(binding));
        }

        json modelJson = json::object();
        modelJson["material_bindings"] = std::move(materialBindings);

        UpsertMaterialModelJson(outOverrides, modelKey, std::move(modelJson));
        return true;
    }

    // Model sidecar material extraction and runtime application helpers
    bool TryGetCachedSidecarBindings(const string& jsonPathKey, vector<MaterialSlotBinding>& outBindings)
    {
        std::lock_guard<std::mutex> lock(g_materialCacheMutex);
        const auto cacheIt = g_materialBindingCache.find(jsonPathKey);
        if (cacheIt == g_materialBindingCache.end())
            return false;

        outBindings = cacheIt->second;
        return true;
    }

    void CacheSidecarBindings(const string& jsonPathKey, const vector<MaterialSlotBinding>& bindings)
    {
        std::lock_guard<std::mutex> lock(g_materialCacheMutex);
        g_materialBindingCache[jsonPathKey] = bindings;
    }

    string ExtractMaterialAssetName(const string& objectPath)
    {
        const size_t dot = objectPath.find('.');
        const string packagePath = (dot != string::npos)
            ? objectPath.substr(0, dot)
            : objectPath;

        const size_t slash = packagePath.rfind('/');
        return (slash != string::npos)
            ? packagePath.substr(slash + 1)
            : packagePath;
    }

    bool TryAppendStaticMaterialBinding(const json& materialJson, uint32 materialIndex, vector<MaterialSlotBinding>& outBindings)
    {
        if (!materialJson.contains("MaterialInterface"))
            return false;

        const json& materialInterface = materialJson["MaterialInterface"];
        if (!materialInterface.is_object())
            return false;

        const string objectPath = materialInterface.value("ObjectPath", "");
        if (objectPath.empty())
            return false;

        const string assetName = ExtractMaterialAssetName(objectPath);
        if (assetName.empty())
            return false;

        outBindings.emplace_back(materialIndex, assetName);
        return true;
    }

    void AppendStaticMaterialsFromModelJson(const json& modelJson, vector<MaterialSlotBinding>& outBindings)
    {
        for (const json& entry : modelJson)
        {
            if (entry.value("Type", "") != "StaticMesh")
                continue;

            if (!entry.contains("Properties") ||
                !entry["Properties"].contains("StaticMaterials"))
            {
                break;
            }

            const json& staticMats = entry["Properties"]["StaticMaterials"];
            for (uint32 i = 0; i < static_cast<uint32>(staticMats.size()); ++i)
                (void)TryAppendStaticMaterialBinding(staticMats[i], i, outBindings);

            break;
        }
    }

    vector<MaterialSlotBinding> ReadSidecarStaticMaterials(const fs::path& jsonPath)
    {
        vector<MaterialSlotBinding> bindings;
        ifstream jfs(jsonPath);

        try
        {
            if (jfs.is_open())
            {
                json modelJson = json::parse(jfs);
                AppendStaticMaterialsFromModelJson(modelJson, bindings);
            }
        }
        catch (...)
        {
        }

        return bindings;
    }

    vector<MaterialSlotBinding> LoadSidecarStaticMaterials(const string& modelFullPath)
    {
        fs::path jsonPath = fs::path(modelFullPath).replace_extension(".json");
        const string jsonPathKey = jsonPath.generic_string();

        vector<MaterialSlotBinding> bindings;
        if (TryGetCachedSidecarBindings(jsonPathKey, bindings))
            return bindings;

        bindings = ReadSidecarStaticMaterials(jsonPath);
        CacheSidecarBindings(jsonPathKey, bindings);
        return bindings;
    }

    string ResolveBundleKey(const MapAdapter::MaterialApplyDesc& desc)
    {
        string bundleKey = fs::path(desc.modelFullPath).parent_path().filename().string();
        if (bundleKey.empty())
            bundleKey = desc.modelFolder;

        transform(bundleKey.begin(), bundleKey.end(), bundleKey.begin(), ::tolower);
        return bundleKey;
    }

    void FillMissingBaseBundleKeyForBindings(json& modelJson, const string& fallbackBundleKey)
    {
        const string rootBaseBundleKey = modelJson.value("base_bundle_key", string{});
        const string resolvedBundleKey = rootBaseBundleKey.empty() ? fallbackBundleKey : rootBaseBundleKey;

        if (!modelJson.contains("material_bindings") || !modelJson["material_bindings"].is_array())
            return;

        for (json& binding : modelJson["material_bindings"])
        {
            if (!binding.is_object())
                continue;

            const string savedBundleKey = binding.value("base_bundle_key", string{});
            if (savedBundleKey.empty())
                binding["base_bundle_key"] = resolvedBundleKey;
        }
    }

    void ApplyBaseBindings(ModelCom& model, const string& bundleKey, const vector<MaterialSlotBinding>& bindings)
    {
        for (const MaterialSlotBinding& binding : bindings)
            model.Apply_MaterialByAssetName(binding.first, bundleKey, binding.second);
    }

    void LogBaseMaterialMatch(const string& modelStem, const string& bundleKey, size_t bindingCount, const MapAdapter::MaterialOverrideView& view)
    {
        if (!view.hasOverride)
            return;

        LOG_DEBUG(
            "[MapLoader::Material] base material match model='{}' baseBundle='{}' staticBindings={}",
            modelStem,
            bundleKey,
            bindingCount);
    }

    string ResolveLegacyTexturePath(const string& mapResourceRoot, const string& texPath)
    {
        const fs::path texFsPath(texPath);
        if (texFsPath.is_absolute() ||
            texPath.starts_with("../") ||
            texPath.starts_with("..\\"))
        {
            return texPath;
        }

        return (fs::path(mapResourceRoot) / texFsPath).generic_string();
    }

    void ApplyTextureSlotToAllMaterials(ModelCom& model, MaterialTextureSlot slot, const string& texturePath)
    {
        for (uint32 i = 0; i < static_cast<uint32>(model.Get_NumMaterials()); ++i)
        {
            auto mat = model.Get_Material(i);
            if (mat)
                mat->Override_TextureSlot(slot, 0, String::ToWString(texturePath));
        }
    }

    void ApplyLegacyTextureOverride(ModelCom& model, const string& mapResourceRoot, int32 slotIdx, const string& texPath)
    {
        if (texPath.empty())
            return;

        const string fullPath = ResolveLegacyTexturePath(mapResourceRoot, texPath);
        const auto slot = static_cast<MaterialTextureSlot>(slotIdx);
        ApplyTextureSlotToAllMaterials(model, slot, fullPath);
    }

    void ApplyLegacyTextureOverrides(ModelCom& model, const MapAdapter::MaterialApplyDesc& desc)
    {
        if (desc.overrideView.textureSlots == nullptr)
            return;

        for (const auto& [slotIdx, texPath] : *desc.overrideView.textureSlots)
            ApplyLegacyTextureOverride(model, desc.mapResourceRoot, slotIdx, texPath);
    }

    bool HasRuntimeMaterialBindings(const MapAdapter::MaterialOverrideView& view)
    {
        return view.hasOverride &&
            view.modelJson != nullptr &&
            view.modelJson->is_object() &&
            view.modelJson->contains("material_bindings");
    }

    void ApplyRuntimeMaterialBindings(ModelCom& model, const string& modelStem, const string& bundleKey, const MapAdapter::MaterialOverrideView& view)
    {
        if (!HasRuntimeMaterialBindings(view))
            return;

        json modelJson = *view.modelJson;
        FillMissingBaseBundleKeyForBindings(modelJson, bundleKey);

        model.From_Json(modelJson);
        LOG_DEBUG(
            "[MapLoader::Material] model json material applied model='{}' baseBundle='{}' bindings={}",
            modelStem,
            bundleKey,
            modelJson["material_bindings"].size());
    }
}

string MapAdapter::ResolveMapModelPath(const string& modelFolder, const string& modelPathStr)
{
    const fs::path modelPath(modelPathStr);
    const string directPathA = MakeMapResourcePathString(modelPath);
    if (fs::exists(fs::path(directPathA)))
        return directPathA;

    const string folderPathA = MakeMapResourcePathString(fs::path(modelFolder) / modelPath);
    if (fs::exists(fs::path(folderPathA)))
        return folderPathA;

    return modelPath.has_parent_path() ? directPathA : folderPathA;
}

bool MapAdapter::TryLoadSplitMapInstanceData(const wstring& filePath, SplitMapInstanceData& outData)
{
    fs::path root;
    if (!TryResolveSplitMapInstanceRoot(filePath, root))
        return false;

    if (!HasAnySplitObjectDirectory(root))
        return false;

    SplitMapInstanceData data{};
    data.root = root;
    data.sidecarJson = ResolveSplitSidecarJson(filePath, root);
    data.authoringJson = ResolveSplitAuthoringJson(root);
    data.modelFolder = root.filename().string();

    LoadSplitJsonObjects(root / kStaticMeshFolderName, data.staticMeshes, data.preloadModelPaths);
    LoadSplitJsonObjects(root / kInstanceMeshFolderName, data.instancedMeshes, data.preloadModelPaths);
    LoadSplitJsonObjects(root / kPostProcessVolumeFolderName, data.postProcessVolumes, data.preloadModelPaths);
    LoadSplitJsonObjects(root / kShaftVolumeFolderName, data.shaftVolumes, data.preloadModelPaths);
    LoadSplitJsonObjects(root / kWaterFolderName, data.waters, data.preloadModelPaths);
    LoadSplitJsonObjects(root / kETCFolderName, data.lights, data.preloadModelPaths,
        [](const json& j) { return j.value("LightType", "Directional") == "Point"; });
    LoadSplitAuthoringObjects(data.authoringJson, data.prefabInstances, data.authoredObjects);

    TryAppendModelOverridesFromJsonFile(data.sidecarJson, data.modelOverrides);
    AppendModelOverridesFromMaterialDirectory(MaterialResourceRootPath() / root.filename(), data.modelOverrides);
    MapCollisionAdapter::AppendSiblingModelCollisionSources(
        data.modelFolder,
        data.staticMeshes,
        data.cookedCollisionCache,
        data.primitiveCollisionCache);

    outData = std::move(data);
    return true;
}

void MapAdapter::AppendModelOverrides(const json& mapData, unordered_map<string, ModelOverrideEntry>& outOverrides)
{
    if (!mapData.contains("ModelOverrides") || !mapData["ModelOverrides"].is_object())
        return;

    for (auto& [filename, entry] : mapData["ModelOverrides"].items())
    {
        if (!entry.is_object())
            continue;

        ModelOverrideEntry overrideEntry;
        AppendMaterialBindingsFromSavedOverride(entry, overrideEntry);
        AppendLegacyPassToMaterialBindings(entry, overrideEntry);

        if (entry.contains("TextureSlots") && entry["TextureSlots"].is_object())
        {
            for (auto& [slotStr, texPath] : entry["TextureSlots"].items())
            {
                if (!texPath.is_string())
                    continue;

                try
                {
                    overrideEntry.textureSlots[stoi(slotStr)] = texPath.get<string>();
                }
                catch (...)
                {
                }
            }
        }

        outOverrides[filename] = overrideEntry;
    }
}

void MapAdapter::TryAppendModelOverridesFromJsonFile(const fs::path& jsonPath, unordered_map<string, ModelOverrideEntry>& outOverrides)
{
    if (jsonPath.empty() || !fs::exists(jsonPath) || !fs::is_regular_file(jsonPath))
        return;

    ifstream ifs(jsonPath);
    if (!ifs.is_open())
        return;

    try
    {
        json root = json::parse(ifs);
        if (root.contains("MapData") && root["MapData"].is_object())
            AppendModelOverrides(root["MapData"], outOverrides);
    }
    catch (...)
    {
    }
}

void MapAdapter::AppendModelOverridesFromMaterialMapName(const string& mapName, unordered_map<string, ModelOverrideEntry>& outOverrides)
{
    if (mapName.empty())
        return;

    AppendModelOverridesFromMaterialDirectory(MaterialResourceRootPath() / mapName, outOverrides);
}

void MapAdapter::AppendModelOverridesFromMaterialDirectory(const fs::path& materialDir, unordered_map<string, ModelOverrideEntry>& outOverrides)
{
    if (materialDir.empty() || !fs::exists(materialDir) || !fs::is_directory(materialDir))
        return;

    for (const fs::directory_entry& entry : fs::directory_iterator(materialDir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;

        ifstream ifs(entry.path());
        if (!ifs.is_open())
            continue;

        try
        {
            json root = json::parse(ifs);
            if (TryAppendModelOverrideFromV2MaterialJson(root, entry.path(), outOverrides))
                continue;

            (void)TryAppendModelOverrideFromLegacyMaterialJson(root, entry.path(), outOverrides);
        }
        catch (...)
        {
        }
    }
}

MapAdapter::MaterialApplyDesc MapAdapter::BuildMaterialApplyDesc(
    const string& modelFolder,
    const string& modelFullPath,
    const ModelOverrideEntry* overrideEntry)
{
    MaterialApplyDesc desc{};
    desc.modelFolder = modelFolder;
    desc.modelFullPath = modelFullPath;
    desc.mapResourceRoot = kMapResourceRoot;

    if (overrideEntry == nullptr)
        return desc;

    desc.overrideView.hasOverride = true;
    desc.overrideView.textureSlots = &overrideEntry->textureSlots;
    desc.overrideView.modelJson = &overrideEntry->modelJson;
    return desc;
}

void MapAdapter::ApplyMaterial(ModelCom& model, const MaterialApplyDesc& desc)
{
    if (desc.modelFolder.empty())
        return;

    const string modelStem = fs::path(desc.modelFullPath).stem().string();
    const string bundleKey = ResolveBundleKey(desc);
    const vector<MaterialSlotBinding> bindings = LoadSidecarStaticMaterials(desc.modelFullPath);

    ApplyBaseBindings(model, bundleKey, bindings);
    LogBaseMaterialMatch(modelStem, bundleKey, bindings.size(), desc.overrideView);
    ApplyLegacyTextureOverrides(model, desc);
    ApplyRuntimeMaterialBindings(model, modelStem, bundleKey, desc.overrideView);
}

void MapAdapter::Clear_MaterialCache()
{
    std::lock_guard<std::mutex> lock(g_materialCacheMutex);
    g_materialBindingCache.clear();
}
