#include "MapLoader.h"
#include "GameInstance.h"
#include "MapAdapter.h"
#include "MapCollisionAdapter.h"
#include "MapAuthoringIO.h"
#include "MapStaticMesh.h"
#include "MapInstancedMesh.h"
#include "DebugMemoryTracker.h"
#include "Mesh.h"
#include "ModelCom.h"
#include "StaticCollisionData.h"
#include "Shader_CBuffer_Define.h"
#include "MapWater.h"
#include "Client_Constants.h"
#include <mutex>

// 전역 모델 및 콜리전 캐시 정적 멤버 정의 및 초기화
std::mutex MapLoader::g_globalCacheMutex;
umap<string, Shared<ModelCom>> MapLoader::g_globalModelCache;
unordered_set<string> MapLoader::g_globalMissingModelCache;
umap<string, Shared<StaticCollisionData>> MapLoader::g_globalCollisionCache;
umap<string, string> MapLoader::g_globalMaterialSignatureByModel;
uint32 MapLoader::g_currentLevelIndex = UINT32_MAX;
vector<LIGHT_DESC> MapLoader::g_cachedLights;
umap<uint32, wstring> MapLoader::g_loadedMapInstanceRootByLevel;

MapCollisionAdapter::RuntimeContext MapLoader::MakeCollisionRuntimeContext(LoadSession& session)
{
    MapCollisionAdapter::RuntimeContext context{};
    context.globalCacheMutex = &g_globalCacheMutex;
    context.globalCollisionCache = &g_globalCollisionCache;
    context.preloadedCollisionCache = &session.preloadedCollisionCache;
    context.cookedCollisionCache = &session.cookedCollisionCache;
    context.primitiveCollisionCache = &session.primitiveCollisionCache;
    context.outBuildDuration = &session.collisionBuildDuration;
    context.outBuildCount = &session.collisionBuildCount;
    return context;
}

wstring MapLoader::Get_LoadedMapInstanceRoot(uint32 levelIndex)
{
    const auto iter = g_loadedMapInstanceRootByLevel.find(levelIndex);
    if (iter == g_loadedMapInstanceRootByLevel.end())
        return {};

    return iter->second;
}

namespace
{
    void Append_JsonArray(const json& mapData, const string& key, vector<json>& outArray)
    {
        if (!mapData.contains(key) || !mapData[key].is_array())
            return;

        for (const json& item : mapData[key])
        {
            if (item.is_object())
                outArray.push_back(item);
        }
    }

    void Append_UniqueString(vector<string>& values, string value)
    {
        if (value.empty())
            return;

        if (find(values.begin(), values.end(), value) == values.end())
            values.push_back(std::move(value));
    }

    const MapLoader::ModelOverrideEntry* Find_ModelOverrideEntry(
        const unordered_map<string, MapLoader::ModelOverrideEntry>& modelOverrides,
        const string& modelPathStr)
    {
        auto overrideIt = modelOverrides.find(modelPathStr);
        if (overrideIt != modelOverrides.end())
            return &overrideIt->second;

        const string modelFileName = fs::path(modelPathStr).filename().string();
        overrideIt = modelOverrides.find(modelFileName);
        if (overrideIt != modelOverrides.end())
            return &overrideIt->second;

        return nullptr;
    }

    void Append_MaterialOverridesForMapCandidates(
        const json& mapData,
        const wstring& actualPath,
        unordered_map<string, MapLoader::ModelOverrideEntry>& outOverrides)
    {
        vector<string> mapNames;
        Append_UniqueString(mapNames, mapData.value("MapName", string{}));
        Append_UniqueString(mapNames, mapData.value("ModelFolder", string{}));
        Append_UniqueString(mapNames, fs::path(actualPath).stem().string());

        for (const string& mapName : mapNames)
            MapAdapter::AppendModelOverridesFromMaterialMapName(mapName, outOverrides);
    }

    string Build_InstancedMeshNameLabel(const vector<json>& instancedMeshes, const vector<size_t>& groupIndices)
    {
        vector<string> names{};
        names.reserve(groupIndices.size());

        for (size_t idx : groupIndices)
        {
            if (idx >= instancedMeshes.size())
                continue;

            const json& entry = instancedMeshes[idx];
            string name = entry.value("Id", string{});
            if (name.empty())
                name = fs::path(entry.value("ModelPath", string{})).stem().string();

            Append_UniqueString(names, std::move(name));
        }

        if (names.empty())
            return {};

        string label{};

        if (names.size() == 1)
            label = names.front();
        else if (names.size() == 2)
            label = names[0] + ", " + names[1];
        else
            label = names[0] + " +" + to_string(names.size() - 1);

        return label;
    }

    size_t Count_InstancedMeshInstances(const vector<json>& instancedMeshes, const vector<size_t>& groupIndices)
    {
        size_t instanceCount = 0;

        for (size_t idx : groupIndices)
        {
            if (idx >= instancedMeshes.size())
                continue;

            const json& entry = instancedMeshes[idx];
            if (entry.contains("Instances") && entry["Instances"].is_array())
                instanceCount += entry["Instances"].size();
        }

        return instanceCount;
    }

    string Build_InstancedMeshDisplayName(const string& nameLabel, size_t instanceCount)
    {
        if (nameLabel.empty())
            return {};

        return nameLabel + " (cnt:" + to_string(instanceCount) + ")";
    }

    string Build_InstancedMeshChunkDisplayName(const string& nameLabel, size_t chunkInstanceCount, size_t totalInstanceCount)
    {
        if (nameLabel.empty())
            return {};

        if (chunkInstanceCount == totalInstanceCount)
            return Build_InstancedMeshDisplayName(nameLabel, chunkInstanceCount);

        return nameLabel +
            " (cnt:" + to_string(chunkInstanceCount) +
            " / total:" + to_string(totalInstanceCount) + ")";
    }

    void Apply_PostProcessVolumes(const vector<json>& postProcessVolumes)
    {
        FogZoneCB fogCB{};
        bool hasGroundFogGlobals = false;
        for (const auto& e : postProcessVolumes)
        {
            float density = e.value("FogDensity", 0.f);
            if (density <= 0.f)
                continue;

            if (!hasGroundFogGlobals)
            {
                fogCB.groundFogMaxDistance = e.value("GroundFogMaxDistance", 100.f);
                fogCB.groundFogMinAccumulationDistance = e.value("GroundFogMinAccumulationDistance", 10.f);
                fogCB.groundFogDistanceInfluence = e.value("GroundFogDistanceInfluence", 0.15f);
                fogCB.groundFogNearFadeDistance = e.value("GroundFogNearFadeDistance", 12.f);
                fogCB.groundFogNearMinFactor = e.value("GroundFogNearMinFactor", 0.65f);
                fogCB.globalFogIntensity = e.value("GlobalFogIntensity", 0.5f);
                hasGroundFogGlobals = true;
            }

            Vec3 position{};
            if (e.contains("Position"))
            {
                auto& p = e["Position"];
                position = Vec3(p[0], p[1], p[2]);
            }

            Vec3 extent{ 5.f, 5.f, 5.f };
            if (e.contains("Extent"))
            {
                auto& ex = e["Extent"];
                extent = Vec3(ex[0], ex[1], ex[2]);
            }

            Quat rotation = Quat::Identity;
            if (e.contains("Rotation"))
            {
                auto& r = e["Rotation"];
                rotation = Quat(r[0], r[1], r[2], r[3]);
            }

            const string shape = e.value("Shape", "Box");
            if (shape == "OBB")
            {
                if (fogCB.obbCount >= FogZoneCB::MAX_FOG_OBB_ZONES)
                    continue;

                FogOBBZoneEntry entry{};
                const Matrix world =
                    Matrix::CreateFromQuaternion(rotation) *
                    Matrix::CreateTranslation(position);

                entry.worldToLocal = world.Invert();
                entry.halfExtents = extent;
                if (e.contains("FogColor"))
                {
                    auto& c = e["FogColor"];
                    entry.fogColor = Vec4(c[0], c[1], c[2], 1.f);
                }

                entry.fogDensity = density;
                entry.baseHeight = e.value("BaseHeight", 0.f);
                entry.topFadeStart = e.value("TopFadeStart", 0.4f);
                entry.topHeight = e.value("TopHeight", 0.9f);
                entry.edgeFade = e.value("EdgeFade", 1.f);
                entry.noiseScale = e.value("NoiseScale", 0.0025f);
                if (e.contains("NoiseSpeed"))
                {
                    auto& speed = e["NoiseSpeed"];
                    entry.noiseSpeed = Vec2(speed[0], speed[1]);
                }
                entry.noiseStrength = e.value("NoiseStrength", 0.9f);
                entry.noiseMin = e.value("NoiseMin", 0.38f);
                fogCB.obbZones[fogCB.obbCount++] = entry;
                continue;
            }

            if (fogCB.aabbCount >= FogZoneCB::MAX_FOG_ZONES)
                continue;

            FogZoneEntry entry{};
            if (e.contains("FogColor"))
            {
                auto& c = e["FogColor"];
                entry.fogColor = Vec4(c[0], c[1], c[2], 1.f);
            }

            entry.fogDensity = density;
            entry.baseHeight = e.value("BaseHeight", 0.f);
            entry.topFadeStart = e.value("TopFadeStart", 0.4f);
            entry.topHeight = e.value("TopHeight", 0.9f);
            entry.edgeFade = e.value("EdgeFade", 1.f);
            entry.noiseScale = e.value("NoiseScale", 0.0025f);
            if (e.contains("NoiseSpeed"))
            {
                auto& speed = e["NoiseSpeed"];
                entry.noiseSpeed = Vec2(speed[0], speed[1]);
            }
            entry.noiseStrength = e.value("NoiseStrength", 0.9f);
            entry.noiseMin = e.value("NoiseMin", 0.38f);
            entry.zoneMin = position - extent;
            entry.zoneMax = position + extent;
            fogCB.aabbZones[fogCB.aabbCount++] = entry;
        }

        GAME->Set_FogZones(fogCB);
    }

    void Apply_ShaftVolumes(const vector<json>& shaftVolumes)
    {
        VolumetricLightShaftZoneCB shaftCB{};

        for (const auto& e : shaftVolumes)
        {
            if (shaftCB.zoneCount >= VolumetricLightShaftZoneCB::MAX_LIGHT_SHAFT_ZONES)
                break;

            Vec3 position{};
            if (e.contains("Position"))
            {
                auto& p = e["Position"];
                position = Vec3(p[0], p[1], p[2]);
            }

            Quat rotation = Quat::Identity;
            if (e.contains("Rotation"))
            {
                auto& r = e["Rotation"];
                rotation = Quat(r[0], r[1], r[2], r[3]);
            }

            Vec3 extent{ 5.f, 5.f, 5.f };
            if (e.contains("Extent"))
            {
                auto& ex = e["Extent"];
                extent = Vec3(ex[0], ex[1], ex[2]);
            }

            const float radius = e.value("Radius", 5.f);
            const string shape = e.value("Shape", "Box");

            VolumetricLightShaftZoneEntry entry{};
            const Matrix world =
                Matrix::CreateFromQuaternion(rotation) *
                Matrix::CreateTranslation(position);
            entry.worldToLocal = world.Invert();
            entry.halfExtents = (shape == "Sphere") ? Vec3(radius, radius, radius) : extent;
            entry.density = e.value("Density", 1.f);
            if (e.contains("LightColor"))
            {
                auto& c = e["LightColor"];
                entry.lightColor = Vec4(c[0], c[1], c[2], c.size() > 3 ? static_cast<float>(c[3]) : 1.f);
            }
            entry.edgeFade = e.value("EdgeFade", 0.5f);
            entry.intensity = e.value("Intensity", 1.f);
            shaftCB.zones[shaftCB.zoneCount++] = entry;
        }

        GAME->Set_VolumetricLightShaftZones(shaftCB);
    }

    HRESULT Add_WaterObject(
        const json& e,
        uint32 levelIndex,
        const wstring& layerTag,
        vector<Shared<GameObject>>* spawnedObjects)
    {
        const wstring waterTexBase = Constants::Map::WaterPath;
        auto WaterTexPath = [&](const string& filename) -> wstring {
            if (filename.empty())
            {
                return {};
            }
            return waterTexBase + String::ToWString(filename);
            };

        MapWater::MAPWATER_DESC desc{};

        if (e.contains("Position"))
        {
            auto& p = e["Position"];
            desc.position = Vec3(p[0], p[1], p[2]);
        }
        if (e.contains("Rotation"))
        {
            auto& r = e["Rotation"];
            desc.rotation = Quat(r[0], r[1], r[2], r[3]);
        }

        desc.planeDesc.countX = e.value("CountX", 64u);
        desc.planeDesc.countZ = e.value("CountZ", 64u);
        desc.planeDesc.sizeX = e.value("SizeX", 1.f);
        desc.planeDesc.sizeZ = e.value("SizeZ", 1.f);

        desc.largeWaveNormalPath = WaterTexPath(e.value("LargeWaveNormal", string{}));
        desc.smallWaveNormalPath = WaterTexPath(e.value("SmallWaveNormal", string{}));
        desc.seafoamPath = WaterTexPath(e.value("SeafoamTexture", string{}));
        desc.cubemapPath = WaterTexPath(e.value("Cubemap", string{}));

        auto& p = desc.waterParams;
        p.largeWaveScale = e.value("LargeWaveScale", 64.f);
        p.largeWaveSpeed = e.value("LargeWaveSpeed", 0.1f);
        p.largeWaveAmplify = e.value("LargeWaveAmplify", 1.f);
        p.smallWaveScale = e.value("SmallWaveScale", 512.f);
        p.smallWaveSpeed = e.value("SmallWaveSpeed", 1.f);
        p.smallWaveAmplify = e.value("SmallWaveAmplify", 1.f);
        p.seafoamScale = e.value("SeafoamScale", 512.f);
        p.seafoamSpeed = e.value("SeafoamSpeed", 0.015f);
        p.seafoamHeightPower = e.value("SeafoamHeightPower", 2.f);
        p.seafoamHeightMultiply = e.value("SeafoamHeightMultiply", 3.f);
        p.foamDistortion = e.value("FoamDistortion", 0.f);
        p.depthFade = e.value("DepthFade", 0.f);
        p.cameraDistanceFade = e.value("CameraDistanceFade", 0.f);
        p.reflectionAmount = e.value("ReflectionAmount", 0.f);
        p.luminanceBias = e.value("LuminanceBias", 0.f);

        if (e.contains("WaterColor"))
        {
            auto& c = e["WaterColor"];
            p.waterColor = Vec4(c[0], c[1], c[2], c[3]);
        }
        if (e.contains("FresnelColor"))
        {
            auto& c = e["FresnelColor"];
            p.fresnelColor = Vec4(c[0], c[1], c[2], c[3]);
        }

        Shared<GameObject> water = nullptr;
        HRESULT hr = GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("Prototype_MapWater"),
            levelIndex,
            layerTag,
            water,
            &desc);

        if (SUCCEEDED(hr) && spawnedObjects != nullptr && water != nullptr)
            spawnedObjects->push_back(water);

        return hr;
    }

}

HRESULT MapLoader::Try_BeginMapInstanceSession(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const wstring& filePath,
    uint32 levelIndex,
    const wstring& layerTag,
    LoadSession& outSession)
{
    // Split MapInstance 경로 해석:
    // - 파일 경로 입력: MapInstance/<json stem>
    // - 맵 이름 입력: MapInstance/<map name>
    // 각 하위 json은 오브젝트 배치 정보와 Map 루트 기준 ModelPath를 가진다.
    const auto sessionBegin = chrono::steady_clock::now();

    MapAdapter::SplitMapInstanceData splitData{};
    if (!MapAdapter::TryLoadSplitMapInstanceData(filePath, splitData))
        return E_FAIL;

    outSession = {};
    outSession.device = device;
    outSession.context = context;
    outSession.levelIndex = levelIndex;
    outSession.layerTag = layerTag;
    outSession.sourceMapPath = splitData.authoringJson.wstring();
    g_loadedMapInstanceRootByLevel[levelIndex] = splitData.root.wstring();
    outSession.modelFolder = splitData.modelFolder;
    outSession.modelOverrides = std::move(splitData.modelOverrides);
    outSession.staticMeshes = std::move(splitData.staticMeshes);
    outSession.instancedMeshes = std::move(splitData.instancedMeshes);
    outSession.postProcessVolumes = std::move(splitData.postProcessVolumes);
    outSession.shaftVolumes = std::move(splitData.shaftVolumes);
    outSession.waters = std::move(splitData.waters);
    outSession.lights = std::move(splitData.lights);
    outSession.prefabInstances = std::move(splitData.prefabInstances);
    outSession.authoredObjects = std::move(splitData.authoredObjects);
    outSession.preloadModelPaths = std::move(splitData.preloadModelPaths);
    outSession.cookedCollisionCache = std::move(splitData.cookedCollisionCache);
    outSession.primitiveCollisionCache = std::move(splitData.primitiveCollisionCache);

    outSession.preloadModelsDone = outSession.preloadModelPaths.empty();
    outSession.materialsDone = outSession.preloadModelPaths.empty();
    outSession.staticMeshesDone = outSession.staticMeshes.empty();
    outSession.instancedMeshesDone = outSession.instancedMeshes.empty();
    outSession.postProcessVolumesDone = outSession.postProcessVolumes.empty();
    outSession.shaftVolumesDone = outSession.shaftVolumes.empty();
    outSession.watersDone = outSession.waters.empty();
    outSession.lightsDone = outSession.lights.empty();
    outSession.prefabInstancesDone = outSession.prefabInstances.empty();
    outSession.authoredObjectsDone = outSession.authoredObjects.empty();

    outSession.beginSessionDuration =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - sessionBegin);

    LOG_DEBUG(
        "[MapLoader::MapInstance] root='{}' modelFolder='{}' staticMeshes={} instancedMeshes={} postProcessVolumes={} shaftVolumes={} waters={} prefabInstances={} authoredObjects={} preloadModels={} modelOverrides={}",
        splitData.root.generic_string(),
        outSession.modelFolder,
        outSession.staticMeshes.size(),
        outSession.instancedMeshes.size(),
        outSession.postProcessVolumes.size(),
        outSession.shaftVolumes.size(),
        outSession.waters.size(),
        outSession.prefabInstances.size(),
        outSession.authoredObjects.size(),
        outSession.preloadModelPaths.size(),
        outSession.modelOverrides.size());

    return S_OK;
}

HRESULT MapLoader::Begin_LoadSession(const ComPtr<Device>& device, const ComPtr<Context>& context,
    const wstring& filePath, uint32 levelIndex, const wstring& layerTag, LoadSession& outSession)
{
    // 새로운 레벨로 진입한 경우 기존 전역 캐시 클리어 (메모리 누수 방지)
    if (g_currentLevelIndex != levelIndex)
    {
        {
            std::lock_guard<std::mutex> lock(g_globalCacheMutex);
            g_globalModelCache.clear();
            g_globalCollisionCache.clear();
            g_globalMaterialSignatureByModel.clear();
            g_currentLevelIndex = levelIndex;
        }
        MapAdapter::Clear_MaterialCache();
    }

    {
        LoadSession mapInstanceSession{};
        // split MapInstance가 있으면 우선 사용한다.
        // 실패하면 아래 기존 통합 SaveJson/MapData 로드 경로를 그대로 유지한다.
        if (SUCCEEDED(Try_BeginMapInstanceSession(
            device,
            context,
            filePath,
            levelIndex,
            layerTag,
            mapInstanceSession)))
        {
            outSession = std::move(mapInstanceSession);
            return S_OK;
        }
    }

    const auto sessionBegin = chrono::steady_clock::now();

    wstring actualPath = filePath;
    string pathA = String::ToString(actualPath);
    bool isBinary = false;

    // 만약 확장자가 .json이고 동일한 경로에 .bin 파일이 존재하면 자동으로 .bin 로드로 전환
    size_t jsonExtPos = filePath.rfind(L".json");
    if (jsonExtPos != wstring::npos)
    {
        wstring binPath = filePath.substr(0, jsonExtPos) + L".bin";
        string binPathA = String::ToString(binPath);
        ifstream testBin(binPathA, ios::binary);
        if (testBin.is_open())
        {
            bool preferBin = true;

            try
            {
                const fs::path jsonFsPath = fs::path(filePath);
                const fs::path binFsPath = fs::path(binPath);
                if (fs::exists(jsonFsPath) && fs::exists(binFsPath))
                {
                    // If the json was edited after the bin was generated, prefer the fresher source of truth.
                    preferBin = fs::last_write_time(binFsPath) >= fs::last_write_time(jsonFsPath);
                }
            }
            catch (...)
            {
            }

            if (preferBin)
            {
                actualPath = binPath;
                pathA = binPathA;
                isBinary = true;
            }
        }
    }
    else if (filePath.find(L".bin") != wstring::npos)
    {
        isBinary = true;
    }

    json root;
    if (isBinary)
    {
        ifstream ifs(pathA, ios::binary);
        if (!ifs.is_open())
            return E_FAIL;

        try
        {
            root = json::from_msgpack(ifs);
        }
        catch (...)
        {
            return E_FAIL;
        }
    }
    else
    {
        ifstream ifs(pathA);
        if (!ifs.is_open())
            return E_FAIL;

        try
        {
            root = json::parse(ifs);
        }
        catch (...)
        {
            return E_FAIL;
        }
    }

    const json& mapData = root["MapData"];

    outSession = {};
    outSession.device = device;
    outSession.context = context;
    outSession.levelIndex = levelIndex;
    outSession.layerTag = layerTag;
    outSession.sourceMapPath = filePath;
    outSession.modelFolder = mapData.value("ModelFolder", string{});

    Append_JsonArray(mapData, "PostProcessVolumes", outSession.postProcessVolumes);
    Append_JsonArray(mapData, "ShaftVolumes", outSession.shaftVolumes);
    Append_JsonArray(mapData, "Waters", outSession.waters);
    Append_JsonArray(mapData, "Lights", outSession.lights);

    MapAdapter::AppendModelOverrides(mapData, outSession.modelOverrides);
    Append_MaterialOverridesForMapCandidates(mapData, actualPath, outSession.modelOverrides);

    if (mapData.contains("StaticMeshes"))
    {
        for (const json& item : mapData["StaticMeshes"])
        {
            outSession.staticMeshes.push_back(item);

            const string modelPath = item.value("ModelPath", string{});
            if (!modelPath.empty() &&
                find(outSession.preloadModelPaths.begin(), outSession.preloadModelPaths.end(), modelPath) == outSession.preloadModelPaths.end())
            {
                outSession.preloadModelPaths.push_back(modelPath);
            }
        }
    }

    if (mapData.contains("InstancedMeshes"))
    {
        for (const json& item : mapData["InstancedMeshes"])
        {
            outSession.instancedMeshes.push_back(item);

            const string modelPath = item.value("ModelPath", string{});
            if (!modelPath.empty() &&
                find(outSession.preloadModelPaths.begin(), outSession.preloadModelPaths.end(), modelPath) == outSession.preloadModelPaths.end())
            {
                outSession.preloadModelPaths.push_back(modelPath);
            }
        }
    }

    if (mapData.contains("PrefabInstances") && mapData["PrefabInstances"].is_array())
    {
        for (const json& item : mapData["PrefabInstances"])
            outSession.prefabInstances.push_back(item);
    }

    if (mapData.contains("AuthoredObjects") && mapData["AuthoredObjects"].is_array())
    {
        for (const json& item : mapData["AuthoredObjects"])
            outSession.authoredObjects.push_back(item);
    }

    MapCollisionAdapter::AppendModelCollisionSources(
        mapData,
        outSession.cookedCollisionCache,
        outSession.primitiveCollisionCache);
    MapCollisionAdapter::AppendSiblingModelCollisionSources(
        outSession.modelFolder,
        outSession.staticMeshes,
        outSession.cookedCollisionCache,
        outSession.primitiveCollisionCache);

    outSession.preloadModelsDone = outSession.preloadModelPaths.empty();
    outSession.materialsDone = outSession.preloadModelPaths.empty();
    outSession.staticMeshesDone = outSession.staticMeshes.empty();
    outSession.instancedMeshesDone = outSession.instancedMeshes.empty();
    outSession.postProcessVolumesDone = outSession.postProcessVolumes.empty();
    outSession.shaftVolumesDone = outSession.shaftVolumes.empty();
    outSession.watersDone = outSession.waters.empty();
    outSession.lightsDone = outSession.lights.empty();
    outSession.prefabInstancesDone = outSession.prefabInstances.empty();
    outSession.authoredObjectsDone = outSession.authoredObjects.empty();
    outSession.beginSessionDuration =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - sessionBegin);

    return S_OK;
}

HRESULT MapLoader::Preload_SessionData(const ComPtr<Device>& device, const ComPtr<Context>& context,
    const wstring& filePath, uint32 levelIndex, const wstring& layerTag, LoadSession& outSession)
{
    // 1. JSON 파일 로드 및 구조 분석 (백그라운드 스레드)
    CHECK_FAILED(Begin_LoadSession(device, context, filePath, levelIndex, layerTag, outSession), E_FAIL);

    // 2. 모델 리소스 파일 디스크 로드 루프 (백그라운드 스레드)
    while (!outSession.preloadModelsDone)
    {
        if (outSession.nextPreloadModelIndex >= outSession.preloadModelPaths.size())
        {
            outSession.preloadModelsDone = true;
            break;
        }

        const string modelPathStr = outSession.preloadModelPaths[outSession.nextPreloadModelIndex++];
        if (modelPathStr.empty() ||
            outSession.preloadedModelCache.find(modelPathStr) != outSession.preloadedModelCache.end())
        {
            continue;
        }

        // 스레드 안전한 전역 캐시를 활용해 모델 조회 및 로드
        auto model = GetOrLoadModel(
            outSession.device,
            outSession.context,
            outSession.modelFolder,
            modelPathStr,
            outSession.modelOverrides,
            false);

        if (!model)
        {
            continue;
        }

        outSession.preloadedModelCache[modelPathStr] = model;

        // 다른 스레드(특히 메인 스레드)에 CPU 사용권 양보
        this_thread::yield();
    }

    // 2.5. 정적 콜리전 데이터 백그라운드 사전 빌드 (백그라운드 스레드)
    vector<string> collisionModelPaths;
    auto AppendCollisionModelPath = [&collisionModelPaths](const json& e)
        {
            Append_UniqueString(collisionModelPaths, e.value("ModelPath", string{}));
        };

    for (const json& e : outSession.staticMeshes)
        AppendCollisionModelPath(e);

    for (const json& e : outSession.instancedMeshes)
        AppendCollisionModelPath(e);

    auto collisionContext = MakeCollisionRuntimeContext(outSession);
    for (const string& modelPathStr : collisionModelPaths)
    {
        if (outSession.preloadedCollisionCache.find(modelPathStr) != outSession.preloadedCollisionCache.end())
            continue;

        auto it = outSession.preloadedModelCache.find(modelPathStr);
        if (it == outSession.preloadedModelCache.end() || it->second == nullptr)
        {
            auto model = GetOrLoadModel(
                outSession.device,
                outSession.context,
                outSession.modelFolder,
                modelPathStr,
                outSession.modelOverrides,
                false);

            if (!model)
                continue;

            outSession.preloadedModelCache[modelPathStr] = model;
            it = outSession.preloadedModelCache.find(modelPathStr);
        }

        // 저장된 cooked/proxy collision이 있으면 재사용하고, 없을 때만 모델 기반 collision을 빌드한다.
        MapCollisionAdapter::GetOrCreateStaticCollision(collisionContext, it->second, modelPathStr);

        // 다른 스레드(특히 메인 스레드)에 CPU 사용권 양보
        this_thread::yield();
    }

    // 3. 인스턴스 정렬 및 CPU 무거운 콜리전 빌드 사전 완료 (백그라운드 스레드)
    if (!outSession.staticAutoInstancingPrepared)
    {
        Prepare_StaticAutoInstancing(outSession);
        outSession.staticAutoInstancingPrepared = true;
        outSession.staticMeshesDone = outSession.staticMeshes.empty();
        outSession.instancedMeshesDone = outSession.instancedMeshes.empty();
    }

    return S_OK;
}

HRESULT MapLoader::Process_LoadSession(LoadSession& session, size_t maxMeshesPerFrame,
    chrono::milliseconds maxTimePerFrame, bool& outFinished)
{
    // 처리 순서는 의도된 계약이다.
    // preload -> material -> 전역 one-shot 시스템 -> water -> static -> instanced -> prefab 순서를 유지해야
    // 기존 통합 맵과 신규 split 맵의 동작 차이를 줄일 수 있다.
    outFinished = false;
    ++session.processCallCount;

    const auto begin = chrono::steady_clock::now();
    size_t processedCount = 0;

    auto IsBudgetExceeded = [&]() -> bool
        {
            if (processedCount >= maxMeshesPerFrame)
                return true;

            if (processedCount > 0 &&
                maxTimePerFrame.count() > 0 &&
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - begin) >= maxTimePerFrame)
                return true;

            return false;
        };

    while (!session.preloadModelsDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        if (session.nextPreloadModelIndex >= session.preloadModelPaths.size())
        {
            session.preloadModelsDone = true;
            break;
        }

        const string modelPathStr = session.preloadModelPaths[session.nextPreloadModelIndex++];
        if (modelPathStr.empty() ||
            session.preloadedModelCache.find(modelPathStr) != session.preloadedModelCache.end())
        {
            continue;
        }

        // 전역 프로토타입 캐시를 이용해 안전하게 모델 획득 및 갱신
        auto model = GetOrLoadModel(
            session.device,
            session.context,
            session.modelFolder,
            modelPathStr,
            session.modelOverrides,
            false,
            &session.modelCreateDuration,
            &session.modelCreateCount,
            nullptr,
            nullptr);

        if (!model)
        {
            continue;
        }

        session.preloadedModelCache[modelPathStr] = model;

        ++processedCount;
    }

    while (!session.materialsDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        if (session.nextMaterialApplyModelIndex >= session.preloadModelPaths.size())
        {
            session.materialsDone = true;
            break;
        }

        const string modelPathStr = session.preloadModelPaths[session.nextMaterialApplyModelIndex++];
        if (modelPathStr.empty())
            continue;

        auto it = session.preloadedModelCache.find(modelPathStr);
        Shared<ModelCom> model = (it != session.preloadedModelCache.end()) ? it->second : nullptr;

        if (model == nullptr)
        {
            model = GetOrLoadModel(
                session.device,
                session.context,
                session.modelFolder,
                modelPathStr,
                session.modelOverrides,
                true,
                &session.modelCreateDuration,
                &session.modelCreateCount,
                &session.materialApplyDuration,
                &session.materialApplyCount);

            if (model != nullptr)
                session.preloadedModelCache[modelPathStr] = model;
        }

        ApplySessionModelMaterials(session, modelPathStr, model);
        ++processedCount;
    }

    if (!session.staticAutoInstancingPrepared)
    {
        Prepare_StaticAutoInstancing(session);
        session.staticAutoInstancingPrepared = true;
        session.staticMeshesDone = session.staticMeshes.empty();
        session.instancedMeshesDone = session.instancedMeshes.empty();
        session.postProcessVolumesDone = session.postProcessVolumes.empty();
        session.shaftVolumesDone = session.shaftVolumes.empty();
        session.watersDone = session.waters.empty();
        session.prefabInstancesDone = session.prefabInstances.empty();
        session.authoredObjectsDone = session.authoredObjects.empty();
    }

    if (!session.postProcessVolumesDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        Apply_PostProcessVolumes(session.postProcessVolumes);
        session.postProcessVolumesDone = true;
        ++processedCount;
    }

    if (!session.shaftVolumesDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        Apply_ShaftVolumes(session.shaftVolumes);
        session.shaftVolumesDone = true;
        ++processedCount;
    }

    if (!session.lightsDone)
    {
        if (IsBudgetExceeded())
        {
            return S_OK;
        }

        for (const auto& e : session.lights)
        {
            LIGHT_DESC desc{};
            desc.type = LightType::Point;

            const auto& pos = e["Position"];
            desc.position = Vec4(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>(), 1.f);
            desc.range = e.value("Range", 10.f);
            desc.pointAttenuationPower = e.value("AttenuationPower", 1.f);

            const auto& diff = e["Diffuse"];
            desc.diffuse = Vec4(diff[0].get<float>(), diff[1].get<float>(), diff[2].get<float>(), diff[3].get<float>());

            const auto& amb = e["Ambient"];
            desc.ambient = Vec4(amb[0].get<float>(), amb[1].get<float>(), amb[2].get<float>(), amb[3].get<float>());

            const auto& spec = e["Specular"];
            desc.specular = Vec4(spec[0].get<float>(), spec[1].get<float>(), spec[2].get<float>(), spec[3].get<float>());

            g_cachedLights.push_back(desc);
            GAME->Add_PointLight(
                Vec3(desc.position.x, desc.position.y, desc.position.z),
                desc.range,
                desc.diffuse,
                desc.ambient,
                desc.specular,
                desc.pointAttenuationPower);
        }

        session.lightsDone = true;
        ++processedCount;
    }

    while (!session.watersDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        if (session.nextWaterIndex >= session.waters.size())
        {
            session.watersDone = true;
            break;
        }

        const json& e = session.waters[session.nextWaterIndex++];
        Add_WaterObject(e, session.levelIndex, session.layerTag, session.spawnedObjects);
        ++processedCount;
    }

    while (!session.staticMeshesDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        if (session.nextStaticMeshIndex >= session.staticMeshes.size())
        {
            session.staticMeshesDone = true;
            break;
        }

        const size_t sourceStaticMeshIndex = session.nextStaticMeshIndex++;
        const json& e = session.staticMeshes[sourceStaticMeshIndex];
        const string modelPathStr = e.value("ModelPath", string{});
        if (modelPathStr.empty())
            continue;

        MapStaticMesh::MAPSTATICMESH_DESC desc{};
        desc.name = String::ToWString(e.value("Id", string{}));
        desc.sourceStaticMeshIndex = sourceStaticMeshIndex;
        desc.sourceMapPath = session.sourceMapPath;
        desc.visible = e.value("Visible", true);
        desc.cullEnabled = e.value("CullEnabled", true);
        desc.castShadow = e.value("CastShadow", true);

        auto it = session.preloadedModelCache.find(modelPathStr);
        if (it != session.preloadedModelCache.end())
        {
            desc.model = it->second;
            ApplySessionModelMaterials(session, modelPathStr, desc.model);
        }
        else
        {
            // 전역 프로토타입 캐시를 이용해 안전하게 모델 획득 및 갱신
            auto model = GetOrLoadModel(
                session.device,
                session.context,
                session.modelFolder,
                modelPathStr,
                session.modelOverrides,
                false,
                &session.modelCreateDuration,
                &session.modelCreateCount,
                nullptr,
                nullptr);

            if (!model)
            {
                continue;
            }

            session.preloadedModelCache[modelPathStr] = model;
            MarkSessionModelMaterialsApplied(session, modelPathStr);
            desc.model = model;
        }

        // StaticMeshes 전용 collision data cache
        auto collisionIt = session.preloadedCollisionCache.find(modelPathStr);
        if (collisionIt != session.preloadedCollisionCache.end())
        {
            desc.collisionData = collisionIt->second;
        }
        else
        {
            desc.collisionData = MapCollisionAdapter::GetOrCreateStaticCollision(
                MakeCollisionRuntimeContext(session),
                desc.model,
                modelPathStr);
        }

        desc.modelPath = String::ToWString(modelPathStr);

        const auto& pos = e["Position"];
        desc.position = Vec3(
            pos[0].get<float>(),
            pos[1].get<float>(),
            pos[2].get<float>());

        const auto& rot = e["Rotation"];
        desc.rotation = Quat(
            rot[0].get<float>(),
            rot[1].get<float>(),
            rot[2].get<float>(),
            rot[3].get<float>());

        const auto& scl = e["Scale"];
        desc.scale = Vec3(
            scl[0].get<float>(),
            scl[1].get<float>(),
            scl[2].get<float>());

        Shared<GameObject> spawnedObject = nullptr;

        const auto staticAddBegin = chrono::steady_clock::now();
        const HRESULT addResult = GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_MapStaticMesh"),
            session.levelIndex,
            session.layerTag,
            spawnedObject,
            &desc);
        const auto staticAddMs =
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - staticAddBegin);
        session.staticAddDuration += staticAddMs;
        ++session.staticAddCount;

        if (FAILED(addResult))
            continue;

        if (session.spawnedObjects != nullptr && spawnedObject != nullptr)
            session.spawnedObjects->push_back(spawnedObject);

        ++processedCount;
    }

    while (!session.instancedMeshesDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        if (!session.instancedMeshGroupsBuilt)
        {
            for (size_t i = 0; i < session.instancedMeshes.size(); ++i)
            {
                const string mp = session.instancedMeshes[i].value("ModelPath", string{});
                if (mp.empty())
                {
                    continue;
                }

                if (session.instancedMeshGroups.find(mp) == session.instancedMeshGroups.end())
                {
                    session.instancedMeshModelOrder.push_back(mp);
                }
                session.instancedMeshGroups[mp].push_back(i);
            }
            session.instancedMeshGroupsBuilt = true;
        }

        if (session.nextInstancedMeshGroupIndex >= session.instancedMeshModelOrder.size())
        {
            session.instancedMeshesDone = true;
            break;
        }

        const string& modelPathStr = session.instancedMeshModelOrder[session.nextInstancedMeshGroupIndex++];
        const vector<size_t>& groupIndices = session.instancedMeshGroups.at(modelPathStr);
        const json& firstEntry = session.instancedMeshes[groupIndices[0]];
        const string displayNameLabel = Build_InstancedMeshNameLabel(session.instancedMeshes, groupIndices);
        const size_t totalInstanceCount = Count_InstancedMeshInstances(session.instancedMeshes, groupIndices);

        MapInstancedMesh::MAPINSTANCEDMESH_DESC desc{};
        desc.visible = true;
        desc.cullEnabled = true;
        desc.castShadow = true;
        desc.hasCollision = false;
        desc.displayName = String::ToWString(
            Build_InstancedMeshDisplayName(displayNameLabel, totalInstanceCount));

        for (size_t idx : groupIndices)
        {
            const json& entry = session.instancedMeshes[idx];
            if (!entry.value("Visible", true))     desc.visible = false;
            if (!entry.value("CullEnabled", true)) desc.cullEnabled = false;
            if (!entry.value("CastShadow", true))  desc.castShadow = false;
            if (entry.value("HasCollision", false)) desc.hasCollision = true;
        }

        desc.modelPath = String::ToWString(MapAdapter::ResolveMapModelPath(session.modelFolder, modelPathStr));

        auto it = session.preloadedModelCache.find(modelPathStr);
        if (it != session.preloadedModelCache.end())
        {
            desc.model = it->second;
            ApplySessionModelMaterials(session, modelPathStr, desc.model);
        }
        else
        {
            // 전역 프로토타입 캐시를 이용해 안전하게 모델 획득 및 갱신
            auto model = GetOrLoadModel(
                session.device,
                session.context,
                session.modelFolder,
                modelPathStr,
                session.modelOverrides,
                true,
                &session.modelCreateDuration,
                &session.modelCreateCount,
                &session.materialApplyDuration,
                &session.materialApplyCount);

            if (!model)
            {
                continue;
            }

            session.preloadedModelCache[modelPathStr] = model;
            MarkSessionModelMaterialsApplied(session, modelPathStr);
            desc.model = model;
        }

        // InstancedMeshes도 static mesh와 같은 collision cache를 재사용한다.
        const bool isCollisionFlagTrustedMap =
            session.sourceMapPath.find(L"SmallLevel_MF_Zone_01_Mk1") != wstring::npos;

        if (desc.hasCollision || !isCollisionFlagTrustedMap)
        {
            desc.collisionData = MapCollisionAdapter::GetOrCreateStaticCollision(
                MakeCollisionRuntimeContext(session),
                desc.model,
                modelPathStr);
        }

        const auto instancedTransformBegin = chrono::steady_clock::now();
        for (size_t idx : groupIndices)
        {
            const json& e = session.instancedMeshes[idx];
            if (!e.contains("Instances"))
            {
                continue;
            }

            for (const auto& inst : e["Instances"])
            {
                InstanceTransform t{};
                const auto& p = inst["Position"];
                t._position = Vec3(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
                const auto& r = inst["Rotation"];
                t._rotation = Vec4(r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>());
                const auto& s = inst["Scale"];
                t._scale = Vec3(s[0].get<float>(), s[1].get<float>(), s[2].get<float>());
                desc.transforms.push_back(t);
            }
        }
        session.instancedTransformDuration +=
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - instancedTransformBegin);

        static constexpr float CHUNK_SIZE = 200.f;

        map<pair<int32, int32>, vector<InstanceTransform>> chunkMap;
        for (const auto& t : desc.transforms)
        {
            int32 cx = static_cast<int32>(floorf(t._position.x / CHUNK_SIZE));
            int32 cz = static_cast<int32>(floorf(t._position.z / CHUNK_SIZE));
            chunkMap[{cx, cz}].push_back(t);
        }

        const auto instancedAddBegin = chrono::steady_clock::now();
        for (auto& [key, chunkTransforms] : chunkMap)
        {
            MapInstancedMesh::MAPINSTANCEDMESH_DESC chunkDesc{};
            chunkDesc.modelPath = desc.modelPath;
            chunkDesc.displayName = String::ToWString(
                Build_InstancedMeshChunkDisplayName(
                    displayNameLabel,
                    chunkTransforms.size(),
                    totalInstanceCount));
            chunkDesc.model = desc.model;
            chunkDesc.collisionData = desc.collisionData;
            chunkDesc.transforms = chunkTransforms;
            chunkDesc.visible = desc.visible;
            chunkDesc.cullEnabled = desc.cullEnabled;
            chunkDesc.castShadow = desc.castShadow;
            chunkDesc.cullDist = firstEntry.value("CullDist", 125.f);

            Shared<GameObject> spawnedObject = nullptr;
            const HRESULT addResult = GAME->Add_GameObject(
                ETOI(LevelType::Static),
                TEXT("GameObject_MapInstancedMesh"),
                session.levelIndex,
                session.layerTag,
                spawnedObject,
                &chunkDesc);

            if (FAILED(addResult))
                continue;

            if (session.spawnedObjects != nullptr && spawnedObject != nullptr)
                session.spawnedObjects->push_back(spawnedObject);

            ++session.instancedAddCount;
        }
        session.instancedAddDuration +=
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - instancedAddBegin);

        ++processedCount;
    }

    while (!session.prefabInstancesDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        if (session.nextPrefabInstanceIndex >= session.prefabInstances.size())
        {
            session.prefabInstancesDone = true;
            break;
        }

        const size_t sourcePrefabIndex = session.nextPrefabInstanceIndex++;
        json singleMapData = json::object();
        singleMapData["PrefabInstances"] = json::array(
            { session.prefabInstances[sourcePrefabIndex] });

        const auto prefabBegin = chrono::steady_clock::now();
        if (MapAuthoringIO::Load_PrefabInstances_FromMapData(
            singleMapData,
            session.levelIndex,
            session.spawnedObjects,
            session.sourceMapPath,
            sourcePrefabIndex))
        {
            ++session.prefabInstanceAddCount;
        }

        session.prefabInstanceAddDuration +=
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - prefabBegin);

        ++processedCount;
    }

    while (!session.authoredObjectsDone)
    {
        if (IsBudgetExceeded())
            return S_OK;

        if (session.nextAuthoredObjectIndex >= session.authoredObjects.size())
        {
            session.authoredObjectsDone = true;
            break;
        }

        const size_t sourceAuthoredIndex = session.nextAuthoredObjectIndex++;
        json singleMapData = json::object();
        singleMapData["AuthoredObjects"] = json::array(
            { session.authoredObjects[sourceAuthoredIndex] });

        const auto authoredBegin = chrono::steady_clock::now();
        if (MapAuthoringIO::Load_AuthoredObjects_FromMapData(
            singleMapData,
            session.levelIndex,
            session.spawnedObjects,
            session.sourceMapPath,
            0,
            sourceAuthoredIndex))
        {
            ++session.authoredObjectAddCount;
        }

        session.authoredObjectAddDuration +=
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - authoredBegin);

        ++processedCount;
    }

    outFinished =
        session.preloadModelsDone &&
        session.materialsDone &&
        session.staticMeshesDone &&
        session.instancedMeshesDone &&
        session.postProcessVolumesDone &&
        session.shaftVolumesDone &&
        session.watersDone &&
        session.lightsDone &&
        session.prefabInstancesDone &&
        session.authoredObjectsDone;

    if (outFinished && !session.summaryLogged)
    {
        LOG_DEBUG(
            "[MapLoader::Summary] processCalls={} begin={}ms models(data={}, createCalls={}, create={}ms, materials={}ms/{}) staticMeshes(data={}, add={}ms/{}, collisionBuild={}ms/{}) instancedMeshes(data={}, transforms={}ms, add={}ms/{}) prefabInstances(data={}, add={}ms/{}) authoredObjects(data={}, add={}ms/{}) autoInstanced(groups={}, statics={}) lights={}",
            session.processCallCount,
            session.beginSessionDuration.count(),
            session.preloadModelPaths.size(),
            session.modelCreateCount,
            session.modelCreateDuration.count(),
            session.materialApplyDuration.count(),
            session.materialApplyCount,
            session.staticMeshes.size(),
            session.staticAddDuration.count(),
            session.staticAddCount,
            session.collisionBuildDuration.count(),
            session.collisionBuildCount,
            session.instancedMeshes.size(),
            session.instancedTransformDuration.count(),
            session.instancedAddDuration.count(),
            session.instancedAddCount,
            session.prefabInstances.size(),
            session.prefabInstanceAddDuration.count(),
            session.prefabInstanceAddCount,
            session.authoredObjects.size(),
            session.authoredObjectAddDuration.count(),
            session.authoredObjectAddCount,
            session.autoInstancedGroupCount,
            session.autoInstancedStaticCount,
            session.lights.size());

        session.summaryLogged = true;
    }

    return S_OK;
}

void MapLoader::Prepare_StaticAutoInstancing(LoadSession& session)
{
    // 개별 하이어라키/피킹/충돌 확인이 우선이라 런타임 static auto-instancing은 잠시 끈다.
    session.nextStaticMeshIndex = 0;
    session.nextInstancedMeshIndex = 0;
    session.nextInstancedMeshGroupIndex = 0;
}

void MapLoader::ApplyMaterials(Shared<ModelCom>& model, const string& modelFolder, const string& modelFullPath, const ModelOverrideEntry* overrideEntry)
{
    if (model == nullptr)
        return;

    MapAdapter::ApplyMaterial(*model, MapAdapter::BuildMaterialApplyDesc(modelFolder, modelFullPath, overrideEntry));
}

string MapLoader::BuildMaterialSignature(const string& modelFolder, const string& modelFullPath, const ModelOverrideEntry* overrideEntry)
{
    string signature = String::ToLowerCopy(modelFolder);
    signature += '|';
    signature += String::ToLowerCopy(fs::path(modelFullPath).lexically_normal().generic_string());

    if (overrideEntry == nullptr)
        return signature;

    signature += "|override";

    vector<pair<int32, string>> textureSlots;
    textureSlots.reserve(overrideEntry->textureSlots.size());
    for (const auto& [slot, path] : overrideEntry->textureSlots)
        textureSlots.emplace_back(slot, path);

    sort(textureSlots.begin(), textureSlots.end(),
        [](const pair<int32, string>& lhs, const pair<int32, string>& rhs)
        {
            return lhs.first < rhs.first;
        });

    for (const auto& [slot, path] : textureSlots)
    {
        signature += '|';
        signature += std::to_string(slot);
        signature += '=';
        signature += String::ToLowerCopy(path);
    }

    if (!overrideEntry->modelJson.is_null() && !overrideEntry->modelJson.empty())
    {
        signature += "|json=";
        signature += overrideEntry->modelJson.dump();
    }

    return signature;
}

void MapLoader::MarkSessionModelMaterialsApplied(LoadSession& session, const string& modelPathStr)
{
    if (modelPathStr.empty())
        return;

    if (find(session.materialAppliedModelPaths.begin(), session.materialAppliedModelPaths.end(), modelPathStr) !=
        session.materialAppliedModelPaths.end())
    {
        return;
    }

    session.materialAppliedModelPaths.push_back(modelPathStr);
}

void MapLoader::MarkGlobalModelMaterialsApplied(
    const string& modelFolder,
    const string& modelPathStr,
    const unordered_map<string, ModelOverrideEntry>& modelOverrides)
{
    if (modelPathStr.empty())
        return;

    const string fullPathA = MapAdapter::ResolveMapModelPath(modelFolder, modelPathStr);
    const string cacheKey = String::ToLowerCopy(fs::path(fullPathA).lexically_normal().generic_string());
    const ModelOverrideEntry* overrideEntry = Find_ModelOverrideEntry(modelOverrides, modelPathStr);
    const string materialSignature = BuildMaterialSignature(modelFolder, fullPathA, overrideEntry);

    std::lock_guard<std::mutex> lock(g_globalCacheMutex);
    g_globalMaterialSignatureByModel[cacheKey] = materialSignature;
}

void MapLoader::ApplySessionModelMaterials(LoadSession& session, const string& modelPathStr, Shared<ModelCom>& model)
{
    if (model == nullptr || modelPathStr.empty())
        return;

    if (find(session.materialAppliedModelPaths.begin(), session.materialAppliedModelPaths.end(), modelPathStr) !=
        session.materialAppliedModelPaths.end())
    {
        return;
    }

    const string fullPathA = MapAdapter::ResolveMapModelPath(session.modelFolder, modelPathStr);
    const ModelOverrideEntry* overrideEntry = Find_ModelOverrideEntry(session.modelOverrides, modelPathStr);

    const auto materialBegin = chrono::steady_clock::now();
    ApplyMaterials(model, session.modelFolder, fullPathA, overrideEntry);
    session.materialApplyDuration +=
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - materialBegin);
    ++session.materialApplyCount;

    MarkSessionModelMaterialsApplied(session, modelPathStr);
    MarkGlobalModelMaterialsApplied(session.modelFolder, modelPathStr, session.modelOverrides);
}

HRESULT MapLoader::Load(const ComPtr<Device>& device, const ComPtr<Context>& context, const wstring& filePath, uint32 levelIndex, const wstring& layerTag)
{
    LoadSession session{};
    CHECK_FAILED(Begin_LoadSession(device, context, filePath, levelIndex, layerTag, session), E_FAIL);

    bool finished = false;
    while (!finished)
        CHECK_FAILED(Process_LoadSession(session, SIZE_MAX, chrono::milliseconds(0), finished), E_FAIL);

    return S_OK;
}

HRESULT MapLoader::Convert_Json_To_Bin(const wstring& jsonPath, const wstring& binPath)
{
    string jsonPathA = String::ToString(jsonPath);
    ifstream ifs(jsonPathA);
    if (!ifs.is_open())
        return E_FAIL;

    json root;
    try
    {
        root = json::parse(ifs);

        if (root.contains("MapData") && root["MapData"].contains("ModelCollisions"))
        {
            auto& modelCollisions = root["MapData"]["ModelCollisions"];
            for (auto it = modelCollisions.begin(); it != modelCollisions.end(); ++it)
            {
                auto& collisionEntry = it.value();
                if (collisionEntry.is_array()) // 구포맷 유지
                {
                    json binaryMeshArray = json::array();
                    for (auto& base64Mesh : collisionEntry)
                    {
                        if (base64Mesh.is_string())
                        {
                            string base64Str = base64Mesh.get<string>();
                            vector<uint8_t> binaryData = String::Base64_Decode(base64Str);
                            binaryMeshArray.push_back(json::binary(binaryData));
                        }
                    }
                    modelCollisions[it.key()] = binaryMeshArray;
                }
                else if (collisionEntry.is_object()) // 신포맷은 cooked triangle mesh만 binary로 바꾸고 primitive는 그대로 둔다.
                {
                    if (!collisionEntry.contains("CookedTriangleMeshes") ||
                        !collisionEntry["CookedTriangleMeshes"].is_array())
                    {
                        continue;
                    }

                    json binaryMeshArray = json::array();
                    for (auto& base64Mesh : collisionEntry["CookedTriangleMeshes"])
                    {
                        if (base64Mesh.is_string())
                        {
                            string base64Str = base64Mesh.get<string>();
                            vector<uint8_t> binaryData = String::Base64_Decode(base64Str);
                            binaryMeshArray.push_back(json::binary(binaryData));
                        }
                        else if (base64Mesh.is_binary())
                        {
                            binaryMeshArray.push_back(base64Mesh);
                        }
                    }

                    collisionEntry["CookedTriangleMeshes"] = std::move(binaryMeshArray);
                }
            }
        }
    }
    catch (...)
    {
        return E_FAIL;
    }
    ifs.close();

    string binPathA = String::ToString(binPath);
    ofstream ofs(binPathA, ios::binary);
    if (!ofs.is_open())
        return E_FAIL;

    try
    {
        vector<uint8_t> msgpack = json::to_msgpack(root);
        ofs.write(reinterpret_cast<const char*>(msgpack.data()), msgpack.size());
    }
    catch (...)
    {
        return E_FAIL;
    }
    ofs.close();

    return S_OK;
}

HRESULT MapLoader::Convert_MapInstance_To_Bin(const wstring& mapInstanceRoot)
{
    fs::path root(mapInstanceRoot);
    if (!fs::exists(root) || !fs::is_directory(root))
    {
        return E_FAIL;
    }

    const vector<pair<string, function<bool(const json&)>>> subfolders = {
        { "StaticMesh", nullptr },
        { "InstanceMesh", nullptr },
        { "PostProcessVolume", nullptr },
        { "ShaftVolume", nullptr },
        { "Water", nullptr },
        { "ETC", [](const json& j) { return j.value("LightType", "Directional") == "Point"; } },
    };

    for (const auto& [folderName, filter] : subfolders)
    {
        fs::path dir = root / folderName;
        if (!fs::exists(dir))
        {
            continue;
        }

        json arr = json::array();

        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            ifstream ifs(entry.path());
            if (!ifs.is_open())
            {
                continue;
            }

            try
            {
                json item = json::parse(ifs);
                if (!item.is_object())
                {
                    continue;
                }

                if (filter && !filter(item))
                {
                    continue;
                }

                arr.push_back(std::move(item));
            }
            catch (...) {}
        }

        fs::path binPath = root / (folderName + ".bin");
        ofstream ofs(binPath, ios::binary);
        if (!ofs.is_open())
        {
            continue;
        }

        try
        {
            vector<uint8_t> msgpack = json::to_msgpack(arr);
            ofs.write(reinterpret_cast<const char*>(msgpack.data()), msgpack.size());
        }
        catch (...) { return E_FAIL; }
    }

    return S_OK;
}

Shared<ModelCom> MapLoader::GetOrLoadModel(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const string& modelFolder,
    const string& modelPathStr,
    const unordered_map<string, ModelOverrideEntry>& modelOverrides,
    bool applyMaterials,
    chrono::milliseconds* outCreateDuration,
    size_t* outCreateCount,
    chrono::milliseconds* outMaterialDuration,
    size_t* outMaterialCount)
{
    // modelPathStr은 두 형태를 모두 지원한다.
    // 1) Map 루트 기준 상대 경로: CustomModel/U_BedRock.model
    // 2) 구버전 파일명-only 경로: U_BedRock.model + modelFolder fallback
    string fullPathA = MapAdapter::ResolveMapModelPath(modelFolder, modelPathStr);

    const string cacheKey = String::ToLowerCopy(fs::path(fullPathA).lexically_normal().generic_string());
    const ModelOverrideEntry* overrideEntry = Find_ModelOverrideEntry(modelOverrides, modelPathStr);
    const string materialSignature = BuildMaterialSignature(modelFolder, fullPathA, overrideEntry);

    // 1단계: 스레드 안전하게 전역 캐시에서 동일 모델 존재 확인
    Shared<ModelCom> cachedModel = nullptr;
    bool needsMaterialApply = true;
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        if (g_globalMissingModelCache.find(cacheKey) != g_globalMissingModelCache.end())
            return nullptr;

        auto it = g_globalModelCache.find(cacheKey);
        if (it != g_globalModelCache.end())
        {
            cachedModel = it->second;

            const auto materialIt = g_globalMaterialSignatureByModel.find(cacheKey);
            if (materialIt != g_globalMaterialSignatureByModel.end() && materialIt->second == materialSignature)
                needsMaterialApply = false;
        }
    }

    if (cachedModel != nullptr)
    {
        DebugMemoryTracker::Record_CPUBytes(
            "map.model.cache_hit",
            "MapLoader.GetOrLoadModel",
            fs::path(fullPathA).wstring(),
            0);

        if (applyMaterials && needsMaterialApply)
        {
            const auto materialBegin = chrono::steady_clock::now();
            ApplyMaterials(cachedModel, modelFolder, fullPathA, overrideEntry);
            const auto materialMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - materialBegin);

            if (outMaterialDuration) *outMaterialDuration += materialMs;
            if (outMaterialCount) (*outMaterialCount)++;

            MarkGlobalModelMaterialsApplied(modelFolder, modelPathStr, modelOverrides);
        }

        return cachedModel;
    }

    if (!fs::exists(fs::path(fullPathA)))
    {
        {
            std::lock_guard<std::mutex> lock(g_globalCacheMutex);
            g_globalMissingModelCache.insert(cacheKey);
        }

        LOG_WARN(
            "[MapLoader::Model] skip missing model modelFolder='{}' modelPath='{}' resolved='{}'",
            modelFolder,
            modelPathStr,
            fullPathA);
        return nullptr;
    }

    // 2단계: 전역 캐시에 없는 경우 디스크에서 신규 로드 수행
    const auto createBegin = chrono::steady_clock::now();
    auto model = ModelCom::Create(
        device,
        context,
        ModelType::NonAnim,
        fullPathA.c_str(),
        Matrix::CreateScale(0.01f));
    const auto createMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - createBegin);

    if (outCreateDuration) *outCreateDuration += createMs;
    if (outCreateCount) (*outCreateCount)++;

    if (!model)
    {
        {
            std::lock_guard<std::mutex> lock(g_globalCacheMutex);
            g_globalMissingModelCache.insert(cacheKey);
        }

        LOG_WARN(
            "[MapLoader::Model] failed to create model modelFolder='{}' modelPath='{}' resolved='{}' directExists={}",
            modelFolder,
            modelPathStr,
            fullPathA,
            fs::exists(fs::path(fullPathA)));
        return nullptr;
    }

    if (applyMaterials)
    {
        const auto materialBegin = chrono::steady_clock::now();
        ApplyMaterials(model, modelFolder, fullPathA, overrideEntry);
        const auto materialMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - materialBegin);

        if (outMaterialDuration) *outMaterialDuration += materialMs;
        if (outMaterialCount) (*outMaterialCount)++;

        MarkGlobalModelMaterialsApplied(modelFolder, modelPathStr, modelOverrides);
    }

    // 3단계: 로드가 완료된 새 모델을 전역 캐시에 안전하게 등록
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        g_globalModelCache[cacheKey] = model;
    }

    size_t meshBytes = 0;
    for (const Shared<Mesh>& mesh : model->Get_Meshes())
    {
        if (mesh)
            meshBytes += mesh->Get_DebugEstimatedBufferBytes();
    }

    DebugMemoryTracker::Record_CPUBytes(
        "map.model.loaded",
        "MapLoader.GetOrLoadModel",
        fs::path(fullPathA).wstring(),
        max<size_t>(1, meshBytes));

    return model;
}

void MapLoader::Clear_GlobalCache()
{
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        g_globalModelCache.clear();
        g_globalMissingModelCache.clear();
        g_globalCollisionCache.clear();
        g_globalMaterialSignatureByModel.clear();
    }
    MapAdapter::Clear_MaterialCache();
    Clear_CachedLights();
}

void MapLoader::Clear_CachedLights()
{
    g_cachedLights.clear();
}

void MapLoader::Reapply_CachedLights()
{
    for (const auto& desc : g_cachedLights)
    {
        if (desc.type == LightType::Point)
        {
            GAME->Add_PointLight(
                Vec3(desc.position.x, desc.position.y, desc.position.z),
                desc.range,
                desc.diffuse,
                desc.ambient,
                desc.specular,
                desc.pointAttenuationPower);
        }
        else
        {
            GAME->Add_Light(desc);
        }
    }
}
