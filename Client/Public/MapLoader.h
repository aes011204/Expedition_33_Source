#pragma once
#include "Client_Defines.h"

NS_BEGIN(Engine)
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

        // 청크 unload를 위해 생성 객체를 선택적으로 수집
        vector<Shared<GameObject>>* spawnedObjects = nullptr; 

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

private:
    static void ApplyMaterials(Shared<ModelCom>& model, const string& modelFolder, const string& modelFullPath, int32 forcedPassIndex = -1);
};

NS_END
