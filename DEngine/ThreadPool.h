#pragma once

class ThreadPool
{
public:
    ThreadPool() = default;
    ~ThreadPool();

    void Init(uint32 threadCount);
    void Shutdown();

    void Enqueue(function<void()> job);

    void WaitIdle();

private:
    void WorkerLoop();

private:
    vector<thread> _workers;
    queue<function<void()>> _jobs;

    mutex _mutex;
    condition_variable _condition;
    condition_variable _idleCondition;

    bool _stop = false;

    uint32 _activeJobCount = 0;
};