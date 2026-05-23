#pragma once
#include "Client_Defines.h"
#include "CollisionProxy_Types.h"
#include <mutex>

NS_BEGIN(Engine)
class GameObject;
class ModelCom;
struct StaticCollisionData;
NS_END

NS_BEGIN(Client)

class MapStaticMesh;

class MapLoader final
{
public:
    struct LoadSession
    {
        ComPtr<Device> device = nullptr;
        ComPtr<Context> context = nullptr;

        uint32 levelIndex = 0;
        wstring layerTag = {};

        string modelFolder = {};
        unordered_map<string, int32> modelOverrides;
        vector<json> staticMeshes = {};
        vector<json> instancedMeshes = {};
        vector<string> preloadModelPaths = {};

        size_t nextPreloadModelIndex = 0;
        size_t nextStaticMeshIndex = 0;
        size_t nextInstancedMeshIndex = 0;

        umap<string, Shared<ModelCom>> preloadedModelCache = {};
        umap<string, Shared<StaticCollisionData>> preloadedCollisionCache = {};
        umap<string, vector<vector<uint8_t>>> cookedCollisionCache = {};
        umap<string, vector<ModelCollisionProxy>> primitiveCollisionCache = {};

        // 청크 unload를 위해 생성 객체를 선택적으로 수집
        vector<Shared<GameObject>>* spawnedObjects = nullptr; 

        bool staticAutoInstancingPrepared = false;
        bool preloadModelsDone = false;
        bool staticMeshesDone = false;
        bool instancedMeshesDone = false;

        chrono::milliseconds beginSessionDuration{ 0 };
        chrono::milliseconds modelCreateDuration{ 0 };
        chrono::milliseconds materialApplyDuration{ 0 };
        chrono::milliseconds collisionBuildDuration{ 0 };
        chrono::milliseconds staticAddDuration{ 0 };
        chrono::milliseconds instancedTransformDuration{ 0 };
        chrono::milliseconds instancedAddDuration{ 0 };

        size_t processCallCount = 0;
        size_t modelCreateCount = 0;
        size_t materialApplyCount = 0;
        size_t collisionBuildCount = 0;
        size_t staticAddCount = 0;
        size_t instancedAddCount = 0;
        size_t autoInstancedStaticCount = 0;
        size_t autoInstancedGroupCount = 0;

        bool summaryLogged = false;
    };

public:
    static HRESULT Begin_LoadSession(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& filePath,
        uint32 levelIndex,
        const wstring& layerTag,
        LoadSession& outSession);

    static HRESULT Preload_SessionData(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& filePath,
        uint32 levelIndex,
        const wstring& layerTag,
        LoadSession& outSession);

    static HRESULT Process_LoadSession(
        LoadSession& session,
        size_t maxMeshesPerFrame,
        chrono::milliseconds maxTimePerFrame,
        bool& outFinished);

    static HRESULT Load(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& filePath,
        uint32 levelIndex,
        const wstring& layerTag);

    static HRESULT Convert_Json_To_Bin(const wstring& jsonPath, const wstring& binPath);
    static void Clear_GlobalCache();

private:
    static void Prepare_StaticAutoInstancing(LoadSession& session);
    static void ApplyMaterials(Shared<ModelCom>& model, const string& modelFolder, const string& modelFullPath, int32 forcedPassIndex = -1);

    // 스레드 안전한 전역 모델 조회 및 로드 헬퍼 함수
    static Shared<ModelCom> GetOrLoadModel(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const string& modelFolder,
        const string& modelPathStr,
        const unordered_map<string, int32>& modelOverrides,
        chrono::milliseconds* outCreateDuration = nullptr,
        size_t* outCreateCount = nullptr,
        chrono::milliseconds* outMaterialDuration = nullptr,
        size_t* outMaterialCount = nullptr);

    // 스레드 안전한 전역 콜리전 빌드 및 조회 헬퍼 함수
    static Shared<StaticCollisionData> GetOrBuildCollision(
        const Shared<ModelCom>& model,
        const string& modelPathStr,
        chrono::milliseconds* outDuration = nullptr,
        size_t* outCount = nullptr);

    static Shared<StaticCollisionData> GetOrCreateCookedCollision(
        LoadSession& session,
        const Shared<ModelCom>& model,
        const string& modelPathStr);

private:
    // 스레드 안전한 전역 맵 모델 프로토타입 캐시 멤버
    static std::mutex g_globalCacheMutex;
    static umap<string, Shared<ModelCom>> g_globalModelCache;
    static umap<string, Shared<StaticCollisionData>> g_globalCollisionCache;
    static umap<string, std::vector<std::pair<uint32, std::string>>> g_globalMaterialBindingCache;
    static uint32 g_currentLevelIndex;
};

NS_END
