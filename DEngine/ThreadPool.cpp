#include "pch.h"
#include "ThreadPool.h"

ThreadPool::~ThreadPool()
{
    Shutdown();
}

/// <summary>
/// Thread 를 threadCount 수만큼 생성
/// </summary>
/// <param name="threadCount"></param>
void ThreadPool::Init(uint32 threadCount)
{
    _stop = false;

    for (uint32 i = 0; i < threadCount; ++i)
    {
        _workers.emplace_back(
            [this]()
            {
                WorkerLoop();
            }
        );
    }
}

void ThreadPool::Shutdown()
{
    {
        lock_guard<mutex> lock(_mutex);
        _stop = true;
    }

    _condition.notify_all();

    for (thread& worker : _workers)
    {
        if (worker.joinable())
            worker.join();
    }

    _workers.clear();
}

void ThreadPool::Enqueue(function<void()> job)
{
    {
        lock_guard<mutex> lock(_mutex);
        _jobs.push(move(job));
    }   

    _condition.notify_one();
}

void ThreadPool::WaitIdle()
{
    unique_lock<mutex> lock(_mutex);

    _idleCondition.wait(lock,
        [this]()
        {
            return _jobs.empty() && _activeJobCount == 0;
        });
}

void ThreadPool::WorkerLoop()
{
    while (true)
    {
        function<void()> job;

        {
            unique_lock<mutex> lock(_mutex);

            _condition.wait(lock,
                [this]()
                {
                    return _stop || !_jobs.empty();
                });

            if (_stop && _jobs.empty())
                return;

            job = move(_jobs.front());
            _jobs.pop();

            ++_activeJobCount;
        }

        job();

        {
            lock_guard<mutex> lock(_mutex);

            --_activeJobCount;

            if (_jobs.empty() && _activeJobCount == 0)
                _idleCondition.notify_all();
        }
    }
}