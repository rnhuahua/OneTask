// /include/OneTask/scheduler/scheduler.h
#pragma once
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "OneTask/monitor/monitor.h"
#include "OneTask/task/task.h"
#include "OneTask/threadpool/thread_pool.h"
namespace OneTaskCore
{

class Scheduler
{
   public:
    void AddTask(std::shared_ptr<Task> task);

    void Run();  // 主循环
    void Stop();
    const TaskMonitor& GetMonitor() const
    {
        return m_monitor;
    }
    void CheckTimeouts();
    void HandleTaskTimeout(std::shared_ptr<Task> task);
    void CleanupTasks();
   private:
    std::vector<std::shared_ptr<Task>> m_tasks;
    TaskMonitor m_monitor;
    ThreadPool m_pool;
    std::mutex mtx;
    std::condition_variable m_cv;
    bool m_stop = false;  // 用于停止调度器

    std::unordered_set<uint32_t> m_finished_tasks;
    std::shared_ptr<Task> PickNextTask();
    std::shared_ptr<Task> PickNextTaskInternal();
    

};

}  // namespace OneTaskCore