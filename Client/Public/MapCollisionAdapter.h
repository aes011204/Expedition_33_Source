#pragma once

#include "Client_Defines.h"
#include "CollisionProxy_Types.h"
#include <mutex>

NS_BEGIN(Engine)
class ModelCom;
struct StaticCollisionData;
NS_END

NS_BEGIN(Client)

class MapCollisionAdapter final
{
public:
    using CookedCollisionCache = umap<string, vector<vector<uint8_t>>>;
    using PrimitiveCollisionCache = umap<string, vector<ModelCollisionProxy>>;
    using StaticCollisionCache = umap<string, Shared<Engine::StaticCollisionData>>;

    struct RuntimeContext
    {
        std::mutex* globalCacheMutex = nullptr;
        StaticCollisionCache* globalCollisionCache = nullptr;
        StaticCollisionCache* preloadedCollisionCache = nullptr;
        CookedCollisionCache* cookedCollisionCache = nullptr;
        PrimitiveCollisionCache* primitiveCollisionCache = nullptr;
        chrono::milliseconds* outBuildDuration = nullptr;
        size_t* outBuildCount = nullptr;
    };

public:
    // Saved collision source ingestion. Legacy MapData and sibling model binaries share the same cache shape.
    static void AppendModelCollisionSources(
        const json& mapData,
        CookedCollisionCache& outCookedCollisionCache,
        PrimitiveCollisionCache& outPrimitiveCollisionCache);

    static void AppendSiblingModelCollisionSources(
        const string& modelFolder,
        const vector<json>& staticMeshes,
        CookedCollisionCache& outCookedCollisionCache,
        PrimitiveCollisionCache& outPrimitiveCollisionCache);

    // Runtime StaticCollisionData creation and cache reuse for map objects.
    static Shared<Engine::StaticCollisionData> GetOrCreateStaticCollision(
        RuntimeContext context,
        const Shared<Engine::ModelCom>& model,
        const string& modelPathStr);

    static Shared<Engine::StaticCollisionData> BuildStaticCollision(
        const Shared<Engine::ModelCom>& model,
        const string& modelPathStr,
        std::mutex* globalCacheMutex,
        StaticCollisionCache* globalCollisionCache,
        chrono::milliseconds* outDuration = nullptr,
        size_t* outCount = nullptr);
};

NS_END
