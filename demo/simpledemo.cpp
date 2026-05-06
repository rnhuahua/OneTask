#include <iostream>
#include <thread>

#include "OneTask/OneTask.h"

using namespace OneTaskCore;

// --- 1. 正常周期性任务 (心跳) ---
class HeartbeatTask : public Task
{
   public:
    HeartbeatTask(uint32_t id) : Task(id, 10)
    {                    // 优先级 10
        SetPeriod(500);  // 500ms 周期 [cite: 11, 12]
    }
    bool Init() override
    {
        return true;
    }
    TaskResult Execute() override
    {
        static int count = 0;
        std::cout << "[Heartbeat] Task " << m_id << " tick " << ++count << std::endl;
        if (count >= 5)
        {
            Cancel();  // 执行 5 次后取消 [cite: 5, 83]
        }
        return TaskResult::SUCCESS;
    }
};

// --- 2. 故意超时的任务 (测试 Watchdog) ---
class TimeoutTask : public Task
{
   public:
    TimeoutTask(uint32_t id) : Task(id, 5)
    {                     // 优先级 5
        SetTimeout(100);  // 设置 100ms 超时限制 [cite: 97, 177]
    }
    bool Init() override
    {
        return true;
    }
    TaskResult Execute() override
    {
        std::cout << "[TimeoutTest] Task " << m_id << " starting slow work..." << std::endl;

        // 模拟耗时操作，故意超过 100ms
        for (int i = 0; i < 20; ++i)
        {
            if (IsCancelled())
            {  // 检查 Watchdog 是否发出了取消信号 [cite: 167, 177]
                std::cout << "[TimeoutTest] Task " << m_id << " detected watchdog cancel!" << std::endl;
                return TaskResult::FAILED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return TaskResult::SUCCESS;
    }
};

int main()
{
    Scheduler scheduler;

    // 添加任务
    scheduler.AddTask(std::make_shared<HeartbeatTask>(101));
    scheduler.AddTask(std::make_shared<TimeoutTask>(202));

    // 启动调度器线程
    std::thread scheduler_thread(
        [&scheduler]()
        {
            scheduler.Run();  // [cite: 17, 130]
        });

    // 让系统跑 3 秒
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 测试优雅停机
    std::cout << "\n[Main] Triggering Graceful Shutdown..." << std::endl;
    scheduler.Stop();  // [cite: 181]

    if (scheduler_thread.joinable())
    {
        scheduler_thread.join();  // 等待调度器安全退出
    }

    // 打印最终性能报告
    scheduler.GetMonitor().PrintAll();  // [cite: 21, 59]

    return 0;
}