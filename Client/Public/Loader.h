#pragma once

#include "Client_Defines.h"
#include "Base.h"
#include "MapLoader.h"
#include "ResourceLoader.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(Client)

class Loader final : public Base
{
private:
    struct LevelMapLoadDesc
    {
        LevelType level = LevelType::END;
        array<const wchar_t*, 4> mapPaths = {};
        size_t mapPathCount = 0;
        const wchar_t* layerTag = nullptr;
    };

    // land
    struct LevelLandscapeDesc
    {
        LevelType level = LevelType::END;
        array<const WCHAR*, 8> hmapPaths = {};
        size_t hmapPathCount = {};
        const WCHAR* texRoot = {};
        const WCHAR* layerTag = {};
    };

public:
    Loader(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Loader() = default;

public:
    // - worker thread 여기에서 시작
    HRESULT Initialize(LevelType eNextLevelID);
    // - worker thread 진입점이 호출하는 실제 작업 함수
    HRESULT Loading();

    HRESULT Process_PendingLoadJobs(
        size_t maxCommitPerFrame,
        size_t maxMainThreadCreatePerFrame = 2,
        chrono::milliseconds maxMainThreadCreateTime = chrono::milliseconds(3));

    bool isFinished() { return _isFinished.load(); }
    bool Has_Failed() const { return _hasFailed.load(); }

    // 진행률. 그런데 33원정대 로딩에서 Progress Bar 없던거같은데
    float Get_Progress() const;

    bool Is_WorkerDone() const
    {
        return (_workerDone.load() && _activeCreateJobCount.load() == 0) || _hasFailed.load();
    }


#ifdef _DEBUG
public:
    HRESULT Print_LoadingText();
#endif

private:
    HRESULT Append_ResourceJobsForLevel(LevelType levelType);
    HRESULT Load_ResourceJobsForLevel(LevelType levelType);
    HRESULT Load_ResourceJobsForLevels(const vector<LevelType>& levelTypes);
    HRESULT Load_ResourceJobsForNextLevel();

    HRESULT Loading_For_LogoLevel();
    HRESULT Loading_For_GameEntryLevel();
    HRESULT Loading_For_GamePlayLevel();
    HRESULT Loading_For_MapTestLevel();
    HRESULT Loading_For_Stage01Level();
    HRESULT Loading_For_GestralVillageLevel();

private:
    void Accumulate_LoadJobStats(ELoadJobType jobType, chrono::milliseconds duration);
    void Print_LoadJobSummary();

private:
    static string Format_Duration(chrono::milliseconds duration);

    HRESULT Schedule_PendingCreateJobs(size_t maxMainThreadCreatePerFrame, chrono::milliseconds maxMainThreadCreateTime);
    HRESULT Push_CompletedLoadJob(
        const FLoadJob& loadJob,
        const Shared<Base>& prototype,
        HRESULT result,
        chrono::milliseconds elapsed);

    HRESULT Commit_CompletedLoadJobs(size_t maxCommitPerFrame);
    HRESULT Try_FinishLoading();

private: /* 맵 로딩 */
    HRESULT         Process_LevelMapLoad(size_t maxMeshesPerFrame, chrono::milliseconds maxTimePerFrame);

    const LevelMapLoadDesc* Find_MapLoadDesc() const;

    HRESULT Process_LevelLandscapeLoad();
    const LevelLandscapeDesc* Find_LandscapeDesc() const;

private:
    struct LoadJobStats
    {
        size_t count = 0;
        chrono::milliseconds totalDuration{ 0 };
    };

    struct LoadProcessStats
    {
        size_t count = 0;
        chrono::milliseconds totalDuration{ 0 };
        chrono::milliseconds scheduleDuration{ 0 };
        chrono::milliseconds commitDuration{ 0 };
        chrono::milliseconds finishDuration{ 0 };
    };

    struct CompletedLoadJob
    {
        FLoadJob loadJob{};
        Shared<Base> prototype = nullptr;
        HRESULT result = S_OK;
        chrono::milliseconds elapsed{};
    };

private:
    ComPtr<Device>              _device = {};
    ComPtr<Context>             _context = {};

    LevelType					_nextLevel = { LevelType::END };

    atomic<bool>                _workerStarted = false;

    //HANDLE					_threadHandle = {};
    //CRITICAL_SECTION			_criticalSection = {}; 이제 사용 안할듯?

    // worker thread 완료 여부
    atomic<bool>                _workerDone = false;
    atomic<bool>                _isFinished = false;
    atomic<bool>                _hasFailed = false;
    atomic<bool>                _includedStaticJobs = false;
    bool                        _printedLoadSummary = false;

    wchar_t						_loadingText[MAX_PATH] = {};

    // worker가 만들어 놓은 job 목록
    mutex               _pendingJobsMutex;
    deque<FLoadJob>     _pendingJobs;

    // 진행률 계산용
    atomic<size_t>      _totalJobCount = 0;
    atomic<size_t>      _completedJobCount = 0;

    array<LoadJobStats, static_cast<size_t>(ELoadJobType::Unknown) + 1> _loadJobStats = {};
    LoadProcessStats _loadProcessStats = {};

private:
    // worker thread에서 prototype 생성이 끝난 job들
    // main thread가 여기서 꺼내서 GAME->Add_Prototype이 생성되게
    mutex               _completedJobsMutex;
    deque<CompletedLoadJob>  _completedJobs;

    // 현재 스레드풀에서 prototype 생성중인 job 수
    atomic<size_t>      _activeCreateJobCount = 0;

private: /* 맵 로딩 */
    bool _mapLoadStarted = false;
    bool _mapLoadFinished = false;
    size_t _mapLoadPathIndex = 0;
    MapLoader::LoadSession _mapLoadSession{};
    umap<string, Shared<ModelCom>> _mapPreloadedModelCache = {};
    umap<string, Shared<StaticCollisionData>> _mapPreloadedCollisionCache = {};

    // add
    bool _landscapeLoadFinished = false;
    size_t _landscapeLoadIndex = {};

public:
    static Shared<Loader> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, LevelType eNextLevelID);
    virtual void Free() override;

};

NS_END





