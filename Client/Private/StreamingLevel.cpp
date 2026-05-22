#include "StreamingLevel.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "GameObject.h"
#include "TransformCom.h"

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

    // manifest.json을 로드하여 실제 맵 바운드 경계 설정
    fs::path manifestPath = fs::path(desc.chunkRoot) / "manifest.json";
    json manifest = GAME->Load_JsonFile(manifestPath);
    if (!manifest.is_null() && manifest.is_object())
    {
        _xMin = manifest["bounds"]["x_min"].get<float>();
        _zMin = manifest["bounds"]["z_min"].get<float>();
        _chunkSizeX = manifest["chunk_size"]["x"].get<float>();
        _chunkSizeZ = manifest["chunk_size"]["z"].get<float>();
        _isManifestLoaded = true;

        LOG_INFO("[StreamingLevel] Loaded manifest. bounds(x_min={}, z_min={}) chunk_size(x={}, z={})",
            _xMin, _zMin, _chunkSizeX, _chunkSizeZ);
    }
    else
    {
        LOG_ERROR("[StreamingLevel] Failed to load manifest.json from {}", String::ToString(manifestPath.wstring()));
        return E_FAIL;
    }

    Queue_InitialMapChunks();

    return S_OK;
}

void StreamingLevel::Update_MapStreaming(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    // 실시간 플레이어 위치 추적 및 중심 격자 갱신
    Shared<GameObject> player = CLIENT->Find_Player(_mapStreamingDesc.levelIndex);
    if (player != nullptr && _isManifestLoaded)
    {
        Vec3 playerPos = player->Get_Transform()->Get_Position();
        
        int32 currentCol = static_cast<int32>((playerPos.x - _xMin) / _chunkSizeX);
        int32 currentRow = static_cast<int32>((playerPos.z - _zMin) / _chunkSizeZ);

        currentCol = max(0, min(currentCol, _mapStreamingDesc.gridWidth - 1));
        currentRow = max(0, min(currentRow, _mapStreamingDesc.gridHeight - 1));

        if (currentCol != _prevCenterCol || currentRow != _prevCenterRow)
        {
            _prevCenterCol = currentCol;
            _prevCenterRow = currentRow;
            Update_ActiveChunks(currentCol, currentRow);
        }
    }

    Process_MapChunkQueue();
}

void StreamingLevel::Free_MapStreaming()
{
    _mapChunkLoadQueue.clear();
    _loadingChunkIndex = -1;
    _preloadingChunkIndex = -1;

    for (int32 i = 0; i < static_cast<int32>(_mapChunks.size()); ++i)
    {
        Unload_MapChunk(i);
    }
    _mapChunks.clear();
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
            chunk.preloadFinished = make_shared<atomic<bool>>(false);
            chunk.preloadFailed = make_shared<atomic<bool>>(false);
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
    // 초기 청크는 로딩 화면에서 Loader가 이미 스폰함
    // → StreamingLevel은 Loaded로 마킹만 해서 재스폰 방지
    auto MarkLoaderSpawned = [this](int32 x, int32 z)
    {
        const int32 idx = To_ChunkIndex(x, z);
        if (idx >= 0)
            _mapChunks[idx].state = ChunkLoadState::Loaded; // loadedObjects는 비어있음 (Loader가 스폰)
    };

    MarkLoaderSpawned(_mapStreamingDesc.initialChunkX,     _mapStreamingDesc.initialChunkZ);
    MarkLoaderSpawned(_mapStreamingDesc.initialChunkX + 1, _mapStreamingDesc.initialChunkZ);
    MarkLoaderSpawned(_mapStreamingDesc.initialChunkX,     _mapStreamingDesc.initialChunkZ + 1);
    MarkLoaderSpawned(_mapStreamingDesc.initialChunkX + 1, _mapStreamingDesc.initialChunkZ + 1);
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
    // 1. 비동기 프리로드 및 메인 로드가 둘 다 비어있을 때 새로운 큐를 예약 진행
    if (_loadingChunkIndex < 0 && _preloadingChunkIndex < 0)
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

            chunk.preloadFinished->store(false);
            chunk.preloadFailed->store(false);
            chunk.state = ChunkLoadState::Preloading;
            _preloadingChunkIndex = nextIndex;

            // 스레드풀에서 안전한 백그라운드 프리로드 예약 실행
            const bool enqueued = GAME->Enqueue_ThreadJob([this, nextIndex]()
            {
                StreamChunk& targetChunk = _mapChunks[nextIndex];

                HRESULT hr = MapLoader::Preload_SessionData(
                    _device,
                    _context,
                    targetChunk.path,
                    _mapStreamingDesc.levelIndex,
                    _mapStreamingDesc.layerTag,
                    targetChunk.session);

                if (SUCCEEDED(hr))
                    targetChunk.preloadFinished->store(true);
                else
                    targetChunk.preloadFailed->store(true);
            });

            if (enqueued)
            {
                LOG_DEBUG("[StreamingLevel] Dispatched background thread preload for chunk=({}, {})", chunk.gridX, chunk.gridZ);
                break;
            }
            else
            {
                LOG_ERROR("[StreamingLevel] Failed to dispatch thread job for chunk=({}, {})", chunk.gridX, chunk.gridZ);
                chunk.state = ChunkLoadState::Unloaded;
                _preloadingChunkIndex = -1;
                continue;
            }
        }
    }

    // 2. 비동기 백그라운드 스레드의 성공/실패 여부를 모니터링하여 메인 스레드 바톤 터치
    if (_preloadingChunkIndex >= 0)
    {
        StreamChunk& preloadingChunk = _mapChunks[_preloadingChunkIndex];
        if (preloadingChunk.preloadFinished->load())
        {
            preloadingChunk.state = ChunkLoadState::Loading;
            preloadingChunk.loadedObjects.clear();
            preloadingChunk.session.spawnedObjects = &preloadingChunk.loadedObjects;

            _loadingChunkIndex = _preloadingChunkIndex;
            _preloadingChunkIndex = -1;

            LOG_DEBUG("[StreamingLevel] Preload successfully finished for chunk=({}, {}). Commencing main thread spawn.",
                preloadingChunk.gridX, preloadingChunk.gridZ);
        }
        else if (preloadingChunk.preloadFailed->load())
        {
            preloadingChunk.state = ChunkLoadState::Unloaded;
            preloadingChunk.session = {};
            _preloadingChunkIndex = -1;

            LOG_ERROR("[StreamingLevel] Async preloading failed for chunk=({}, {})",
                preloadingChunk.gridX, preloadingChunk.gridZ);
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
            "[StreamingLevel] Process load session failed for chunk=({}, {})",
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
        "[StreamingLevel] Fully loaded chunk=({}, {}) spawned_objects={}",
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

void StreamingLevel::Update_ActiveChunks(int32 centerCol, int32 centerRow)
{
    int32 radius = _mapStreamingDesc.activeChunkRadius;

    // 1. 활성화 1반경 내 청크 로딩 대기열 큐 삽입
    int32 minCol = max(0, centerCol - radius);
    int32 maxCol = min(_mapStreamingDesc.gridWidth - 1, centerCol + radius);
    int32 minRow = max(0, centerRow - radius);
    int32 maxRow = min(_mapStreamingDesc.gridHeight - 1, centerRow + radius);

    for (int32 r = minRow; r <= maxRow; ++r)
    {
        for (int32 c = minCol; c <= maxCol; ++c)
        {
            Queue_MapChunk(c, r);
        }
    }

    // 2. 활성화 1반경 외 청크 실시간 디스폰(메모리 환수) 및 대기열 이탈 처리
    for (int32 i = 0; i < static_cast<int32>(_mapChunks.size()); ++i)
    {
        StreamChunk& chunk = _mapChunks[i];
        int32 distCol = abs(chunk.gridX - centerCol);
        int32 distRow = abs(chunk.gridZ - centerRow);

        if (distCol > radius || distRow > radius)
        {
            if (chunk.state == ChunkLoadState::Loaded)
            {
                // loadedObjects가 비어있으면 Loader가 스폰한 청크 → 언로드 대상에서 제외
                if (!chunk.loadedObjects.empty())
                    Unload_MapChunk(i);
            }
            else if (chunk.state == ChunkLoadState::Queued)
            {
                auto it = std::find(_mapChunkLoadQueue.begin(), _mapChunkLoadQueue.end(), i);
                if (it != _mapChunkLoadQueue.end())
                {
                    _mapChunkLoadQueue.erase(it);
                }
                chunk.state = ChunkLoadState::Unloaded;
                LOG_DEBUG("[StreamingLevel] Queue canceled for out-of-bounds chunk=({}, {})", chunk.gridX, chunk.gridZ);
            }
        }
    }
}
