#pragma once
#include "Client_Defines.h"

NS_BEGIN(Engine)
class ModelCom;
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
        vector<json> staticMeshes = {};
        vector<json> instancedMeshes = {};

        size_t nextStaticMeshIndex = 0;
        size_t nextInstancedMeshIndex = 0;

        unordered_map<string, Shared<ModelCom>> staticModelCache = {};
        unordered_map<string, Shared<ModelCom>> instancedModelCache = {};

        bool staticMeshesDone = false;
        bool instancedMeshesDone = false;
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
    static void ApplyMaterials(Shared<ModelCom>& model, const string& modelFolder, const string& modelFullPath);
};

NS_END
