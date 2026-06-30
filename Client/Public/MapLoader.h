#pragma once
#include "Client_Defines.h"
#include "MapAdapter.h"
#include "MapCollisionAdapter.h"
#include <mutex>
#include <unordered_set>

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
    using ModelOverrideEntry = MapAdapter::ModelOverrideEntry;

    struct LoadSession
    {
        ComPtr<Device> device = nullptr;
        ComPtr<Context> context = nullptr;

        uint32 levelIndex = 0;
        wstring layerTag = {};
        wstring sourceMapPath = {};

        string modelFolder = {};
        unordered_map<string, ModelOverrideEntry> modelOverrides;
        vector<json> staticMeshes = {};
        vector<json> instancedMeshes = {};
        vector<json> postProcessVolumes = {};
        vector<json> shaftVolumes = {};
        vector<json> waters = {};
        vector<json> lights;
        vector<json> prefabInstances = {};
        vector<json> authoredObjects = {};
        vector<string> preloadModelPaths = {};
        vector<string> materialAppliedModelPaths = {};

        size_t nextPreloadModelIndex = 0;
        size_t nextMaterialApplyModelIndex = 0;
        size_t nextStaticMeshIndex = 0;
        size_t nextInstancedMeshIndex = 0;

        size_t nextInstancedMeshGroupIndex = 0;
        bool instancedMeshGroupsBuilt = false;
        vector<string> instancedMeshModelOrder;
        umap<string, vector<size_t>> instancedMeshGroups;

        size_t nextWaterIndex = 0;
        size_t nextPrefabInstanceIndex = 0;
        size_t nextAuthoredObjectIndex = 0;

        umap<string, Shared<ModelCom>> preloadedModelCache = {};
        umap<string, Shared<StaticCollisionData>> preloadedCollisionCache = {};
        MapCollisionAdapter::CookedCollisionCache cookedCollisionCache = {};
        MapCollisionAdapter::PrimitiveCollisionCache primitiveCollisionCache = {};

        // 청크 unload를 위해 생성 객체를 선택적으로 수집
        vector<Shared<GameObject>>* spawnedObjects = nullptr; 

        bool staticAutoInstancingPrepared = false;
        bool preloadModelsDone = false;
        bool materialsDone = false;
        bool staticMeshesDone = false;
        bool instancedMeshesDone = false;
        bool postProcessVolumesDone = false;
        bool shaftVolumesDone = false;
        bool watersDone = false;
        bool lightsDone = false;
        bool prefabInstancesDone = false;
        bool authoredObjectsDone = false;

        chrono::milliseconds beginSessionDuration{ 0 };
        chrono::milliseconds modelCreateDuration{ 0 };
        chrono::milliseconds materialApplyDuration{ 0 };
        chrono::milliseconds collisionBuildDuration{ 0 };
        chrono::milliseconds staticAddDuration{ 0 };
        chrono::milliseconds instancedTransformDuration{ 0 };
        chrono::milliseconds instancedAddDuration{ 0 };
        chrono::milliseconds prefabInstanceAddDuration{ 0 };
        chrono::milliseconds authoredObjectAddDuration{ 0 };

        size_t processCallCount = 0;
        size_t modelCreateCount = 0;
        size_t materialApplyCount = 0;
        size_t collisionBuildCount = 0;
        size_t staticAddCount = 0;
        size_t instancedAddCount = 0;
        size_t prefabInstanceAddCount = 0;
        size_t authoredObjectAddCount = 0;
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
    static HRESULT Convert_MapInstance_To_Bin(const wstring& mapInstanceRoot);
    static void Clear_GlobalCache();
    static void Clear_CachedLights();
    static void Reapply_CachedLights();
    static wstring Get_LoadedMapInstanceRoot(uint32 levelIndex);

private:
    static HRESULT Try_BeginMapInstanceSession(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& filePath,
        uint32 levelIndex,
        const wstring& layerTag,
        LoadSession& outSession);

    static void Prepare_StaticAutoInstancing(LoadSession& session);
    static void ApplyMaterials(Shared<ModelCom>& model, const string& modelFolder, const string& modelFullPath, const ModelOverrideEntry* overrideEntry = nullptr);
    static string BuildMaterialSignature(const string& modelFolder, const string& modelFullPath, const ModelOverrideEntry* overrideEntry);
    static void MarkSessionModelMaterialsApplied(LoadSession& session, const string& modelPathStr);
    static void MarkGlobalModelMaterialsApplied(const string& modelFolder, const string& modelPathStr, const unordered_map<string, ModelOverrideEntry>& modelOverrides);
    static void ApplySessionModelMaterials(LoadSession& session, const string& modelPathStr, Shared<ModelCom>& model);

    // 스레드 안전한 전역 모델 조회 및 로드 헬퍼 함수
    static Shared<ModelCom> GetOrLoadModel(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const string& modelFolder,
        const string& modelPathStr,
        const unordered_map<string, ModelOverrideEntry>& modelOverrides,
        bool applyMaterials,
        chrono::milliseconds* outCreateDuration = nullptr,
        size_t* outCreateCount = nullptr,
        chrono::milliseconds* outMaterialDuration = nullptr,
        size_t* outMaterialCount = nullptr);

private:
    static MapCollisionAdapter::RuntimeContext MakeCollisionRuntimeContext(LoadSession& session);

    // 스레드 안전한 전역 맵 모델 프로토타입 캐시 멤버
    static std::mutex g_globalCacheMutex;
    static umap<string, Shared<ModelCom>> g_globalModelCache;
    static unordered_set<string> g_globalMissingModelCache;
    static umap<string, Shared<StaticCollisionData>> g_globalCollisionCache;
    static umap<string, string> g_globalMaterialSignatureByModel;
    static uint32 g_currentLevelIndex;
    static vector<LIGHT_DESC> g_cachedLights;
    static umap<uint32, wstring> g_loadedMapInstanceRootByLevel;
};

NS_END
