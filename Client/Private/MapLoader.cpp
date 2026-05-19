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

    return S_OK;
}

HRESULT MapLoader::Process_LoadSession(LoadSession& session, size_t maxMeshesPerFrame,
    chrono::milliseconds maxTimePerFrame, bool& outFinished)
{
    outFinished = false;

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

        auto model = ModelCom::Create(
            session.device,
            session.context,
            ModelType::NonAnim,
            fullPathA.c_str(),
            Matrix::CreateScale(0.01f));

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

        ApplyMaterials(model, session.modelFolder, fullPathA, forcedPass);
        session.preloadedModelCache[modelPathStr] = model;

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

        const json& e = session.staticMeshes[session.nextStaticMeshIndex++];
        const string modelPathStr = e.value("ModelPath", string{});
        if (modelPathStr.empty())
            continue;

        MapStaticMesh::MAPSTATICMESH_DESC desc{};

        auto it = session.preloadedModelCache.find(modelPathStr);
        if (it != session.preloadedModelCache.end())
        {
            desc.model = it->second;
        }
        else
        {
            string fullPathA = "../../../Client/Bin/Resources/Map/" + session.modelFolder + "/" + modelPathStr;

            auto model = ModelCom::Create(
                session.device,
                session.context,
                ModelType::NonAnim,
                fullPathA.c_str(),
                Matrix::CreateScale(0.01f));

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

            ApplyMaterials(model, session.modelFolder, fullPathA, forcedPass);
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
            Shared<StaticCollisionData> collisionData = MapStaticCollisionBuilder::Build(desc.model);
            session.preloadedCollisionCache[modelPathStr] = collisionData;
            desc.collisionData = collisionData;
        }

        desc.modelPath = {};

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

        if (FAILED(GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_MapStaticMesh"),
            session.levelIndex,
            session.layerTag,
            &desc)))
        {
            continue;
        }

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

            auto model = ModelCom::Create(
                session.device,
                session.context,
                ModelType::NonAnim,
                fullPathA.c_str(),
                Matrix::CreateScale(0.01f));

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

            ApplyMaterials(model, session.modelFolder, fullPathA, forcedPass);
            session.preloadedModelCache[modelPathStr] = model;
            desc.model = model;
        }

        // InstancedMeshes는 collision data를 만들지 않는다.
        if (e.contains("Instances"))
        {
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
        }

        if (FAILED(GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_MapInstancedMesh"),
            session.levelIndex,
            session.layerTag,
            &desc)))
        {
            continue;
        }

        ++processedCount;
    }

    outFinished =
        session.preloadModelsDone &&
        session.staticMeshesDone &&
        session.instancedMeshesDone;

    return S_OK;
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
    ifstream jfs(jsonPath);
    if (!jfs.is_open())
    {
        return;
    }

    try
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

                model->Apply_MaterialByAssetName(i, bundleKey, miName);
            }

            break;
        }
    }
    catch (...)
    {
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
