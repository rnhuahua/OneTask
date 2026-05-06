#include "OneTask/threadpool/thread_pool.h"
ThreadPool::ThreadPool(size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        workers.emplace_back(
            [this]()
            {
                while (true)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this]() { return stop || !tasks.empty(); });

                        if (stop && tasks.empty())
                            return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    task();  // 执行任务
                }
            });
    }
}

void ThreadPool::Enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}
ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop = true;
    }
    cv.notify_all();  // 唤醒所有正在等待任务的线程
    for (std::thread& worker : workers)
    {
        if (worker.joinable())
            worker.join();  // 等待所有线程安全退出
    }
}