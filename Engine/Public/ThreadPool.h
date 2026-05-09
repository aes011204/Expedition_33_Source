#pragma once

#include "Base.h"

NS_BEGIN(Engine)

// 미리 생성한 worker thread들이 job queue의 작업을 나눠 처리하는 비동기 실행 시스템
// thread 생성/소멸 비용을 줄이고, 백그라운드의 반복작업을 재사용 가능하게
class ThreadPool : public Base
{
public:
    ThreadPool() = default;
    ~ThreadPool() override = default;

public:
    HRESULT Initialize(size_t threadCount);

    // 작업 하나를 thread pool에 넣는다.
    // job은 worker thread 중 하나가 나중에 실행된다.
    bool Enqueue(function<void()> job);

    // 모든 worker thread 종료. Engine shutdown 시 반드시 호출해야함
    void Stop();
    size_t Get_PendingJobCount() const;

private:
    void Worker_Main();

private:
    vector<thread> _workers;
    queue<function<void()>> _jobs;

    mutable mutex _mutex;
    condition_variable _cv;

    bool _isStopping = false;

public:
    static Unique<ThreadPool> Create(size_t threadCount);
    void Free() override;

};

NS_END
