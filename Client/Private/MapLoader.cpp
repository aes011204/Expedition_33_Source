#include "MapLoader.h"
#include "GameInstance.h"
#include "MapStaticMesh.h"
#include "MapInstancedMesh.h"
#include "MapStaticCollisionBuilder.h"
#include "ModelCom.h"
#include "StaticCollisionData.h"
#include <mutex>

// 전역 모델 및 콜리전 캐시 정적 멤버 정의 및 초기화
std::mutex MapLoader::g_globalCacheMutex;
umap<string, Shared<ModelCom>> MapLoader::g_globalModelCache;
umap<string, Shared<StaticCollisionData>> MapLoader::g_globalCollisionCache;
umap<string, std::vector<std::pair<uint32, std::string>>> MapLoader::g_globalMaterialBindingCache;
uint32 MapLoader::g_currentLevelIndex = UINT32_MAX;

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
            actualPath = binPath;
            pathA = binPathA;
            isBinary = true;
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

    if (mapData.contains("ModelCollisions") && mapData["ModelCollisions"].is_object())
    {
        for (const auto& [modelPath, meshArray] : mapData["ModelCollisions"].items())
        {
            if (!meshArray.is_array())
                continue;

            vector<vector<uint8_t>> cookedMeshes;
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

            if (!cookedMeshes.empty())
                outSession.cookedCollisionCache[modelPath] = std::move(cookedMeshes);
        }
    }

    outSession.preloadModelsDone = outSession.preloadModelPaths.empty();
    outSession.staticMeshesDone = outSession.staticMeshes.empty();
    outSession.instancedMeshesDone = outSession.instancedMeshes.empty();
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

        // InstancedMeshes는 collision data를 만들지 않는다.
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

    outFinished =
        session.preloadModelsDone &&
        session.staticMeshesDone &&
        session.instancedMeshesDone;

    if (outFinished && !session.summaryLogged)
    {
        LOG_DEBUG(
            "[MapLoader::Summary] processCalls={} begin={}ms models(data={}, createCalls={}, create={}ms, materials={}ms/{}) staticMeshes(data={}, add={}ms/{}, collisionBuild={}ms/{}) instancedMeshes(data={}, transforms={}ms, add={}ms/{}) autoInstanced(groups={}, statics={})",
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
            session.autoInstancedGroupCount,
            session.autoInstancedStaticCount);

        session.summaryLogged = true;
    }

    return S_OK;
}

void MapLoader::Prepare_StaticAutoInstancing(LoadSession& session)
{
    unordered_map<string, vector<size_t>> indicesByModelPath;
    indicesByModelPath.reserve(session.staticMeshes.size());

    for (size_t i = 0; i < session.staticMeshes.size(); ++i)
    {
        const string modelPath = session.staticMeshes[i].value("ModelPath", string{});
        if (!modelPath.empty())
        {
            indicesByModelPath[modelPath].push_back(i);
        }
    }

    vector<json> remainingStaticMeshes;
    remainingStaticMeshes.reserve(session.staticMeshes.size());

    for (const auto& item : session.staticMeshes)
    {
        const string modelPath = item.value("ModelPath", string{});
        const auto groupIt = indicesByModelPath.find(modelPath);
        if (modelPath.empty() || groupIt == indicesByModelPath.end() || groupIt->second.size() < 2)
        {
            remainingStaticMeshes.push_back(item);
            continue;
        }

        auto modelIt = session.preloadedModelCache.find(modelPath);
        if (modelIt == session.preloadedModelCache.end() || modelIt->second == nullptr)
        {
            remainingStaticMeshes.push_back(item);
            continue;
        }

        Shared<StaticCollisionData> collisionData = GetOrCreateCookedCollision(session, modelIt->second, modelPath);
        if (collisionData != nullptr)
        {
            remainingStaticMeshes.push_back(item);
            continue;
        }

        const vector<size_t>& groupIndices = groupIt->second;
        if (groupIndices.front() != (&item - session.staticMeshes.data()))
        {
            continue;
        }

        json instancedItem;
        instancedItem["Id"] = modelPath;
        instancedItem["ModelPath"] = modelPath;
        instancedItem["Instances"] = json::array();

        for (const size_t sourceIndex : groupIndices)
        {
            const json& source = session.staticMeshes[sourceIndex];
            json instance;
            instance["Position"] = source["Position"];
            instance["Rotation"] = source["Rotation"];
            instance["Scale"] = source["Scale"];
            instancedItem["Instances"].push_back(std::move(instance));
        }

        session.autoInstancedStaticCount += groupIndices.size();
        ++session.autoInstancedGroupCount;
        session.instancedMeshes.push_back(std::move(instancedItem));
    }

    session.staticMeshes = std::move(remainingStaticMeshes);
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
                auto& meshArray = it.value();
                if (meshArray.is_array())
                {
                    json binaryMeshArray = json::array();
                    for (auto& base64Mesh : meshArray)
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

    auto sessionIt = session.preloadedCollisionCache.find(modelPathStr);
    if (sessionIt != session.preloadedCollisionCache.end())
        return sessionIt->second;

    {
        std::lock_guard<std::mutex> lock(g_globalCacheMutex);
        auto globalIt = g_globalCollisionCache.find(modelPathStr);
        if (globalIt != g_globalCollisionCache.end())
        {
            session.preloadedCollisionCache[modelPathStr] = globalIt->second;
            return globalIt->second;
        }
    }

    auto cookedIt = session.cookedCollisionCache.find(modelPathStr);
    if (cookedIt == session.cookedCollisionCache.end() || cookedIt->second.empty())
        return nullptr;

    auto collisionData = make_shared<StaticCollisionData>();
    collisionData->cacheKey = model->Get_SourceModelPath();
    if (collisionData->cacheKey.empty())
        collisionData->cacheKey = String::ToWString(modelPathStr);
    collisionData->preLocalTransform = model->Get_PreLocalTransformMatrix();
    collisionData->cookedTriangleMeshes = cookedIt->second;

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
