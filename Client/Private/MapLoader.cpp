#include "MapLoader.h"
#include "GameInstance.h"
#include "MapStaticMesh.h"
#include "MapInstancedMesh.h"
#include "MapStaticCollisionBuilder.h"
#include "ModelCom.h"
#include "StaticCollisionData.h"

HRESULT MapLoader::Begin_LoadSession(const ComPtr<Device>& device, const ComPtr<Context>& context,
    const wstring& filePath, uint32 levelIndex, const wstring& layerTag, LoadSession& outSession)
{
    const auto sessionBegin = chrono::steady_clock::now();

    string pathA = String::ToString(filePath);
    ifstream ifs(pathA);
    if (!ifs.is_open())
    {
        return E_FAIL;
    }

    json root = json::parse(ifs);
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

    outSession.preloadModelsDone = outSession.preloadModelPaths.empty();
    outSession.staticMeshesDone = outSession.staticMeshes.empty();
    outSession.instancedMeshesDone = outSession.instancedMeshes.empty();
    outSession.beginSessionDuration =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - sessionBegin);

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

        string fullPathA = "../../../Client/Bin/Resources/Map/" + session.modelFolder + "/" + modelPathStr;

        const auto modelCreateBegin = chrono::steady_clock::now();
        auto model = ModelCom::Create(
            session.device,
            session.context,
            ModelType::NonAnim,
            fullPathA.c_str(),
            Matrix::CreateScale(0.01f));
        const auto modelCreateMs =
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - modelCreateBegin);
        session.modelCreateDuration += modelCreateMs;
        ++session.modelCreateCount;
        if (modelCreateMs >= chrono::milliseconds(100))
        {
            LOG_DEBUG("[MapLoader::Slow] model create={} path={}", modelCreateMs.count(), modelPathStr);
        }

        if (!model)
        {
            continue;
        }

        int32 forcedPass = -1;
        auto overrideIt = session.modelOverrides.find(modelPathStr);
        if (overrideIt != session.modelOverrides.end())
        {
            forcedPass = overrideIt->second;
        }

        const auto materialApplyBegin = chrono::steady_clock::now();
        ApplyMaterials(model, session.modelFolder, fullPathA, forcedPass);
        const auto materialApplyMs =
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - materialApplyBegin);
        session.materialApplyDuration += materialApplyMs;
        ++session.materialApplyCount;
        if (materialApplyMs >= chrono::milliseconds(100))
        {
            LOG_DEBUG("[MapLoader::Slow] material apply={} path={}", materialApplyMs.count(), modelPathStr);
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
            string fullPathA = "../../../Client/Bin/Resources/Map/" + session.modelFolder + "/" + modelPathStr;

            const auto modelCreateBegin = chrono::steady_clock::now();
            auto model = ModelCom::Create(
                session.device,
                session.context,
                ModelType::NonAnim,
                fullPathA.c_str(),
                Matrix::CreateScale(0.01f));
            const auto modelCreateMs =
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - modelCreateBegin);
            session.modelCreateDuration += modelCreateMs;
            ++session.modelCreateCount;
            if (modelCreateMs >= chrono::milliseconds(100))
            {
                LOG_DEBUG("[MapLoader::Slow] model create={} path={}", modelCreateMs.count(), modelPathStr);
            }

            if (!model)
            {
                continue;
            }

            int32 forcedPass = -1;
            auto overrideIt = session.modelOverrides.find(modelPathStr);
            if (overrideIt != session.modelOverrides.end())
            {
                forcedPass = overrideIt->second;
            }

            const auto materialApplyBegin = chrono::steady_clock::now();
            ApplyMaterials(model, session.modelFolder, fullPathA, forcedPass);
            const auto materialApplyMs =
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - materialApplyBegin);
            session.materialApplyDuration += materialApplyMs;
            ++session.materialApplyCount;
            if (materialApplyMs >= chrono::milliseconds(100))
            {
                LOG_DEBUG("[MapLoader::Slow] material apply={} path={}", materialApplyMs.count(), modelPathStr);
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
            const auto collisionBuildBegin = chrono::steady_clock::now();
            Shared<StaticCollisionData> collisionData = MapStaticCollisionBuilder::Build(desc.model);
            const auto collisionBuildMs =
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - collisionBuildBegin);
            session.collisionBuildDuration += collisionBuildMs;
            ++session.collisionBuildCount;
            if (collisionBuildMs >= chrono::milliseconds(100))
            {
                LOG_DEBUG("[MapLoader::Slow] collision build={} path={}", collisionBuildMs.count(), modelPathStr);
            }
            session.preloadedCollisionCache[modelPathStr] = collisionData;
            desc.collisionData = collisionData;
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
        if (staticAddMs >= chrono::milliseconds(100))
        {
            LOG_DEBUG(
                "[MapLoader::Slow] static add={} model={} name={}",
                staticAddMs.count(),
                modelPathStr,
                String::ToString(desc.name));
        }

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
            string fullPathA = "../../../Client/Bin/Resources/Map/" + session.modelFolder + "/" + modelPathStr;

            const auto modelCreateBegin = chrono::steady_clock::now();
            auto model = ModelCom::Create(
                session.device,
                session.context,
                ModelType::NonAnim,
                fullPathA.c_str(),
                Matrix::CreateScale(0.01f));
            const auto modelCreateMs =
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - modelCreateBegin);
            session.modelCreateDuration += modelCreateMs;
            ++session.modelCreateCount;
            if (modelCreateMs >= chrono::milliseconds(100))
            {
                LOG_DEBUG("[MapLoader::Slow] model create={} path={}", modelCreateMs.count(), modelPathStr);
            }

            if (!model)
            {
                continue;
            }

            int32 forcedPass = -1;
            auto overrideIt = session.modelOverrides.find(modelPathStr);
            if (overrideIt != session.modelOverrides.end())
            {
                forcedPass = overrideIt->second;
            }

            const auto materialApplyBegin = chrono::steady_clock::now();
            ApplyMaterials(model, session.modelFolder, fullPathA, forcedPass);
            const auto materialApplyMs =
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - materialApplyBegin);
            session.materialApplyDuration += materialApplyMs;
            ++session.materialApplyCount;
            if (materialApplyMs >= chrono::milliseconds(100))
            {
                LOG_DEBUG("[MapLoader::Slow] material apply={} path={}", materialApplyMs.count(), modelPathStr);
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
        if (instancedAddMs >= chrono::milliseconds(100))
        {
            LOG_DEBUG("[MapLoader::Slow] instanced add={} model={}", instancedAddMs.count(), modelPathStr);
        }

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

        auto collisionIt = session.preloadedCollisionCache.find(modelPath);
        if (collisionIt == session.preloadedCollisionCache.end())
        {
            const auto collisionBuildBegin = chrono::steady_clock::now();
            Shared<StaticCollisionData> collisionData = MapStaticCollisionBuilder::Build(modelIt->second);
            session.collisionBuildDuration +=
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - collisionBuildBegin);
            ++session.collisionBuildCount;
            collisionIt = session.preloadedCollisionCache.emplace(modelPath, collisionData).first;
        }

        if (collisionIt->second != nullptr)
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
    static unordered_map<string, vector<MaterialSlotBinding>> s_materialBindingCache;

    auto cacheIt = s_materialBindingCache.find(jsonPathKey);
    if (cacheIt == s_materialBindingCache.end())
    {
        vector<MaterialSlotBinding> bindings;
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

        cacheIt = s_materialBindingCache.emplace(jsonPathKey, std::move(bindings)).first;
    }

    for (const MaterialSlotBinding& binding : cacheIt->second)
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
