#include "Loader.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "ResourceLoader.h"
#include "BackGround.h"
#include "Terrain.h"
#include "FreeCamera.h"
#include "SL_Dualliste.h"
#include "MW_Lancelier.h"
#include "Player_WorldBase.h"
#include "Body_Player.h"
#include "Weapon.h"
#include "Sky.h"
#include "MapLandscape.h"

// 레벨 추가되면 여기에 추가로 세팅해줘야함
auto Loader::Find_MapLoadDesc() const -> const LevelMapLoadDesc*
{
    static const LevelMapLoadDesc mapLoadDescs[] =
    {
        // GamePlay: 로딩 화면에서 에셋 캐시 워밍만 수행. 실제 스폰은 StreamingLevel이 담당.
        {
            LevelType::GamePlay,
            {
                Constants::Map::SmallLevel_MF_Zone_01_Path
            },
            1,
            L"Layer_MapStaticMesh"
        },

        { LevelType::MapTest,  { Constants::Map::TestMap_Path }, 1, L"Layer_MapStaticMesh" },

        { LevelType::Stage01,  { Constants::Map::SmallLevel_SimonArea_Remake_Path }, 1, L"Layer_MapStaticMesh" },

        { LevelType::GestralVillage, { Constants::Map::GestralVillage_MapInstance_Path }, 1, L"Layer_MapStaticMesh" }
    };

    for (const LevelMapLoadDesc& desc : mapLoadDescs)
    {
        if (desc.level == _nextLevel)
            return &desc;
    }

    return nullptr;
}

auto Loader::Find_LandscapeDesc() const -> const LevelLandscapeDesc*
{
    static const LevelLandscapeDesc descs[] =
    {
        {
            LevelType::GamePlay,
            {
                Constants::Map::MF_Zone_Boss_HMap,
                Constants::Map::MF_Zone_Main_HMap
                //, Constants::Map::MF_Zone_Stair_1_HMap,
                //Constants::Map::MF_Zone_Stair_2_HMap,
                //Constants::Map::MF_Zone_Stair_3_HMap,
            },
            2,
            Constants::Map::ResourceRoot_Dir,
            L"Layer_BackGround"
        },

        {
            LevelType::GestralVillage,
            {
                Constants::Map::GestralVillage_HMap
            },
            1,
            Constants::Map::ResourceRoot_Dir,
            L"Layer_BackGround"
        },
    };

    for (const LevelLandscapeDesc& desc : descs)
    {
        if (desc.level == _nextLevel)
        {
            return &desc;
        }
    }

    return nullptr;
}

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
                _hasFailed.store(true);

            // 리소스 job 목록 생성뿐 아니라 맵 프리로드까지 끝난 뒤에만 완료로 공개한다.
            _workerDone.store(true);
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

    switch (_nextLevel)
    {
    case LevelType::MainTitle:
        hr = Loading_For_LogoLevel();
        break;

    case LevelType::GameEntry:
        hr = Loading_For_GameEntryLevel();
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

    case LevelType::GestralVillage:
        hr = Loading_For_GestralVillageLevel();
        break;

    default:
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        // 맵 백그라운드 프리로드 수행 (메인 스레드 대기 차단 및 멀티스레드 활용)
        const LevelMapLoadDesc* desc = Find_MapLoadDesc();
        if (desc != nullptr)
        {
            lstrcpy(_loadingText, TEXT("Map Resources Preloading..."));

            for (size_t i = 0; i < desc->mapPathCount; ++i)
            {
                if (desc->mapPaths[i] == nullptr)
                    continue;

                MapLoader::LoadSession session{};
                session.preloadedModelCache = _mapPreloadedModelCache;
                session.preloadedCollisionCache = _mapPreloadedCollisionCache;

                if (SUCCEEDED(MapLoader::Preload_SessionData(_device, _context, desc->mapPaths[i],
                    ETOI(desc->level), desc->layerTag, session)))
                {
                    _mapPreloadedModelCache.insert(session.preloadedModelCache.begin(), session.preloadedModelCache.end());
                    _mapPreloadedCollisionCache.insert(session.preloadedCollisionCache.begin(), session.preloadedCollisionCache.end());
                }
            }
        }
    }

    // 로딩 끝난 시간
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const auto levelName = magic_enum::enum_name(_nextLevel);

    LOG_INFO("[Loader::Loading] 레벨 : {}, 로딩 시간 : {}", levelName, Format_Duration(elapsed));

    if (FAILED(hr))
        _hasFailed.store(true);

    return hr;
}

HRESULT Loader::Process_PendingLoadJobs(
    size_t maxCommitPerFrame,
    size_t maxMainThreadCreatePerFrame,
    chrono::milliseconds maxMainThreadCreateTime)
{
    // 이미 완료됐거나, 실패한 상태면 더 처리할 작업이 없다.
    if (_isFinished.load() || _hasFailed.load())
        return S_OK;

#ifdef _DEBUG
    const auto processBegin = chrono::steady_clock::now();
    const auto scheduleBegin = processBegin;
#endif

    CHECK_FAILED(Schedule_PendingCreateJobs(maxMainThreadCreatePerFrame, maxMainThreadCreateTime), E_FAIL);

#ifdef _DEBUG
    const auto commitBegin = chrono::steady_clock::now();
#endif

    CHECK_FAILED(Commit_CompletedLoadJobs(maxCommitPerFrame), E_FAIL);

#ifdef _DEBUG
    const auto finishBegin = chrono::steady_clock::now();
#endif

    CHECK_FAILED(Try_FinishLoading(), E_FAIL);

#ifdef _DEBUG
    const auto processEnd = chrono::steady_clock::now();
    const auto scheduleMs = chrono::duration_cast<chrono::milliseconds>(commitBegin - scheduleBegin);
    const auto commitMs = chrono::duration_cast<chrono::milliseconds>(finishBegin - commitBegin);
    const auto finishMs = chrono::duration_cast<chrono::milliseconds>(processEnd - finishBegin);
    const auto totalMs = chrono::duration_cast<chrono::milliseconds>(processEnd - processBegin);

    ++_loadProcessStats.count;
    _loadProcessStats.totalDuration += totalMs;
    _loadProcessStats.scheduleDuration += scheduleMs;
    _loadProcessStats.commitDuration += commitMs;
    _loadProcessStats.finishDuration += finishMs;

    if (_isFinished.load() && !_printedLoadSummary)
    {
        Print_LoadJobSummary();
        _printedLoadSummary = true;
    }
#endif

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
    lstrcpy(_loadingText, TEXT("MainTitle 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : MainTitle");
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

HRESULT Loader::Loading_For_GameEntryLevel()
{
    lstrcpy(_loadingText, TEXT("GameEntry 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : GameEntry");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Loader::Loading_For_MapTestLevel()
{
    lstrcpy(_loadingText, TEXT("MapTest 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : MainTitle");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Loader::Loading_For_Stage01Level()
{
    lstrcpy(_loadingText, TEXT("Stage01 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : MainTitle");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Loader::Loading_For_GestralVillageLevel()
{
    lstrcpy(_loadingText, TEXT("GestralVillage 로딩 중..."));

    if (FAILED(Load_ResourceJobsForNextLevel()))
    {
        MSG_BOX("Faild to Load Resource Tables : GestralVillage");
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
#ifdef _DEBUG
    LOG_DEBUG("[Loader::Summary] ---- LoadJob Summary Begin ----");

    LOG_DEBUG(
        "[Loader::Summary] processCount={}, total={}, schedule={}, commit={}, finish={}, progress={:.2f}%%",
        static_cast<int>(_loadProcessStats.count),
        Format_Duration(_loadProcessStats.totalDuration),
        Format_Duration(_loadProcessStats.scheduleDuration),
        Format_Duration(_loadProcessStats.commitDuration),
        Format_Duration(_loadProcessStats.finishDuration),
        Get_Progress() * 100.f);

    for (size_t i = 0; i < _loadJobStats.size(); ++i)
    {
        const LoadJobStats& stats = _loadJobStats[i];
        if (stats.count == 0)
            continue;

        const ELoadJobType jobType = static_cast<ELoadJobType>(i);
        const string jobTypeName = string(magic_enum::enum_name(jobType));

        LOG_DEBUG("[Loader::Summary] type={}, count={}, createTotal={}",
            jobTypeName,
            static_cast<int>(stats.count),
            Format_Duration(stats.totalDuration)
        );
    }

    LOG_DEBUG("[Loader::Summary] ---- LoadJob Summary End ----");
#endif
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

HRESULT Loader::Schedule_PendingCreateJobs(size_t maxMainThreadCreatePerFrame, chrono::milliseconds maxMainThreadCreateTime)
{
    deque<FLoadJob> jobsToSchedule;

    {
        lock_guard<mutex> lock(_pendingJobsMutex);

        if (_pendingJobs.empty())
            return S_OK;

        jobsToSchedule.swap(_pendingJobs);
    }

    deque<FLoadJob> deferredJobs;

    size_t mainThreadCreateCount = 0;
    const auto mainThreadCreateBegin = chrono::steady_clock::now();

    while (!jobsToSchedule.empty())
    {
        FLoadJob loadJob = std::move(jobsToSchedule.front());
        jobsToSchedule.pop_front();

        // 이미 등록된 prototype이면 완료 처리만 한다.
        if (GAME->Has_Prototype(loadJob.iLevelIndex, loadJob.strPrototypeTag))
        {
            _completedJobCount.fetch_add(1);
            continue;
        }

        if (!ResourceLoader::Can_CreatePrototypeOnWorker(loadJob))
        {
            const auto mainThreadCreateElapsed = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - mainThreadCreateBegin);

            const bool countBudgetExceeded =
                maxMainThreadCreatePerFrame == 0 ||
                mainThreadCreateCount >= maxMainThreadCreatePerFrame;
            const bool timeBudgetExceeded =
                maxMainThreadCreateTime.count() > 0 &&
                mainThreadCreateCount > 0 &&
                mainThreadCreateElapsed >= maxMainThreadCreateTime;

            if (countBudgetExceeded || timeBudgetExceeded)
            {
                deferredJobs.push_back(std::move(loadJob));
                continue;
            }

            const auto startTime = chrono::steady_clock::now();

            Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
            CHECK_NULL(resourceLoader, E_FAIL);

            Shared<Base> prototype = nullptr;
            const HRESULT hr = resourceLoader->Create_LoadJobPrototype(loadJob, prototype);

            const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - startTime);

            CHECK_FAILED(Push_CompletedLoadJob(loadJob, prototype, hr, elapsed), E_FAIL);
            ++mainThreadCreateCount;
            continue;
        }

        _activeCreateJobCount.fetch_add(1);

        const bool enqueued = GAME->Enqueue_ThreadJob([this, loadJob]()
        {
            const auto startTime = chrono::steady_clock::now();

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

    if (!deferredJobs.empty())
    {
        lock_guard<mutex> lock(_pendingJobsMutex);
        _pendingJobs.insert(
            _pendingJobs.begin(),
            make_move_iterator(deferredJobs.begin()),
            make_move_iterator(deferredJobs.end()));
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
        {
            jobsToCommit.push_back(std::move(_completedJobs.front()));
            _completedJobs.pop_front();
        }
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
        _includedStaticJobs.store(false);
    }

    // 백그라운드 프리로드가 끝난 후, 메인 스레드 스폰 단계를 60 FPS(16ms) 예산에 맞춰 점진적 수행하여 화면 멈춤 제거
    chrono::milliseconds mapLoadBudgetMs = chrono::milliseconds(16);
    size_t mapLoadMeshesPerFrame = 20;

    if (_nextLevel == LevelType::MapTest)
    {
        mapLoadMeshesPerFrame = 4;              // TestMap은 로딩 화면 hitch를 줄이기 위해 더 잘게 쪼갠다.
        mapLoadBudgetMs = chrono::milliseconds(4);
    }

    CHECK_FAILED(Process_LevelMapLoad(mapLoadMeshesPerFrame, mapLoadBudgetMs), E_FAIL);

    if (!_mapLoadFinished)
        return S_OK;

    // land
    CHECK_FAILED(Process_LevelLandscapeLoad(), E_FAIL);

    if (!_landscapeLoadFinished)
        return S_OK;
    //

    lstrcpy(_loadingText, TEXT("Loading completed."));
    _isFinished.store(true);

    return S_OK;
}

HRESULT Loader::Process_LevelMapLoad(size_t maxMeshesPerFrame, chrono::milliseconds maxTimePerFrame)
{
    const LevelMapLoadDesc* desc = Find_MapLoadDesc();

    if (desc == nullptr)
    {
        _mapLoadFinished = true;
        return S_OK;
    }

    if (_mapLoadPathIndex >= desc->mapPathCount)
    {
        _mapLoadFinished = true;
        return S_OK;
    }

    if (!_mapLoadStarted)
    {
        LOG_DEBUG(
            "[Loader::MapLoad] begin {}/{} path={}",
            _mapLoadPathIndex + 1,
            desc->mapPathCount,
            String::ToString(desc->mapPaths[_mapLoadPathIndex]));

        CHECK_FAILED(
            MapLoader::Begin_LoadSession(_device, _context, desc->mapPaths[_mapLoadPathIndex],
                ETOI(desc->level), desc->layerTag, _mapLoadSession), E_FAIL);

        _mapLoadSession.preloadedModelCache = _mapPreloadedModelCache;
        _mapLoadSession.preloadedCollisionCache = _mapPreloadedCollisionCache;
        _mapLoadStarted = true;
    }

    bool finished = false;
    CHECK_FAILED(
        MapLoader::Process_LoadSession(_mapLoadSession,
            maxMeshesPerFrame, maxTimePerFrame, finished), E_FAIL);

    if (finished)
    {
        _mapPreloadedModelCache.insert(
            _mapLoadSession.preloadedModelCache.begin(),
            _mapLoadSession.preloadedModelCache.end());
        _mapPreloadedCollisionCache.insert(
            _mapLoadSession.preloadedCollisionCache.begin(),
            _mapLoadSession.preloadedCollisionCache.end());

        LOG_DEBUG(
            "[Loader::MapLoad] finished {}/{}",
            _mapLoadPathIndex + 1,
            desc->mapPathCount);

        ++_mapLoadPathIndex;
        _mapLoadStarted = false;
        _mapLoadSession = {};
        _mapLoadFinished = _mapLoadPathIndex >= desc->mapPathCount;
    }

    return S_OK;
}

HRESULT Loader::Process_LevelLandscapeLoad()
{
    const LevelLandscapeDesc* desc = Find_LandscapeDesc();
    if (!desc)
    {
        _landscapeLoadFinished = true;
        return S_OK;
    }

    if (_landscapeLoadIndex >= desc->hmapPathCount)
    {
        _landscapeLoadFinished = true;
        return S_OK;
    }

    MapLandscape::LANDSCAPE_DESC landscapeDesc{};
    landscapeDesc.hmapPath = desc->hmapPaths[_landscapeLoadIndex];
    landscapeDesc.texRoot = desc->texRoot;

    GAME->Add_GameObject(
        ETOI(LevelType::Static),
        TEXT("GameObject_MapLandscape"),
        ETOI(desc->level),
        desc->layerTag,
        &landscapeDesc);

    ++_landscapeLoadIndex;

    if (_landscapeLoadIndex >= desc->hmapPathCount)
    {
        _landscapeLoadFinished = true;
    }

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
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    __super::Free();
}
