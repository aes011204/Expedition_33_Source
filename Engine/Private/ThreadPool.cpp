#include "ThreadPool.h"

HRESULT ThreadPool::Initialize(size_t threadCount)
{
    // 최소 1개 보장
    threadCount = max<size_t>(1, threadCount);

    _isStopping = false;

    try
    {
        _workers.reserve(threadCount);

        for (size_t i = 0; i < threadCount; ++i)
        {
            _workers.emplace_back([this]()
            {
                Worker_Main();
            });
        }

    }
    catch (...)
    {
        Stop();
        return E_FAIL;
    }

    return S_OK;
}

bool ThreadPool::Enqueue(function<void()> job)
{
    if (!job)
        return false;

    {
        lock_guard<mutex> lock(_mutex);

        // 종료 중이면 새 작업 받지 않기
        if (_isStopping)
            return false;

        _jobs.push(std::move(job));
    }

    // 잠들어있는 worker 하나 깨우기
    _cv.notify_one();
    return true;
}

void ThreadPool::Stop()
{
    {
        lock_guard<mutex> lock(_mutex);
        _isStopping = true;
    }

    _cv.notify_all();

    for (thread& worker : _workers)
    {
        if (worker.joinable())
            worker.join();
    }

    _workers.clear();

    {
        lock_guard<mutex> lock(_mutex);

        while (!_jobs.empty())
            _jobs.pop();
    }
}

size_t ThreadPool::Get_PendingJobCount() const
{
    lock_guard<mutex> lock(_mutex);
    return _jobs.size();
}

void ThreadPool::Worker_Main()
{
    while (true)
    {
        function<void()> job;

        {
            unique_lock<mutex> lock(_mutex);

            // job이 들어오거나 종료 요청이 올 때까지 대기
            _cv.wait(lock, [this]()
            {
               return _isStopping || !_jobs.empty(); 
            });

            // 종료 요청이 왔고 남은 job도 없으면 worker thread 종료
            if (_isStopping && _jobs.empty())
                return;

            job = std::move(_jobs.front());
            _jobs.pop();
        }

        // 실제 작업은 lock 밖에서
        if (job)
            job();

    }
}

Unique<ThreadPool> ThreadPool::Create(size_t threadCount)
{
    auto instance = make_unique<ThreadPool>();

    if (FAILED(instance->Initialize(threadCount)))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void ThreadPool::Free()
{
    Stop();

    Base::Free();
}
