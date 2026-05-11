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

    const bool enqueued = GAME->Enqueue_ThreadJob(
        [this]
        {
            const HRESULT hr = Loading();

            if (FAILED(hr))
            {
                _hasFailed.store(true);
                _workerDone.store(true);
            }
        }
    );

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

    HRESULT hr = {};

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

HRESULT Loader::Process_PendingLoadJobs(size_t maxJobPerFrame)
{
    // 이미 완료됐거나, 실패한 상태면 더 처리할 작업이 없다.
    if (_isFinished.load() || _hasFailed.load())
        return S_OK;

    if (maxJobPerFrame == 0)
        return S_OK;

    const Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
    if (resourceLoader == nullptr)
    {
        LOG_ERROR("[Loader::Process_PendingLoadJobs] ResourceLoader::Create failed");
        _hasFailed.store(true);
        return E_FAIL;
    }

    vector<FLoadJob> loadJobsToProcess;;

    {
        // 이번 프레임에 처리할 양만 꺼내기
        lock_guard<mutex> lock(_pendingJobsMutex);

        const size_t jobCount = min(maxJobPerFrame, _pendingJobs.size());
        loadJobsToProcess.reserve(jobCount);

        for (size_t i = 0; i < jobCount; ++i)
            loadJobsToProcess.push_back(_pendingJobs[i]);

        // 이번 프레임에 처리할 job들은 local vector로 복사했으니까 pending 목록에서는 제외
        _pendingJobs.erase(_pendingJobs.begin(), _pendingJobs.begin() + jobCount);
    }

    // 실제 무거운 작업은 lock 밖에서 진행
    for (const FLoadJob& loadJob : loadJobsToProcess)
    {
        const auto startTime = chrono::steady_clock::now();

        if (FAILED(resourceLoader->Register_LoadJob(loadJob)))
        {
            LOG_ERROR(
                "[Loader::Process_PendingLoadJobs] Register_LoadJob failed. tag={}, level={}",
                String::ToString(loadJob.strPrototypeTag),
                loadJob.iLevelIndex
            );

            _hasFailed.store(true);
            return E_FAIL;
        }

        const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - startTime
        );

        Accumulate_LoadJobStats(loadJob.eType, elapsed);

        const string jobTypeName = string(magic_enum::enum_name(loadJob.eType));

        LOG_ERROR(
            "[Loader::Process_PendingLoadJobs] Processed load job. tag={}, type={}, elapsed={}",
            String::ToString(loadJob.strPrototypeTag),
            jobTypeName,
            Format_Duration(elapsed)
        );

        _completedJobCount.fetch_add(1);
    }

    // worker thread가 job 생성을 끝냈고, main thread에도 pending job을 모두 처리했으면 이제 로딩 완료
    if (_workerDone.load())
    {
        lock_guard<mutex> lock(_pendingJobsMutex);

        if (_pendingJobs.empty())
        {
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
        }
    }

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

HRESULT Loader::Append_ResourceJobsForLevel(LevelType levelType)
{
    const Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
    if (resourceLoader == nullptr)
    {
        LOG_ERROR("[Loader::Load_ResourceJobsForLevel] ResourceLoader::Create failed");
        return E_FAIL;
    }

    vector<FLoadJob> builtJobs;
    if (FAILED(resourceLoader->Build_LevelJobs(levelType, builtJobs)))
    {
        LOG_ERROR(
            "[Loader::Load_ResourceJobsForLevel] Build_LevelJobs failed. level={}",
            static_cast<int>(levelType)
        );
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

    for (const LevelType levelType : levelTypes)
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

        LOG_ERROR(
            "[Loader::Print_LoadJobSummary] type={}, count={}, total={}",
            jobTypeName,
            static_cast<int>(stats.count),
            Format_Duration(stats.totalDuration)
        );
    }

    LOG_ERROR("[Loader::Print_LoadJobSummary] ---- LoadJob Summary End ----");
}

string Loader::Format_Duration(chrono::milliseconds duration)
{
    using namespace std::chrono;

    const auto totalMs = duration.count();

    const auto hours = totalMs / (1000 * 60 * 60);
    const auto minutes = totalMs / (1000 * 60) % 60;
    const auto seconds = totalMs / 1000 % 60;
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
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    __super::Free();
}
