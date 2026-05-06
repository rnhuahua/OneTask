// /src/scheduler/scheduler.cpp
#include "OneTask/scheduler/scheduler.h"

#include <algorithm>
#include <iostream>
#include <thread>

namespace OneTaskCore
{

void Scheduler::AddTask(std::shared_ptr<Task> task)
{
    if (!task || !task->Init())
        return;

    {
        std::lock_guard<std::mutex> lock(mtx);
        task->SetState(TaskState::READY);
        m_tasks.push_back(task);
    }
    m_cv.notify_one();
}

// ========================
// 僵尸任务清理 (内存回收)
// ========================
void Scheduler::CleanupTasks()
{
    std::lock_guard<std::mutex> lock(mtx);
    m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(),
                                 [](const std::shared_ptr<Task>& t)
                                 {
                                     TaskState state = t->GetState();
                                     return (state == TaskState::FINISHED || state == TaskState::ERROR) &&
                                            !t->IsRunning();
                                 }),
                  m_tasks.end());
}

// ========================
// 核心调度循环
// ========================

void Scheduler::Run()
{
    while (true)
    {
        // 1. 清理已结束任务
        CleanupTasks();
        CheckTimeouts();
        std::shared_ptr<Task> task = nullptr;
        {
            std::unique_lock<std::mutex> lock(mtx);

            if (m_stop && m_tasks.empty())
            {
                break;
            }

            if (m_tasks.empty())
            {
                m_cv.wait(lock);
                continue;
            }

            // 3. 尝试挑选任务 (内部不带锁)
            task = PickNextTaskInternal();

            if (!task)
            {
                // 如果设置了停止，且当前没有可执行的任务（比如都在等待时间），
                // 且没有正在 RUNNING 的任务，在这里退出
                if (m_stop)
                {
                    // 检查是否还有任务在运行，如果没有了，就可以结束调度
                    bool any_running = false;
                    for (const auto& t : m_tasks)
                    {
                        if (t->GetState() == TaskState::RUNNING)
                        {
                            any_running = true;
                            break;
                        }
                    }
                    if (!any_running)
                        break;
                }
                // 4. 没任务可做（还没到时间），计算最近的唤醒点
                uint64_t now = NowMs();
                uint64_t next_run = UINT64_MAX;
                for (const auto& t : m_tasks)
                {
                    if (t->GetState() == TaskState::READY)
                        next_run = std::min(next_run, t->GetNextRunTime());
                }

                if (next_run != UINT64_MAX && next_run > now)
                {
                    m_cv.wait_for(lock, std::chrono::milliseconds(next_run - now));
                }
                else
                {
                    m_cv.wait_for(lock, std::chrono::milliseconds(10));  // 兜底防止空转
                }
                continue;
            }
        }  // 自动释放锁，允许工作线程或其他线程操作 m_tasks

        // 5. 异步执行：任务逻辑完全封装在 Lambda 中
        m_pool.Enqueue(
            [this, task]()
            {
                task->MarkStartTime();
                uint64_t start_us = NowMs() * 1000;  // 建议监控用微秒更精确

                TaskResult result = TaskResult::FAILED;
                try
                {
                    result = task->Execute();
                }
                catch (...)
                {
                    task->SetState(TaskState::ERROR);
                }

                m_monitor.Record(task->GetId(), (NowMs() * 1000) - start_us);

                // 状态流转
                if (task->IsCancelled())
                {
                    task->SetState(TaskState::FINISHED);
                }
                else
                {
                    switch (result)
                    {
                        case TaskResult::SUCCESS:
                        {
                            {
                                std::lock_guard<std::mutex> inner_lock(mtx);
                                m_finished_tasks.insert(task->GetId());
                            }
                            if (task->IsPeriodic())
                            {
                                task->SetNextRunTime(NowMs() + task->GetPeriod());
                                task->SetState(TaskState::READY);
                            }
                            else
                            {
                                task->SetState(TaskState::FINISHED);
                            }
                            break;
                        }
                        case TaskResult::RETRY:
                            task->SetState(TaskState::READY);
                            break;
                        case TaskResult::FAILED:
                            task->SetState(TaskState::ERROR);
                            break;
                    }
                }
                // 任务完成后，通知调度器重新评估（可能周期任务又 READY 了）
                m_cv.notify_one();
            });
    }
}

void Scheduler::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        m_stop = true;
    }
    m_cv.notify_all();  // 唤醒正在 wait 的调度主循环
}
// 内部函数：不需要自己加锁，由调用者保证安全
std::shared_ptr<Task> Scheduler::PickNextTaskInternal()
{
    uint64_t now = NowMs();
    std::shared_ptr<Task> best = nullptr;

    for (auto& task : m_tasks)
    {
        // 1. 基础检查：状态必须是 READY，且时间已到
        if (task->GetState() != TaskState::READY || task->GetNextRunTime() > now)
            continue;

        // ==========================================
        // 2.  DAG 依赖拦截检查
        // ==========================================
        bool deps_met = true;
        for (uint32_t dep_id : task->GetDependencies())
        {
            // 如果有一个依赖项不在“历史功劳簿”中，说明前置任务还没做完或失败了
            if (m_finished_tasks.find(dep_id) == m_finished_tasks.end())
            {
                deps_met = false;
                break;
            }
        }
        // 依赖满足，跳过该任务（它会继续留在队列中等待）
        if (!deps_met)
            continue;
        // ==========================================

        // 3. 优先级与时间比较
        if (!best || task->GetPriority() > best->GetPriority() ||
            (task->GetPriority() == best->GetPriority() && task->GetNextRunTime() < best->GetNextRunTime()))
        {
            best = task;
        }
    }

    if (best)
        best->SetState(TaskState::RUNNING);
    return best;
}

// ========================
// 选择最高优先级任务
// ========================
std::shared_ptr<Task> Scheduler::PickNextTask()
{
    std::lock_guard<std::mutex> lock(mtx);  // 锁定任务列表，防止并发修改
    uint64_t now = NowMs();
    std::shared_ptr<Task> best = nullptr;

    for (auto& task : m_tasks)
    {
        // 只有 READY 状态且时间已到的任务才能被选中
        if (task->GetState() != TaskState::READY)
            continue;
        if (task->GetNextRunTime() > now)
            continue;

        if (!best || task->GetPriority() > best->GetPriority() ||
            (task->GetPriority() == best->GetPriority() && task->GetNextRunTime() < best->GetNextRunTime()))
        {
            best = task;
        }
    }

    if (best)
    {
        // 在返回前立即切换状态，确保下一轮循环不再选它
        best->SetState(TaskState::RUNNING);
    }
    return best;
}

void Scheduler::CheckTimeouts()
{
    std::lock_guard<std::mutex> lock(mtx);
    uint64_t now = NowMs();

    for (auto& task : m_tasks)
    {
        if (task->GetState() == TaskState::RUNNING)
        {
            uint32_t timeout = task->GetTimeout();
            if (timeout > 0)
            {
                uint64_t duration = now - task->GetStartTime();
                if (duration > timeout)
                {
                    // 发现超时！
                    HandleTaskTimeout(task);
                }
            }
        }
    }
}

void Scheduler::HandleTaskTimeout(std::shared_ptr<Task> task)
{
    // 1. 修改状态为 ERROR，防止任务完成后又被改为 READY
    task->SetState(TaskState::ERROR);

    // 2. 触发取消标记（软退出提示）
    task->Cancel();

    // 3. 记录到遥测系统
    // m_monitor.RecordError(task->GetId(), "TIMEOUT");

    std::cerr << "[Watchdog] Task " << task->GetId() << " timed out after " << task->GetTimeout() << "ms" << std::endl;
}
}  // namespace OneTaskCore