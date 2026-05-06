// /src/monitor/monitor.cpp
#include "OneTask/monitor/monitor.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

namespace OneTaskCore
{

// ========================
// 记录任务单次执行的耗时
// ========================
void TaskMonitor::Record(uint32_t task_id, uint64_t exec_time)
{
    std::lock_guard<std::mutex> lock(mtx);

    // 利用 unordered_map 的 operator[] 特性：
    // 如果 task_id 不存在，会自动创建一个默认初始化的 TaskStats 结构体
    auto& stats = m_stats[task_id];

    stats.exec_count++;             // 累加执行次数
    stats.total_time += exec_time;  // 累加总耗时

    // 更新最大耗时
    if (exec_time > stats.max_time)
    {
        stats.max_time = exec_time;
    }

    // 更新最小耗时
    if (exec_time < stats.min_time)
    {
        stats.min_time = exec_time;
    }
}

// ========================
// 获取特定任务的统计数据
// ========================
const TaskStats* TaskMonitor::GetStats(uint32_t task_id) const
{
    auto it = m_stats.find(task_id);
    if (it != m_stats.end())
    {
        return &(it->second);
    }
    return nullptr;  // 任务尚未执行过或不存在
}

// ========================
// 打印所有任务的性能报告
// ========================
void TaskMonitor::PrintAll() const
{
    std::cout << "\n=========================================================\n";
    std::cout << "                  OneTask Monitor Report                 \n";
    std::cout << "=========================================================\n";
    std::cout << std::left << std::setw(10) << "Task ID" << std::setw(10) << "Execs" << std::setw(12) << "Total(us)"
              << std::setw(10) << "Max(us)" << std::setw(10) << "Min(us)"
              << "Avg(us)\n";
    std::cout << "---------------------------------------------------------\n";

    if (m_stats.empty())
    {
        std::cout << "No tasks recorded yet.\n";
    }
    else
    {
        for (const auto& [id, stats] : m_stats)
        {
            // 处理由于尚未记录数据导致的 min_time 保持 UINT64_MAX 的显示问题
            uint64_t display_min = (stats.min_time == UINT64_MAX) ? 0 : stats.min_time;

            std::cout << std::left << std::setw(10) << id << std::setw(10) << stats.exec_count << std::setw(12)
                      << stats.total_time << std::setw(10) << stats.max_time << std::setw(10) << display_min
                      << std::fixed << std::setprecision(2) << stats.AvgTime() << "\n";
        }
    }
    std::cout << "=========================================================\n\n";
}

}  // namespace OneTaskCore