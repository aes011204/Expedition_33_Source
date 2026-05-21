#pragma once
#include "Client_Defines.h"
#include "Level.h"
#include "MapLoader.h"

NS_BEGIN(Client)

class StreamingLevel abstract : public Level
{
public:
    struct MapStreamingDesc
    {
        wstring chunkRoot = {};
        wstring layerTag = L"Layer_MapStaticMesh";
        uint32 levelIndex = 0;
        int32 gridWidth = 0;
        int32 gridHeight = 0;
        int32 initialChunkX = 0;
        int32 initialChunkZ = 0;
        int32 activeChunkRadius = 1;
        size_t maxMeshesPerFrame = 8;
        chrono::milliseconds maxTimePerFrame{ 8 };
    };

private:
    enum class ChunkLoadState
    {
        Unloaded,
        Queued,
        Loading,
        Loaded,
    };

    struct StreamChunk
    {
        int32 gridX = 0;
        int32 gridZ = 0;
        wstring path = {};
        ChunkLoadState state = ChunkLoadState::Unloaded;
        MapLoader::LoadSession session = {};
        vector<Shared<GameObject>> loadedObjects = {};
    };

protected:
    StreamingLevel(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~StreamingLevel() = default;

protected:
    HRESULT Initialize_MapStreaming(const MapStreamingDesc& desc);
    void    Queue_InitialMapChunks();
    void    Update_MapStreaming(float timeDelta);
    void    Free_MapStreaming();

private:
    void    Initialize_MapChunks();
    void    Queue_MapChunk(int32 gridX, int32 gridZ);
    void    Process_MapChunkQueue();
    void    Unload_MapChunk(int32 chunkIndex);
    int32   To_ChunkIndex(int32 gridX, int32 gridZ) const;
    wstring Build_MapChunkPath(int32 gridX, int32 gridZ) const;

private:
    MapStreamingDesc _mapStreamingDesc = {};
    vector<StreamChunk> _mapChunks = {};
    deque<int32> _mapChunkLoadQueue = {};
    int32 _loadingChunkIndex = -1;
};

NS_END
