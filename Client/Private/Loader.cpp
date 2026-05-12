#include "Loader.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "ResourceLoader.h"
#include "BackGround.h"
#include "Terrain.h"
#include "FreeCamera.h"
#include "Monster.h"
#include "Player.h"
#include "Body_Player.h"
#include "Weapon.h"
#include "Sky.h"

Loader::Loader(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT Loader::Initialize(LevelType eNextLevelID)
{
    _nextLevel = eNextLevelID;

    _workerStarted.store(true);

    const bool enqueued = GAME->Enqueue_ThreadJob([this]()
    {
        const HRESULT hr = Loading();

        if (FAILED(hr))
        {
            _hasFailed.store(true);
            _workerDone.store(true);
        }
    });

    if (!enqueued)
    {
        _workerStarted.store(false);
        _hasFailed.store(true);
        _workerDone.store(true);

        LOG_ERROR("[Loader::Initialize] Failed to enqueue loading job.");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Loader::Loading()
{
    // 로딩 시작 시간
    const auto start = std::chrono::steady_clock::now();

    HRESULT		hr = {};

    //EnterCriticalSection(&_criticalSection);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    switch (_nextLevel)
    {
    case LevelType::Logo:
        hr = Loading_For_LogoLevel();
        break;

    case LevelType::GamePlay:
        hr = Loading_For_GamePlayLevel();
        break;

    case LevelType::MapTest:
        hr = Loading_For_MapTestLevel();
        break;

    case LevelType::Stage01:
        hr = Loading_For_Stage01Level();
        break;

    default:
        hr = E_FAIL;
    }

    CoUninitialize();
    //LeaveCriticalSection(&_criticalSection);

    // 로딩 끝난 시간
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const auto levelName = magic_enum::enum_name(_nextLevel);

    LOG_ERROR("[Loader::Loading] 레벨 : {}, 로딩 시간 : {}", levelName, Format_Duration(elapsed));

    if (FAILED(hr))
        _hasFailed.store(true);

    return hr;
}

HRESULT Loader::Process_PendingLoadJobs(size_t maxCommitPerFrame)
{
    // 이미 완료됐거나, 실패한 상태면 더 처리할 작업이 없다.
    if (_isFinished.load() || _hasFailed.load())
        return S_OK;

    CHECK_FAILED(Schedule_PendingCreateJobs(), E_FAIL);
    CHECK_FAILED(Commit_CompletedLoadJobs(maxCommitPerFrame), E_FAIL);
    CHECK_FAILED(Try_FinishLoading(), E_FAIL);

    return S_OK;
}

float Loader::Get_Progress() const
{
    const size_t totalJobCount = _totalJobCount.load();
    const size_t completedJobCount = _completedJobCount.load();

    // 아직 job 목록 만들기 전
    if (totalJobCount == 0)
        return _isFinished.load() ? 1.f : 0.f;

    return static_cast<float>(completedJobCount) / totalJobCount;
}

#ifdef _DEBUG
HRESULT Loader::Print_LoadingText()
{
    SetWindowText(g_hWnd, _loadingText);

    return S_OK;
}
#endif

HRESULT Loader::Loading_For_LogoLevel()
{
    lstrcpy(_loadingText, TEXT("Logo 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : Logo");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Loader::Loading_For_GamePlayLevel()
{
    lstrcpy(_loadingText, TEXT("GamePlay 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : GamePlay");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Loader::Loading_For_MapTestLevel()
{
    lstrcpy(_loadingText, TEXT("MapTest 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : Logo");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Loader::Loading_For_Stage01Level()
{
    lstrcpy(_loadingText, TEXT("Stage01 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : Logo");
        return E_FAIL;
    }

    return S_OK;
}

void Loader::Accumulate_LoadJobStats(ELoadJobType jobType, chrono::milliseconds duration)
{
    const size_t index = static_cast<size_t>(jobType);

    if (index >= _loadJobStats.size())
        return;

    LoadJobStats& stats = _loadJobStats[index];
    ++stats.count;
    stats.totalDuration += duration;
}

void Loader::Print_LoadJobSummary()
{
    LOG_ERROR("[Loader::Print_LoadJobSummary] ---- LoadJob Summary Begin ----");

    for (size_t i = 0; i < _loadJobStats.size(); ++i)
    {
        const LoadJobStats& stats = _loadJobStats[i];
        if (stats.count == 0)
            continue;

        const ELoadJobType jobType = static_cast<ELoadJobType>(i);
        const string jobTypeName = string(magic_enum::enum_name(jobType));

        LOG_ERROR("[Loader::Print_LoadJobSummary] type={}, count={}, total={}",
            jobTypeName,
            static_cast<int>(stats.count),
            Format_Duration(stats.totalDuration));
    }

    LOG_ERROR("[Loader::Print_LoadJobSummary] ---- LoadJob Summary End ----");
}


HRESULT Loader::Append_ResourceJobsForLevel(LevelType levelType)
{
    Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
    if (resourceLoader == nullptr)
    {
        LOG_ERROR("[Loader::Load_ResourceJobsForLevel] ResourceLoader::Create failed");
        return E_FAIL;
    }

    vector<FLoadJob> builtJobs;
    if (FAILED(resourceLoader->Build_LevelJobs(levelType, builtJobs)))
    {
        LOG_ERROR("[Loader::Load_ResourceJobsForLevel] Build_LevelJobs failed. level={}",
            static_cast<int>(levelType));
        return E_FAIL;
    }

    {
        lock_guard<mutex> lock(_pendingJobsMutex);
        _pendingJobs.insert(_pendingJobs.end(), builtJobs.begin(), builtJobs.end());
    }

    _totalJobCount.fetch_add(builtJobs.size());

    //LOG_ERROR("[Loader::Load_ResourceJobsForLevel] level={}, totalJobs={}",
    //    static_cast<int>(levelType),
    //    static_cast<int>(builtJobs.size()));

    return S_OK;
}

HRESULT Loader::Load_ResourceJobsForLevel(LevelType levelType)
{
    return Load_ResourceJobsForLevels({ levelType });
}

HRESULT Loader::Load_ResourceJobsForLevels(const vector<LevelType>& levelTypes)
{
    _completedJobCount.store(0);
    _totalJobCount.store(0);

    {
        lock_guard<mutex> lock(_pendingJobsMutex);
        _pendingJobs.clear();
    }

    for (LevelType levelType : levelTypes)
    {
        if (FAILED(Append_ResourceJobsForLevel(levelType)))
            return E_FAIL;
    }

    _workerDone.store(true);
    return S_OK;
}

HRESULT Loader::Load_ResourceJobsForNextLevel()
{
    vector<LevelType> levelTypes;

    if (!CLIENT->Is_StaticResourcesLoaded())
    {
        levelTypes.push_back(LevelType::Static);
        _includedStaticJobs.store(true);
    }

    levelTypes.push_back(_nextLevel);

    return Load_ResourceJobsForLevels(levelTypes);
}

string Loader::Format_Duration(chrono::milliseconds duration)
{
    using namespace std::chrono;

    const auto totalMs = duration.count();

    const auto hours = totalMs / (1000 * 60 * 60);
    const auto minutes = (totalMs / (1000 * 60)) % 60;
    const auto seconds = (totalMs / 1000) % 60;
    const auto millisecondsPart = totalMs % 1000;

    std::ostringstream oss;

    if (hours > 0)
    {
        oss << hours << (hours == 1 ? " hour" : " hours");

        if (minutes > 0)
            oss << ' ' << minutes << (minutes == 1 ? " minute" : " minutes");

        if (seconds > 0)
            oss << ' ' << seconds << (seconds == 1 ? " second" : " seconds");

        return oss.str();
    }

    if (minutes > 0)
    {
        oss << minutes << (minutes == 1 ? " minute" : " minutes");

        if (seconds > 0)
            oss << ' ' << seconds << (seconds == 1 ? " second" : " seconds");

        return oss.str();
    }

    if (seconds > 0)
    {
        const double secondsWithFraction = static_cast<double>(totalMs) / 1000.0;
        oss.setf(std::ios_base::fixed);
        oss.precision(secondsWithFraction < 10.0 ? 1 : 0);
        oss << secondsWithFraction << (secondsWithFraction < 2.0 ? " second" : " seconds");
        return oss.str();
    }

    oss << totalMs << (totalMs == 1 ? " millisecond" : " milliseconds");
    return oss.str();
}

HRESULT Loader::Schedule_PendingCreateJobs()
{
    vector<FLoadJob> jobsToSchedule;

    {
        lock_guard<mutex> lock(_pendingJobsMutex);

        if (_pendingJobs.empty())
            return S_OK;

        jobsToSchedule = std::move(_pendingJobs);
        _pendingJobs.clear();
    }

    for (const FLoadJob& loadJob : jobsToSchedule)
    {
        // 이미 등록된 prototype이면 완료 처리만 한다.
        if (GAME->Has_Prototype(loadJob.iLevelIndex, loadJob.strPrototypeTag))
        {
            _completedJobCount.fetch_add(1);
            continue;
        }

        if (!ResourceLoader::Can_CreatePrototypeOnWorker(loadJob))
        {
            const auto startTime = chrono::steady_clock::now();

            Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
            CHECK_NULL(resourceLoader, E_FAIL);

            Shared<Base> prototype = nullptr;
            const HRESULT hr = resourceLoader->Create_LoadJobPrototype(loadJob, prototype);

            const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - startTime);

            CHECK_FAILED(Push_CompletedLoadJob(loadJob, prototype, hr, elapsed), E_FAIL);
            continue;
        }

        _activeCreateJobCount.fetch_add(1);

        const bool enqueued = GAME->Enqueue_ThreadJob([this, loadJob]()
        {
            const auto startTime = chrono::steady_clock::now();

            HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            const bool needCoUninitialize = SUCCEEDED(coHr);

            Shared<Base> prototype = nullptr;
            HRESULT hr = S_OK;

            Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
            if (resourceLoader == nullptr)
            {
                hr = E_FAIL;
            }
            else
            {
                hr = resourceLoader->Create_LoadJobPrototype(loadJob, prototype);
            }

            if (needCoUninitialize)
                CoUninitialize();

            const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - startTime);

            Push_CompletedLoadJob(loadJob, prototype, hr, elapsed);

            if (FAILED(hr))
                _hasFailed.store(true);

            _activeCreateJobCount.fetch_sub(1);
        });

        if (!enqueued)
        {
            _activeCreateJobCount.fetch_sub(1);
            _hasFailed.store(true);

            LOG_ERROR(
                "[Loader::Schedule_PendingCreateJobs] Failed to enqueue load job. tag={}, type={}",
                String::ToString(loadJob.strPrototypeTag),
                string(magic_enum::enum_name(loadJob.eType)));

            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT Loader::Push_CompletedLoadJob(
    const FLoadJob& loadJob,
    const Shared<Base>& prototype,
    HRESULT result,
    chrono::milliseconds elapsed)
{
    CompletedLoadJob completedJob{};
    completedJob.loadJob = loadJob;
    completedJob.prototype = prototype;
    completedJob.result = result;
    completedJob.elapsed = elapsed;

    {
        lock_guard<mutex> lock(_completedJobsMutex);
        _completedJobs.push_back(std::move(completedJob));
    }

    return S_OK;
}

HRESULT Loader::Commit_CompletedLoadJobs(size_t maxCommitPerFrame)
{
    if (maxCommitPerFrame == 0)
        return S_OK;

    vector<CompletedLoadJob> jobsToCommit;

    {
        lock_guard<mutex> lock(_completedJobsMutex);

        const size_t commitCount = min(maxCommitPerFrame, _completedJobs.size());
        jobsToCommit.reserve(commitCount);

        for (size_t i = 0; i < commitCount; ++i)
            jobsToCommit.push_back(std::move(_completedJobs[i]));

        _completedJobs.erase(_completedJobs.begin(), _completedJobs.begin() + commitCount);
    }

    if (jobsToCommit.empty())
        return S_OK;

    Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
    CHECK_NULL(resourceLoader, E_FAIL);

    for (const CompletedLoadJob& completedJob : jobsToCommit)
    {
        const FLoadJob& loadJob = completedJob.loadJob;

        if (FAILED(completedJob.result))
        {
            LOG_ERROR(
                "[Loader::Commit_CompletedLoadJobs] Create prototype failed. tag={}, type={}",
                String::ToString(loadJob.strPrototypeTag),
                string(magic_enum::enum_name(loadJob.eType)));

            _hasFailed.store(true);
            return E_FAIL;
        }

        CHECK_FAILED(
            resourceLoader->Commit_LoadJobPrototype(loadJob, completedJob.prototype),
            E_FAIL);

        Accumulate_LoadJobStats(loadJob.eType, completedJob.elapsed);

        LOG_ERROR(
            "[Loader::Commit_CompletedLoadJobs] Committed load job. tag={}, type={}, createElapsed={}",
            String::ToString(loadJob.strPrototypeTag),
            string(magic_enum::enum_name(loadJob.eType)),
            Format_Duration(completedJob.elapsed));

        _completedJobCount.fetch_add(1);
    }

    return S_OK;
}

HRESULT Loader::Try_FinishLoading()
{
    if (!_workerDone.load())
        return S_OK;

    if (_activeCreateJobCount.load() != 0)
        return S_OK;

    {
        lock_guard<mutex> pendingLock(_pendingJobsMutex);
        if (!_pendingJobs.empty())
            return S_OK;
    }

    {
        lock_guard<mutex> completedLock(_completedJobsMutex);
        if (!_completedJobs.empty())
            return S_OK;
    }

    if (_includedStaticJobs.load())
    {
        if (FAILED(CLIENT->Ready_StaticObjects()))
        {
            _hasFailed.store(true);
            return E_FAIL;
        }

        CLIENT->Set_StaticResourcesLoaded(true);
    }

    lstrcpy(_loadingText, TEXT("Loading completed."));
    _isFinished.store(true);

    Print_LoadJobSummary();

    return S_OK;
}

Shared<Loader> Loader::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, LevelType eNextLevelID)
{
    auto pInstance = make_shared<Loader>(device, context);

    if (FAILED(pInstance->Initialize(eNextLevelID)))
    {
        MSG_BOX("Failed to Created : Loader");
        return nullptr;
    }
    return pInstance;
}

void Loader::Free()
{
    while (_workerStarted.load() && !Is_WorkerDone())
        this_thread::sleep_for(chrono::milliseconds(1));

    __super::Free();
}





