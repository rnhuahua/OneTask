// /include/OneTask/OneTask.h
#pragma once

// ============================================================================
// OneTaskCore - 轻量级 C++ 任务调度和执行核心
// ============================================================================
// 说明：这是对外暴露的唯一公共头文件。
// 外部工程只需 #include "OneTask.h" 即可使用全部调度与监控功能。

// 1. 核心任务接口与状态定义
#include "task/task.h"

// 2. 性能监控与统计模块
#include "monitor/monitor.h"

// 3. 核心调度器
#include "scheduler/scheduler.h"

// 4. 线程池实现
#include "threadpool/thread_pool.h"
