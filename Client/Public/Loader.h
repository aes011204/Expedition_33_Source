#pragma once

#include "Client_Defines.h"
#include "Base.h"
#include "ResourceLoader.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(Client)

class Loader final : public Base
{
public:
    Loader(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Loader() = default;

public:
    // - worker thread 여기에서 시작
    HRESULT Initialize(LevelType eNextLevelID);
    // - worker thread 진입점이 호출하는 실제 작업 함수
    HRESULT Loading();

    HRESULT Process_PendingLoadJobs(size_t maxJobPerFrame);

    bool isFinished() { return _isFinished.load(); }
    bool Has_Failed() const { return _hasFailed.load(); }

    // 진행률. 그런데 33원정대 로딩에서 Progress Bar 없던거같은데
    float Get_Progress() const;

    bool Is_WorkerDone() const { return _workerDone.load() || _hasFailed.load(); }

#ifdef _DEBUG
public:
    HRESULT Print_LoadingText();
#endif

private:
    HRESULT Load_ResourceJobsForLevel(LevelType levelType);

    HRESULT Loading_For_LogoLevel();
    HRESULT Loading_For_GamePlayLevel();
    HRESULT Loading_For_MapTestLevel();
    HRESULT Loading_For_Stage01Level();

private:
    void Accumulate_LoadJobStats(ELoadJobType jobType, chrono::milliseconds duration);
    void Print_LoadJobSummary();

private:
    static string Format_Duration(chrono::milliseconds duration);

private:
    struct LoadJobStats
    {
        size_t count = 0;
        chrono::milliseconds totalDuration{0};
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

    wchar_t						_loadingText[MAX_PATH] = {};

    // worker가 만들어 놓은 job 목록
    mutex               _pendingJobsMutex;
    vector<FLoadJob>    _pendingJobs;

    // 진행률 계산용
    atomic<size_t>      _totalJobCount = 0;
    atomic<size_t>      _completedJobCount = 0;

    array<LoadJobStats, static_cast<size_t>(ELoadJobType::Unknown) + 1> _loadJobStats = {};

public:
    static Shared<Loader> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, LevelType eNextLevelID);
    virtual void Free() override;

};

NS_END





