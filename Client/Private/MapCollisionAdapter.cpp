#include "MapCollisionAdapter.h"
#include "MapAdapter.h"
#include "MapStaticCollisionBuilder.h"
#include "ModelCom.h"
#include "StaticCollisionData.h"

NS_BEGIN(Client)

namespace
{
    bool TryParseCollisionProxy(const json& node, ModelCollisionProxy& outProxy)
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

    const json* FindCollisionArray(const json& node, const char* primaryKey, const char* fallbackKey)
    {
        if (!node.is_object())
            return nullptr;

        if (node.contains(primaryKey) && node[primaryKey].is_array())
            return &node[primaryKey];

        if (node.contains(fallbackKey) && node[fallbackKey].is_array())
            return &node[fallbackKey];

        return nullptr;
    }

    void AppendCookedCollisionMesh(const json& meshData, vector<vector<uint8_t>>& outCookedMeshes)
    {
        if (meshData.is_binary())
        {
            const auto& binaryData = meshData.get_binary();
            outCookedMeshes.emplace_back(binaryData.begin(), binaryData.end());
        }
        else if (meshData.is_string())
        {
            outCookedMeshes.push_back(String::Base64_Decode(meshData.get<string>()));
        }
    }

    void AppendCookedCollisionMeshes(const json& collisionEntry, vector<vector<uint8_t>>& outCookedMeshes)
    {
        if (collisionEntry.is_array()) // Legacy format: cooked triangle mesh array.
        {
            outCookedMeshes.reserve(collisionEntry.size());
            for (const json& meshData : collisionEntry)
                AppendCookedCollisionMesh(meshData, outCookedMeshes);

            return;
        }

        if (!collisionEntry.is_object())
            return;

        const json* meshArray = FindCollisionArray(collisionEntry, "CookedTriangleMeshes", "cooked_triangle_meshes");
        if (meshArray == nullptr)
            return;

        outCookedMeshes.reserve(meshArray->size());
        for (const json& meshData : *meshArray)
            AppendCookedCollisionMesh(meshData, outCookedMeshes);
    }

    void AppendPrimitiveCollisionProxies(const json& collisionEntry, vector<ModelCollisionProxy>& outPrimitiveProxies)
    {
        if (!collisionEntry.is_object())
            return;

        const json* proxyArray = FindCollisionArray(collisionEntry, "PrimitiveProxies", "primitive_proxies");
        if (proxyArray == nullptr)
            return;

        for (const json& proxyNode : *proxyArray)
        {
            ModelCollisionProxy proxy{};
            if (TryParseCollisionProxy(proxyNode, proxy))
                outPrimitiveProxies.push_back(std::move(proxy));
        }
    }

    bool TryLoadSiblingCollisionSource(
        const string& modelFolder,
        const string& modelPathStr,
        vector<vector<uint8_t>>& outCookedMeshes,
        vector<ModelCollisionProxy>& outPrimitiveProxies)
    {
        outCookedMeshes.clear();
        outPrimitiveProxies.clear();

        if (modelPathStr.empty())
            return false;

        fs::path collisionPath(MapAdapter::ResolveMapModelPath(modelFolder, modelPathStr));
        collisionPath.replace_extension(".collision.bin");
        if (!fs::exists(collisionPath) || !fs::is_regular_file(collisionPath))
            return false;

        ifstream ifs(collisionPath, ios::binary);
        if (!ifs.is_open())
            return false;

        try
        {
            const json root = json::from_msgpack(ifs);
            AppendCookedCollisionMeshes(root, outCookedMeshes);
            AppendPrimitiveCollisionProxies(root, outPrimitiveProxies);
        }
        catch (...)
        {
            outCookedMeshes.clear();
            outPrimitiveProxies.clear();
            return false;
        }

        return !outCookedMeshes.empty() || !outPrimitiveProxies.empty();
    }

    void EnsureTriangleMeshes(
        const Shared<StaticCollisionData>& collisionData,
        const Shared<ModelCom>& model,
        const string& modelPathStr)
    {
        if (collisionData == nullptr || model == nullptr)
            return;

        if (!collisionData->cookedTriangleMeshes.empty() || !collisionData->triangleMeshes.empty())
            return;

        const wstring sourceModelPath = !model->Get_SourceModelPath().empty()
            ? model->Get_SourceModelPath()
            : String::ToWString(modelPathStr);

        if (sourceModelPath.empty())
            return;

        // Stored collision may be proxy-only. Gameplay still supplements triangle mesh data from the .model file.
        MapStaticCollisionBuilder::Build_TriangleMeshes_FromModelFile(
            sourceModelPath,
            collisionData->triangleMeshes);
    }
}

void MapCollisionAdapter::AppendModelCollisionSources(
    const json& mapData,
    CookedCollisionCache& outCookedCollisionCache,
    PrimitiveCollisionCache& outPrimitiveCollisionCache)
{
    if (!mapData.contains("ModelCollisions") || !mapData["ModelCollisions"].is_object())
        return;

    for (const auto& [modelPath, collisionEntry] : mapData["ModelCollisions"].items())
    {
        vector<vector<uint8_t>> cookedMeshes;
        vector<ModelCollisionProxy> primitiveProxies;

        AppendCookedCollisionMeshes(collisionEntry, cookedMeshes);
        AppendPrimitiveCollisionProxies(collisionEntry, primitiveProxies);

        if (!cookedMeshes.empty())
            outCookedCollisionCache[modelPath] = std::move(cookedMeshes);

        if (!primitiveProxies.empty())
            outPrimitiveCollisionCache[modelPath] = std::move(primitiveProxies);
    }
}

void MapCollisionAdapter::AppendSiblingModelCollisionSources(
    const string& modelFolder,
    const vector<json>& staticMeshes,
    CookedCollisionCache& outCookedCollisionCache,
    PrimitiveCollisionCache& outPrimitiveCollisionCache)
{
    for (const json& item : staticMeshes)
    {
        if (!item.is_object())
            continue;

        const string modelPath = item.value("ModelPath", string{});
        if (modelPath.empty())
            continue;

        vector<vector<uint8_t>> cookedMeshes;
        vector<ModelCollisionProxy> primitiveProxies;
        if (!TryLoadSiblingCollisionSource(modelFolder, modelPath, cookedMeshes, primitiveProxies))
            continue;

        if (!cookedMeshes.empty())
            outCookedCollisionCache[modelPath] = std::move(cookedMeshes);

        if (!primitiveProxies.empty())
            outPrimitiveCollisionCache[modelPath] = std::move(primitiveProxies);
    }
}

Shared<StaticCollisionData> MapCollisionAdapter::GetOrCreateStaticCollision(
    RuntimeContext context,
    const Shared<ModelCom>& model,
    const string& modelPathStr)
{
    if (model == nullptr || modelPathStr.empty())
        return nullptr;

    if (context.preloadedCollisionCache != nullptr)
    {
        auto sessionIt = context.preloadedCollisionCache->find(modelPathStr);
        if (sessionIt != context.preloadedCollisionCache->end())
        {
            EnsureTriangleMeshes(sessionIt->second, model, modelPathStr);
            return sessionIt->second;
        }
    }

    if (context.globalCacheMutex != nullptr && context.globalCollisionCache != nullptr)
    {
        std::lock_guard<std::mutex> lock(*context.globalCacheMutex);
        auto globalIt = context.globalCollisionCache->find(modelPathStr);
        if (globalIt != context.globalCollisionCache->end())
        {
            EnsureTriangleMeshes(globalIt->second, model, modelPathStr);
            if (context.preloadedCollisionCache != nullptr)
                (*context.preloadedCollisionCache)[modelPathStr] = globalIt->second;
            return globalIt->second;
        }
    }

    auto cookedIt = context.cookedCollisionCache != nullptr
        ? context.cookedCollisionCache->find(modelPathStr)
        : CookedCollisionCache::iterator{};
    auto primitiveIt = context.primitiveCollisionCache != nullptr
        ? context.primitiveCollisionCache->find(modelPathStr)
        : PrimitiveCollisionCache::iterator{};

    const bool hasCookedEntry =
        context.cookedCollisionCache != nullptr &&
        cookedIt != context.cookedCollisionCache->end();
    const bool hasPrimitiveEntry =
        context.primitiveCollisionCache != nullptr &&
        primitiveIt != context.primitiveCollisionCache->end();

    if (!hasCookedEntry && !hasPrimitiveEntry)
    {
        Shared<StaticCollisionData> collisionData = BuildStaticCollision(
            model,
            modelPathStr,
            context.globalCacheMutex,
            context.globalCollisionCache,
            context.outBuildDuration,
            context.outBuildCount);

        if (collisionData != nullptr && context.preloadedCollisionCache != nullptr)
            (*context.preloadedCollisionCache)[modelPathStr] = collisionData;

        return collisionData;
    }

    auto collisionData = make_shared<StaticCollisionData>();
    collisionData->cacheKey = model->Get_SourceModelPath();
    if (collisionData->cacheKey.empty())
        collisionData->cacheKey = String::ToWString(modelPathStr);
    collisionData->preLocalTransform = model->Get_PreLocalTransformMatrix();

    if (hasCookedEntry && !cookedIt->second.empty())
        collisionData->cookedTriangleMeshes = cookedIt->second;
    if (hasPrimitiveEntry && !primitiveIt->second.empty())
        collisionData->primitiveProxies = primitiveIt->second;

    EnsureTriangleMeshes(collisionData, model, modelPathStr);

    if (collisionData->cookedTriangleMeshes.empty() &&
        collisionData->primitiveProxies.empty() &&
        collisionData->triangleMeshes.empty())
    {
        return nullptr;
    }

    if (context.preloadedCollisionCache != nullptr)
        (*context.preloadedCollisionCache)[modelPathStr] = collisionData;

    if (context.globalCacheMutex != nullptr && context.globalCollisionCache != nullptr)
    {
        std::lock_guard<std::mutex> lock(*context.globalCacheMutex);
        (*context.globalCollisionCache)[modelPathStr] = collisionData;
    }

    return collisionData;
}

Shared<StaticCollisionData> MapCollisionAdapter::BuildStaticCollision(
    const Shared<ModelCom>& model,
    const string& modelPathStr,
    std::mutex* globalCacheMutex,
    StaticCollisionCache* globalCollisionCache,
    chrono::milliseconds* outDuration,
    size_t* outCount)
{
    if (model == nullptr)
        return nullptr;

    if (globalCacheMutex != nullptr && globalCollisionCache != nullptr)
    {
        std::lock_guard<std::mutex> lock(*globalCacheMutex);
        auto it = globalCollisionCache->find(modelPathStr);
        if (it != globalCollisionCache->end())
            return it->second;
    }

    const auto begin = chrono::steady_clock::now();
    Shared<StaticCollisionData> collisionData = MapStaticCollisionBuilder::Build(model);
    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - begin);

    if (outDuration) *outDuration += elapsed;
    if (outCount) (*outCount)++;

    if (globalCacheMutex != nullptr && globalCollisionCache != nullptr)
    {
        std::lock_guard<std::mutex> lock(*globalCacheMutex);
        (*globalCollisionCache)[modelPathStr] = collisionData;
    }

    return collisionData;
}

NS_END
