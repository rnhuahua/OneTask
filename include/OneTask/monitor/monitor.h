// /include/OneTask/monitor/monitor.h
#pragma once
#include <cstdint>
#include <unordered_map>
#include <mutex>

namespace OneTaskCore
{

struct TaskStats
{
    uint64_t exec_count = 0;
    uint64_t total_time = 0;  // microseconds
    uint64_t max_time = 0;
    uint64_t min_time = UINT64_MAX;

    double AvgTime() const
    {
        return exec_count == 0 ? 0.0 : (double)total_time / exec_count;
    }
};

class TaskMonitor
{
   public:
    void Record(uint32_t task_id, uint64_t exec_time);

    const TaskStats* GetStats(uint32_t task_id) const;

    void PrintAll() const;

   private:
    std::unordered_map<uint32_t, TaskStats> m_stats;

    std::mutex mtx;
};

}  // namespace OneTaskCore