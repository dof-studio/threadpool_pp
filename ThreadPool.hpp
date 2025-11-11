
// ThreadPool.hpp
//
// A Simplified ThreadPool Utility, Header Only

#include <thread>
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <functional>
#include <condition_variable>

#pragma once

class ThreadPool {
private:
    std::vector<std::thread> _threads;
    std::function<void()> _working_task;
    std::queue<std::function<void()>> _tasks;
    std::mutex _mutex;
    std::condition_variable _condition;
    std::condition_variable _condition_finished;
    size_t _num_threads = 0;
    size_t _num_started = 0;
    size_t _num_killed = 0;
    size_t _num_finished = 0;
    size_t _num_detached = 0;
    bool _stop = false;

public:
    // Constructor
    ThreadPool(size_t num_threads) noexcept : _num_threads(num_threads) {
        // Set working task model
        _working_task = [this]()
            {
                // For ever
                for (;;)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _condition.wait(lock, [this] { return !_tasks.empty() || _stop; });
                        if (_stop && _tasks.empty())
                        {
                            return;
                        }
                        task = std::move(_tasks.front());
                        _tasks.pop(); 
                        _condition.notify_one();
                    }
                    task();
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _num_finished++;
                        _condition_finished.notify_all();
                    }
                }
            };

        
        // Start tasks
        for (size_t i = 0; i < _num_threads; ++i)
        {
            _threads.emplace_back(_working_task);
        }
    }

    // Deconstructor
    ~ThreadPool() noexcept {
        Destroy();
    }


public:
    // Invoke a function (automatically assigned to one internal thread)
    template <typename F, typename... Args>
    void Invoke(F&& function, Args &&... args) noexcept {
        if (!_stop)
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _tasks.emplace([fn = std::forward<F>(function), ... args = std::forward<Args>(args)]() mutable { fn(args...); });
            }
            _condition.notify_one();
            _num_started++;
        }
    }

public:
    // Is there any task pending in queue
    bool IsNoPending() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return _tasks.empty();
    }

    // Get the number of the pending tasks
    size_t PendingTaskNum() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return _tasks.size();
    }

    // Kill all tasks that are currently pended
    void KillAllPending() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_tasks.empty())
        {
            _tasks.pop();
            _num_killed++;
        }
    }

    // Wait until a certain number of tasks have finished （Alias)
    bool WaitTill(size_t num_tasks, bool _ignoreStopSignal = false) noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_ignoreStopSignal)
        {
            _condition_finished.wait(lock, [this, num_tasks]
                { return (_num_finished + _num_killed) >= num_tasks || _stop; });
        }
        else
        {
            _condition_finished.wait(lock, [this, num_tasks]
                { return (_num_finished + _num_killed) >= num_tasks; });
        }
        return _num_finished >= num_tasks;
    }

    // Wait till all invoked tasks have finished, including detached
    bool WaitTillAll(bool _ignoreStopSignal = false) noexcept{
        return WaitTill(_num_started + _num_detached, _ignoreStopSignal);
    }

    // Wait till all invoked tasks have started
    void WaitTillNoPending() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this]
        {
            return _tasks.empty();
        });
        return;
    }

    // Detach all threads and tasks
    void Detach() noexcept {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _stop = true;
        }
        _condition.notify_all();
        for (auto& thread : _threads)
        {
            thread.detach();
            _num_detached++;
        }
        return;
    }

    // Stop the thread pool and all tasks, and _stop is reset to true (including killing threads)
    // While those started tasks cannot be stoped
    void Stop() noexcept {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _stop = true;
            while (!_tasks.empty())
            {
                _tasks.pop();
                _num_killed++;
            }
        }
        _condition.notify_all();
    }

    // Stop the thread pool and all tasks, and _stop is reset to true (including killing threads)
    // Completely stop the threadpool and reset num of threads to zero
    void StopForced() noexcept {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _stop = true;
            while (!_tasks.empty())
            {
                _tasks.pop();
                _num_killed++;
            }
        }
        _condition.notify_all();

        ResetThreadNum(0);
    }

    // Reset thread reserved size (unfinished threads will be coerced to stop)
    void ResetThreadNum(size_t num_threads) noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        if (num_threads < _num_threads){
            _stop = true;
            while (!_tasks.empty())
            {
                _tasks.pop();
                _num_killed++;
            }
            _condition.notify_all();
            lock.unlock();
            for (size_t i = num_threads; i < _num_threads; ++i)
            {
                if (_threads[i].joinable())
                {
                    _threads[i].join();
                }
            }
            lock.lock();
            _threads.resize(num_threads);
            _num_threads = num_threads;
            _stop = false;
        }
        else if (num_threads > _num_threads){
            _num_threads = num_threads;
            for (size_t i = _threads.size(); i < num_threads; ++i)
            {
                _threads.emplace_back(_working_task);
            }
        }
    }

    // Valid or not (Only being valid can a pool invoke functions)
    bool Valid() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return !_stop;
    }

    // Get thread number of the thread pool
    size_t ThreadNum() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return _num_threads;
    }

    // Get invoked, or say, started task number
    size_t StartedNum() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return _num_started;
    }

    // Get finished task number
    size_t FinishedNum() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return _num_finished;
    }

    // Get killed task number
    size_t KilledNum() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return _num_killed;
    }

    // Get detached task number
    size_t DetachedNum() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        return _num_detached;
    }

private:
    // Destory all threads and stop the thread pool (DO NOT MANNUALLY CALL)
    void Destroy() noexcept {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _stop = true;
        }
        _condition.notify_all();
        for (auto& thread : _threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
            _num_killed++;
        }
    }
};
