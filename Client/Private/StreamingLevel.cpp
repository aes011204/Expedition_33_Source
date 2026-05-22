#include "StreamingLevel.h"
#include "GameInstance.h"

StreamingLevel::StreamingLevel(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT StreamingLevel::Initialize_MapStreaming(const MapStreamingDesc& desc)
{
    if (desc.chunkRoot.empty() || desc.gridWidth <= 0 || desc.gridHeight <= 0)
        return E_FAIL;

    _mapStreamingDesc = desc;
    Initialize_MapChunks();
    Queue_InitialMapChunks();

    return S_OK;
}

void StreamingLevel::Update_MapStreaming(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    Process_MapChunkQueue();
}

void StreamingLevel::Free_MapStreaming()
{
    // 1차 구현에서는 unload를 자동 활성화하지 않는다. 큐만 비워 dangling 작업을 막는다.
    _mapChunkLoadQueue.clear();
    _loadingChunkIndex = -1;
}

void StreamingLevel::Initialize_MapChunks()
{
    _mapChunks.clear();
    _mapChunks.reserve(_mapStreamingDesc.gridWidth * _mapStreamingDesc.gridHeight);

    for (int32 z = 0; z < _mapStreamingDesc.gridHeight; ++z)
    {
        for (int32 x = 0; x < _mapStreamingDesc.gridWidth; ++x)
        {
            StreamChunk chunk{};
            chunk.gridX = x;
            chunk.gridZ = z;
            chunk.path = Build_MapChunkPath(x, z);
            _mapChunks.push_back(std::move(chunk));
        }
    }
}

wstring StreamingLevel::Build_MapChunkPath(int32 gridX, int32 gridZ) const
{
    wchar_t buffer[MAX_PATH] = {};
    swprintf_s(
        buffer,
        L"%s/chunk_%02d_%02d.json",
        _mapStreamingDesc.chunkRoot.c_str(),
        gridZ, // 파일명 첫 번째 값은 행(row)이다.
        gridX); // 파일명 두 번째 값은 열(col)이다.

    return buffer;
}

int32 StreamingLevel::To_ChunkIndex(int32 gridX, int32 gridZ) const
{
    if (gridX < 0 || gridX >= _mapStreamingDesc.gridWidth)
        return -1;

    if (gridZ < 0 || gridZ >= _mapStreamingDesc.gridHeight)
        return -1;

    return gridZ * _mapStreamingDesc.gridWidth + gridX;
}

void StreamingLevel::Queue_InitialMapChunks()
{
    Queue_MapChunk(_mapStreamingDesc.initialChunkX, _mapStreamingDesc.initialChunkZ);
    Queue_MapChunk(_mapStreamingDesc.initialChunkX + 1, _mapStreamingDesc.initialChunkZ);
    Queue_MapChunk(_mapStreamingDesc.initialChunkX, _mapStreamingDesc.initialChunkZ + 1);
    Queue_MapChunk(_mapStreamingDesc.initialChunkX + 1, _mapStreamingDesc.initialChunkZ + 1);
}

void StreamingLevel::Queue_MapChunk(int32 gridX, int32 gridZ)
{
    const int32 chunkIndex = To_ChunkIndex(gridX, gridZ);
    if (chunkIndex < 0)
        return;

    StreamChunk& chunk = _mapChunks[chunkIndex];
    if (chunk.state != ChunkLoadState::Unloaded)
        return;

    chunk.state = ChunkLoadState::Queued;
    _mapChunkLoadQueue.push_back(chunkIndex);

    LOG_DEBUG("[StreamingLevel::ChunkStreaming] queued chunk=({}, {})", gridX, gridZ);
}

void StreamingLevel::Process_MapChunkQueue()
{
    if (_loadingChunkIndex < 0)
    {
        while (!_mapChunkLoadQueue.empty())
        {
            const int32 nextIndex = _mapChunkLoadQueue.front();
            _mapChunkLoadQueue.pop_front();

            if (nextIndex < 0 || nextIndex >= static_cast<int32>(_mapChunks.size()))
                continue;

            StreamChunk& chunk = _mapChunks[nextIndex];
            if (chunk.state != ChunkLoadState::Queued)
                continue;

            chunk.loadedObjects.clear();
            chunk.session = {};
            chunk.state = ChunkLoadState::Loading;

            const HRESULT beginResult = MapLoader::Begin_LoadSession(
                _device,
                _context,
                chunk.path,
                _mapStreamingDesc.levelIndex,
                _mapStreamingDesc.layerTag,
                chunk.session);

            if (FAILED(beginResult))
            {
                LOG_ERROR(
                    "[StreamingLevel::ChunkStreaming] failed to begin chunk=({}, {}) path={}",
                    chunk.gridX,
                    chunk.gridZ,
                    String::ToString(chunk.path));

                chunk.session = {};
                chunk.loadedObjects.clear();
                chunk.state = ChunkLoadState::Unloaded;
                continue;
            }

            chunk.session.spawnedObjects = &chunk.loadedObjects;
            _loadingChunkIndex = nextIndex;

            LOG_DEBUG(
                "[StreamingLevel::ChunkStreaming] begin chunk=({}, {}) path={}",
                chunk.gridX,
                chunk.gridZ,
                String::ToString(chunk.path));

            break;
        }
    }

    if (_loadingChunkIndex < 0)
        return;

    StreamChunk& loadingChunk = _mapChunks[_loadingChunkIndex];

    bool finished = false;
    const HRESULT processResult = MapLoader::Process_LoadSession(
        loadingChunk.session,
        _mapStreamingDesc.maxMeshesPerFrame,
        _mapStreamingDesc.maxTimePerFrame,
        finished);

    if (FAILED(processResult))
    {
        LOG_ERROR(
            "[StreamingLevel::ChunkStreaming] failed to process chunk=({}, {})",
            loadingChunk.gridX,
            loadingChunk.gridZ);

        loadingChunk.session = {};
        loadingChunk.state = ChunkLoadState::Unloaded;
        loadingChunk.loadedObjects.clear();
        _loadingChunkIndex = -1;
        return;
    }

    if (!finished)
        return;

    loadingChunk.state = ChunkLoadState::Loaded;

    LOG_DEBUG(
        "[StreamingLevel::ChunkStreaming] loaded chunk=({}, {}) objects={}",
        loadingChunk.gridX,
        loadingChunk.gridZ,
        loadingChunk.loadedObjects.size());

    _loadingChunkIndex = -1;
}

void StreamingLevel::Unload_MapChunk(int32 chunkIndex)
{
    if (chunkIndex < 0 || chunkIndex >= static_cast<int32>(_mapChunks.size()))
        return;

    StreamChunk& chunk = _mapChunks[chunkIndex];
    if (chunk.state != ChunkLoadState::Loaded)
        return;

    for (const Shared<GameObject>& object : chunk.loadedObjects)
    {
        if (object != nullptr)
        {
            GAME->Remove_GameObject(_mapStreamingDesc.levelIndex, object);
        }
    }

    chunk.loadedObjects.clear();
    chunk.session = {};
    chunk.state = ChunkLoadState::Unloaded;

    LOG_DEBUG(
        "[StreamingLevel::ChunkStreaming] unloaded chunk=({}, {})",
        chunk.gridX,
        chunk.gridZ);
}
