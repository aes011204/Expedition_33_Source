#include "MapLoader.h"
#include "GameInstance.h"
#include "MapAuthoringIO.h"
#include "MapStaticMesh.h"
#include "MapInstancedMesh.h"
#include "MapStaticCollisionBuilder.h"
#include "ModelCom.h"
#include "StaticCollisionData.h"
#include "Shader_CBuffer_Define.h"
#include <mutex>

// 전역 모델 및 콜리전 캐시 정적 멤버 정의 및 초기화
std::mutex MapLoader::g_globalCacheMutex;
umap<string, Shared<ModelCom>> MapLoader::g_globalModelCache;
umap<string, Shared<StaticCollisionData>> MapLoader::g_globalCollisionCache;
umap<string, std::vector<std::pair<uint32, std::string>>> MapLoader::g_globalMaterialBindingCache;
uint32 MapLoader::g_currentLevelIndex = UINT32_MAX;

namespace
{
    bool Try_ParseCollisionProxy(const json& node, ModelCollisionProxy& outProxy)
    {
        if (!node.is_object())
            return false;

        outProxy = {};
        outProxy.type = static_cast<ModelCollisionProxyType>(
            node.value("Type", static_cast<uint32>(ModelCollisionProxyType::TriangleMesh)));
        outProxy.name = node.value("Name", string{});

        if (node.contains("Center") && node["Center"].is_array() && node["Center"].size() == 3)
        {
            outProxy.center = Vec3(
                node["Center"][0].get<float>(),
                node["Center"][1].get<float>(),
                node["Center"][2].get<float>());
        }

        if (node.contains("Rotation") && node["Rotation"].is_array() && node["Rotation"].size() == 4)
        {
            outProxy.rotation = Quat(
                node["Rotation"][0].get<float>(),
                node["Rotation"][1].get<float>(),
                node["Rotation"][2].get<float>(),
                node["Rotation"][3].get<float>());
        }

        if (node.contains("Extents") && node["Extents"].is_array() && node["Extents"].size() == 3)
        {
            outProxy.extents = Vec3(
                node["Extents"][0].get<float>(),
                node["Extents"][1].get<float>(),
                node["Extents"][2].get<float>());
        }

        outProxy.radius = node.value("Radius", 0.f);
        outProxy.halfHeight = node.value("HalfHeight", 0.f);

        if (node.contains("Positions") && node["Positions"].is_array())
        {
            for (const json& positionNode : node["Positions"])
            {
                if (!positionNode.is_array() || positionNode.size() != 3)
                    continue;

                outProxy.positions.push_back(Vec3(
                    positionNode[0].get<float>(),
                    positionNode[1].get<float>(),
                    positionNode[2].get<float>()));
            }
        }

        if (node.contains("Indices") && node["Indices"].is_array())
        {
            for (const json& indexNode : node["Indices"])
                outProxy.indices.push_back(indexNode.get<uint32>());
        }

        return true;
    }
}

HRESULT MapLoader::Begin_LoadSession(const ComPtr<Device>& device, const ComPtr<Context>& context,
    const wstring& filePath, uint32 levelIndex, const wstring& layerTag, LoadSession& outSession)
{
    // 새로운 레벨로 진입한 경우 기존 전역 캐시 클리어 (메모리 누수 방지)
    if (g_currentLevelIndex != levelIndex)
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        g_globalModelCache.clear();
        g_globalCollisionCache.clear();
        g_globalMaterialBindingCache.clear();
        g_currentLevelIndex = levelIndex;
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
    outSession.modelFolder = mapData.value("ModelFolder", string{});

    if (mapData.contains("PostProcessVolumes"))
    {
        FogZoneCB fogCB{};
        for (const auto& e : mapData["PostProcessVolumes"])
        {
            if (fogCB.aabbCount + fogCB.sphereCount >= FogZoneCB::MAX_FOG_ZONES)
            {
                break;
            }

            float density = e.value("FogDensity", 0.f);
            if (density <= 0.f)
            {
                continue;
            }

            FogZoneEntry entry{};
            if (e.contains("FogColor"))
            {
                auto& c = e["FogColor"];
                entry.fogColor = Vec4(c[0], c[1], c[2], 1.f);
            }

            entry.fogDensity = density;
            entry.fogStart = e.value("FogStart", 0.f);

            Vec3 position{};
            if (e.contains("Position"))
            {
                auto& p = e["Position"];
                position = Vec3(p[0], p[1], p[2]);
            }

            int zoneType = e.value("FogZoneType", 0);
            if (zoneType == 0) // AABB
            {
                Vec3 extent{ 5.f, 5.f, 5.f };
                if (e.contains("Extent"))
                {
                    auto& ex = e["Extent"];
                    extent = Vec3(ex[0], ex[1], ex[2]);
                }
                entry.zoneMin = position - extent;
                entry.zoneMax = position + extent;
                fogCB.zones[fogCB.aabbCount++] = entry;
            }
            else // sphere
            {
                entry.zoneMin = position;
                entry.zoneRadius = e.value("FogRadius", 10.f);
                fogCB.zones[fogCB.aabbCount + fogCB.sphereCount++] = entry;
            }
        }
        GAME->Set_FogZones(fogCB);
    }

    if (mapData.contains("ModelOverrides"))
    {
        for (auto& [filename, entry] : mapData["ModelOverrides"].items())
        {
            outSession.modelOverrides[filename] = entry.value("PassIndex", -1);
        }
    }

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

    if (mapData.contains("ModelCollisions") && mapData["ModelCollisions"].is_object())
    {
        for (const auto& [modelPath, collisionEntry] : mapData["ModelCollisions"].items())
        {
            vector<vector<uint8_t>> cookedMeshes;
            vector<ModelCollisionProxy> primitiveProxies;

            if (collisionEntry.is_array()) // 구포맷: cooked triangle mesh만 배열로 저장되어 있다.
            {
                cookedMeshes.reserve(collisionEntry.size());

                for (const json& meshData : collisionEntry)
                {
                    if (meshData.is_binary())
                    {
                        const auto& binaryData = meshData.get_binary();
                        cookedMeshes.emplace_back(binaryData.begin(), binaryData.end());
                    }
                    else if (meshData.is_string())
                    {
                        cookedMeshes.push_back(String::Base64_Decode(meshData.get<string>()));
                    }
                }
            }
            else if (collisionEntry.is_object()) // 신포맷: cooked triangle + primitive proxy를 함께 저장한다.
            {
                if (collisionEntry.contains("CookedTriangleMeshes") &&
                    collisionEntry["CookedTriangleMeshes"].is_array())
                {
                    const json& meshArray = collisionEntry["CookedTriangleMeshes"];
                    cookedMeshes.reserve(meshArray.size());

                    for (const json& meshData : meshArray)
                    {
                        if (meshData.is_binary())
                        {
                            const auto& binaryData = meshData.get_binary();
                            cookedMeshes.emplace_back(binaryData.begin(), binaryData.end());
                        }
                        else if (meshData.is_string())
                        {
                            cookedMeshes.push_back(String::Base64_Decode(meshData.get<string>()));
                        }
                    }
                }

                if (collisionEntry.contains("PrimitiveProxies") &&
                    collisionEntry["PrimitiveProxies"].is_array())
                {
                    for (const json& proxyNode : collisionEntry["PrimitiveProxies"])
                    {
                        ModelCollisionProxy proxy{};
                        if (Try_ParseCollisionProxy(proxyNode, proxy))
                            primitiveProxies.push_back(std::move(proxy));
                    }
                }
            }

            if (!cookedMeshes.empty())
                outSession.cookedCollisionCache[modelPath] = std::move(cookedMeshes);

            if (!primitiveProxies.empty())
                outSession.primitiveCollisionCache[modelPath] = std::move(primitiveProxies);
        }
    }

    outSession.preloadModelsDone = outSession.preloadModelPaths.empty();
    outSession.staticMeshesDone = outSession.staticMeshes.empty();
    outSession.instancedMeshesDone = outSession.instancedMeshes.empty();
    outSession.prefabInstancesDone = outSession.prefabInstances.empty();
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
            outSession.modelOverrides);

        if (!model)
        {
            continue;
        }

        outSession.preloadedModelCache[modelPathStr] = model;

        // 다른 스레드(특히 메인 스레드)에 CPU 사용권 양보
        this_thread::yield();
    }

    // 2.5. 정적 콜리전 데이터 백그라운드 사전 빌드 (백그라운드 스레드)
    for (const auto& e : vector<json>{})
    {
        const string modelPathStr = e.value("ModelPath", string{});
        if (modelPathStr.empty())
            continue;

        if (outSession.preloadedCollisionCache.find(modelPathStr) != outSession.preloadedCollisionCache.end())
            continue;

        auto it = outSession.preloadedModelCache.find(modelPathStr);
        if (it != outSession.preloadedModelCache.end() && it->second != nullptr)
        {
            // 전역 캐시 및 안전 빌드 헬퍼 활용
            Shared<StaticCollisionData> collisionData = GetOrBuildCollision(it->second, modelPathStr);
            outSession.preloadedCollisionCache[modelPathStr] = collisionData;
        }

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
            &session.modelCreateDuration,
            &session.modelCreateCount,
            &session.materialApplyDuration,
            &session.materialApplyCount);

        if (!model)
        {
            continue;
        }

        session.preloadedModelCache[modelPathStr] = model;

        ++processedCount;
    }

    if (!session.staticAutoInstancingPrepared)
    {
        Prepare_StaticAutoInstancing(session);
        session.staticAutoInstancingPrepared = true;
        session.staticMeshesDone = session.staticMeshes.empty();
        session.instancedMeshesDone = session.instancedMeshes.empty();
        session.prefabInstancesDone = session.prefabInstances.empty();
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

        const json& e = session.staticMeshes[session.nextStaticMeshIndex++];
        const string modelPathStr = e.value("ModelPath", string{});
        if (modelPathStr.empty())
            continue;

        MapStaticMesh::MAPSTATICMESH_DESC desc{};
        desc.name = String::ToWString(e.value("Id", string{}));

        auto it = session.preloadedModelCache.find(modelPathStr);
        if (it != session.preloadedModelCache.end())
        {
            desc.model = it->second;
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
                &session.modelCreateDuration,
                &session.modelCreateCount,
                &session.materialApplyDuration,
                &session.materialApplyCount);

            if (!model)
            {
                continue;
            }

            session.preloadedModelCache[modelPathStr] = model;
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
            desc.collisionData = GetOrCreateCookedCollision(session, desc.model, modelPathStr);
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

        if (session.nextInstancedMeshIndex >= session.instancedMeshes.size())
        {
            session.instancedMeshesDone = true;
            break;
        }

        const json& e = session.instancedMeshes[session.nextInstancedMeshIndex++];
        const string modelPathStr = e.value("ModelPath", string{});
        if (modelPathStr.empty())
            continue;

        MapInstancedMesh::MAPINSTANCEDMESH_DESC desc{};

        auto it = session.preloadedModelCache.find(modelPathStr);
        if (it != session.preloadedModelCache.end())
        {
            desc.model = it->second;
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
                &session.modelCreateDuration,
                &session.modelCreateCount,
                &session.materialApplyDuration,
                &session.materialApplyCount);

            if (!model)
            {
                continue;
            }

            session.preloadedModelCache[modelPathStr] = model;
            desc.model = model;
        }

        // InstancedMeshes도 static mesh와 같은 collision cache를 재사용한다.
        desc.collisionData = GetOrCreateCookedCollision(session, desc.model, modelPathStr);

        if (e.contains("Instances"))
        {
            const auto instancedTransformBegin = chrono::steady_clock::now();
            for (const auto& inst : e["Instances"])
            {
                InstanceTransform t{};

                const auto& p = inst["Position"];
                t._position = Vec3(
                    p[0].get<float>(),
                    p[1].get<float>(),
                    p[2].get<float>());

                const auto& r = inst["Rotation"];
                t._rotation = Vec4(
                    r[0].get<float>(),
                    r[1].get<float>(),
                    r[2].get<float>(),
                    r[3].get<float>());

                const auto& s = inst["Scale"];
                t._scale = Vec3(
                    s[0].get<float>(),
                    s[1].get<float>(),
                    s[2].get<float>());

                desc.transforms.push_back(t);
            }
            session.instancedTransformDuration +=
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - instancedTransformBegin);
        }

        Shared<GameObject> spawnedObject = nullptr;

        const auto instancedAddBegin = chrono::steady_clock::now();
        const HRESULT addResult = GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_MapInstancedMesh"),
            session.levelIndex,
            session.layerTag,
            spawnedObject,
            &desc);
        const auto instancedAddMs =
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - instancedAddBegin);
        session.instancedAddDuration += instancedAddMs;
        ++session.instancedAddCount;

        if (FAILED(addResult))
        {
            continue;
        }
        if (session.spawnedObjects != nullptr && spawnedObject != nullptr)
        {
            session.spawnedObjects->push_back(spawnedObject);
        }

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

        json singleMapData = json::object();
        singleMapData["PrefabInstances"] = json::array(
            { session.prefabInstances[session.nextPrefabInstanceIndex++] });

        const auto prefabBegin = chrono::steady_clock::now();
        if (MapAuthoringIO::Load_PrefabInstances_FromMapData(
            singleMapData,
            session.levelIndex,
            session.spawnedObjects))
        {
            ++session.prefabInstanceAddCount;
        }

        session.prefabInstanceAddDuration +=
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - prefabBegin);

        ++processedCount;
    }

    outFinished =
        session.preloadModelsDone &&
        session.staticMeshesDone &&
        session.instancedMeshesDone &&
        session.prefabInstancesDone;

    if (outFinished && !session.summaryLogged)
    {
        LOG_DEBUG(
            "[MapLoader::Summary] processCalls={} begin={}ms models(data={}, createCalls={}, create={}ms, materials={}ms/{}) staticMeshes(data={}, add={}ms/{}, collisionBuild={}ms/{}) instancedMeshes(data={}, transforms={}ms, add={}ms/{}) prefabInstances(data={}, add={}ms/{}) autoInstanced(groups={}, statics={})",
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
            session.autoInstancedGroupCount,
            session.autoInstancedStaticCount);

        session.summaryLogged = true;
    }

    return S_OK;
}

void MapLoader::Prepare_StaticAutoInstancing(LoadSession& session)
{
    // 개별 하이어라키/피킹/충돌 확인이 우선이라 런타임 static auto-instancing은 잠시 끈다.
    session.nextStaticMeshIndex = 0;
    session.nextInstancedMeshIndex = 0;
}

void MapLoader::ApplyMaterials(Shared<ModelCom>& model, const string& modelFolder, const string& modelFullPath, int32 forcedPassIndex)
{
    if (model == nullptr)
    {
        return;
    }

    if (modelFolder.empty())
    {
        return;
    }

    string bundleKey = modelFolder;
    transform(bundleKey.begin(), bundleKey.end(), bundleKey.begin(), ::tolower);

    fs::path jsonPath = fs::path(modelFullPath).replace_extension(".json");
    const string jsonPathKey = jsonPath.generic_string();

    using MaterialSlotBinding = pair<uint32, string>;
    
    vector<MaterialSlotBinding> bindings;
    bool isCached = false;
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        auto cacheIt = g_globalMaterialBindingCache.find(jsonPathKey);
        if (cacheIt != g_globalMaterialBindingCache.end())
        {
            bindings = cacheIt->second;
            isCached = true;
        }
    }

    if (!isCached)
    {
        ifstream jfs(jsonPath);

        try
        {
            if (jfs.is_open())
            {
                json modelJson = json::parse(jfs);

                for (auto& entry : modelJson)
                {
                    if (entry.value("Type", "") != "StaticMesh")
                        continue;

                    if (!entry.contains("Properties") ||
                        !entry["Properties"].contains("StaticMaterials"))
                    {
                        break;
                    }

                    const auto& staticMats = entry["Properties"]["StaticMaterials"];

                    for (uint32 i = 0; i < static_cast<uint32>(staticMats.size()); ++i)
                    {
                        const auto& mat = staticMats[i];
                        if (!mat.contains("MaterialInterface"))
                            continue;

                        const auto& mi = mat["MaterialInterface"];
                        if (!mi.is_object())
                            continue;

                        string objectPath = mi.value("ObjectPath", "");
                        if (objectPath.empty())
                            continue;

                        size_t dot = objectPath.find('.');
                        string packagePath = (dot != string::npos)
                            ? objectPath.substr(0, dot)
                            : objectPath;

                        size_t slash = packagePath.rfind('/');
                        string miName = (slash != string::npos)
                            ? packagePath.substr(slash + 1)
                            : packagePath;

                        bindings.emplace_back(i, miName);
                    }

                    break;
                }
            }
        }
        catch (...)
        {
        }

        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        g_globalMaterialBindingCache[jsonPathKey] = bindings;
    }

    for (const MaterialSlotBinding& binding : bindings)
    {
        model->Apply_MaterialByAssetName(binding.first, bundleKey, binding.second);
    }

    if (forcedPassIndex >= 0)
    {
        for (uint32 i = 0; i < static_cast<uint32>(model->Get_NumMaterials()); ++i)
        {
            auto mat = model->Get_Material(i);
            if (mat)
            {
                mat->Set_ForcedPassIndex(forcedPassIndex);
            }
        }
    }
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

Shared<ModelCom> MapLoader::GetOrLoadModel(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const string& modelFolder,
    const string& modelPathStr,
    const unordered_map<string, int32>& modelOverrides,
    chrono::milliseconds* outCreateDuration,
    size_t* outCreateCount,
    chrono::milliseconds* outMaterialDuration,
    size_t* outMaterialCount)
{
    // 1단계: 스레드 안전하게 전역 캐시에서 동일 모델 존재 확인
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        auto it = g_globalModelCache.find(modelPathStr);
        if (it != g_globalModelCache.end())
        {
            return it->second;
        }
    }

    // 2단계: 전역 캐시에 없는 경우 디스크에서 신규 로드 수행
    string fullPathA = "../../../Client/Bin/Resources/Map/" + modelFolder + "/" + modelPathStr;

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
        return nullptr;

    int32 forcedPass = -1;
    auto overrideIt = modelOverrides.find(modelPathStr);
    if (overrideIt != modelOverrides.end())
    {
        forcedPass = overrideIt->second;
    }

    // 머티리얼 적용 로직 수행
    const auto materialBegin = chrono::steady_clock::now();
    ApplyMaterials(model, modelFolder, fullPathA, forcedPass);
    const auto materialMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - materialBegin);

    if (outMaterialDuration) *outMaterialDuration += materialMs;
    if (outMaterialCount) (*outMaterialCount)++;

    // 3단계: 로드가 완료된 새 모델을 전역 캐시에 안전하게 등록
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        g_globalModelCache[modelPathStr] = model;
    }

    return model;
}

Shared<StaticCollisionData> MapLoader::GetOrCreateCookedCollision(
    LoadSession& session,
    const Shared<ModelCom>& model,
    const string& modelPathStr)
{
    if (model == nullptr || modelPathStr.empty())
        return nullptr;

    auto ensureTriangleMeshes = [&](const Shared<StaticCollisionData>& collisionData)
    {
        if (collisionData == nullptr)
            return;

        if (!collisionData->cookedTriangleMeshes.empty() || !collisionData->triangleMeshes.empty())
            return;

        const wstring sourceModelPath = !model->Get_SourceModelPath().empty()
            ? model->Get_SourceModelPath()
            : String::ToWString(modelPathStr);

        if (sourceModelPath.empty())
            return;

        // 저장된 collision이 proxy-only 또는 empty여도 gameplay에서는 .model triangle mesh를 우선 보강한다.
        MapStaticCollisionBuilder::Build_TriangleMeshes_FromModelFile(
            sourceModelPath,
            collisionData->triangleMeshes);
    };

    auto sessionIt = session.preloadedCollisionCache.find(modelPathStr);
    if (sessionIt != session.preloadedCollisionCache.end())
    {
        ensureTriangleMeshes(sessionIt->second);
        return sessionIt->second;
    }

    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        auto globalIt = g_globalCollisionCache.find(modelPathStr);
        if (globalIt != g_globalCollisionCache.end())
        {
            ensureTriangleMeshes(globalIt->second);
            session.preloadedCollisionCache[modelPathStr] = globalIt->second;
            return globalIt->second;
        }
    }

    auto cookedIt = session.cookedCollisionCache.find(modelPathStr);
    auto primitiveIt = session.primitiveCollisionCache.find(modelPathStr);
    const bool hasCollisionEntry =
        (cookedIt != session.cookedCollisionCache.end() || primitiveIt != session.primitiveCollisionCache.end());

    const bool hasCookedTriangles =
        (cookedIt != session.cookedCollisionCache.end() && !cookedIt->second.empty());
    const bool hasPrimitiveProxies =
        (primitiveIt != session.primitiveCollisionCache.end() && !primitiveIt->second.empty());

    if (!hasCollisionEntry)
    {
        // 저장 collision entry가 아예 없는 모델도 런타임에서 triangle/proxy collision을 보강한다.
        Shared<StaticCollisionData> collisionData = GetOrBuildCollision(
            model,
            modelPathStr,
            &session.collisionBuildDuration,
            &session.collisionBuildCount);

        if (collisionData != nullptr)
            session.preloadedCollisionCache[modelPathStr] = collisionData;

        return collisionData;
    }

    auto collisionData = make_shared<StaticCollisionData>();
    collisionData->cacheKey = model->Get_SourceModelPath();
    if (collisionData->cacheKey.empty())
        collisionData->cacheKey = String::ToWString(modelPathStr);
    collisionData->preLocalTransform = model->Get_PreLocalTransformMatrix();
    if (hasCookedTriangles)
        collisionData->cookedTriangleMeshes = cookedIt->second;
    if (hasPrimitiveProxies)
        collisionData->primitiveProxies = primitiveIt->second;
    ensureTriangleMeshes(collisionData);

    if (collisionData->cookedTriangleMeshes.empty() &&
        collisionData->primitiveProxies.empty() &&
        collisionData->triangleMeshes.empty())
    {
        return nullptr;
    }

    session.preloadedCollisionCache[modelPathStr] = collisionData;

    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        g_globalCollisionCache[modelPathStr] = collisionData;
    }

    return collisionData;
}

Shared<StaticCollisionData> MapLoader::GetOrBuildCollision(
    const Shared<ModelCom>& model,
    const string& modelPathStr,
    chrono::milliseconds* outDuration,
    size_t* outCount)
{
    if (model == nullptr)
        return nullptr;

    // 1단계: 전역 콜리전 캐시에서 조회
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        auto it = g_globalCollisionCache.find(modelPathStr);
        if (it != g_globalCollisionCache.end())
        {
            return it->second;
        }
    }

    // 2단계: 캐시 미스 시 신규 콜리전 데이터 빌드 수행
    const auto begin = chrono::steady_clock::now();
    Shared<StaticCollisionData> collisionData = MapStaticCollisionBuilder::Build(model);
    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - begin);

    if (outDuration) *outDuration += elapsed;
    if (outCount) (*outCount)++;

    // 3단계: 빌드된 콜리전 데이터를 전역 캐시에 스레드 안전하게 등록
    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        g_globalCollisionCache[modelPathStr] = collisionData;
    }

    return collisionData;
}

void MapLoader::Clear_GlobalCache()
{
    std::lock_guard<std::mutex> lock(g_globalCacheMutex);
    g_globalModelCache.clear();
    g_globalCollisionCache.clear();
    g_globalMaterialBindingCache.clear();
}
