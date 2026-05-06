# OneTaskCore

`OneTaskCore` 是一个用 C++17 开发的高性能、轻量级、线程安全的任务调度工作流引擎。它专为需要精确时间控制、多线程异步并发、以及复杂任务依赖编排的后端服务、游戏引擎或嵌入式系统而设计。

##  核心特性

* **多线程并发执行**：内置高效线程池，调度与执行完全解耦，防止耗时任务阻塞主调度循环。
* **DAG 任务编排**：内置有向无环图（DAG）支持，轻松实现“任务 A 与 B 成功后，自动触发任务 C”的复杂业务流。
* **精确时间调度**：基于条件变量（Condition Variable）的精确唤醒机制，实现毫秒级触发；无任务时深度休眠，实现极低 CPU 占用。
* **超时监控与预警 (Watchdog)**：自动巡检执行超时的“失控”任务，支持软中断与异常状态记录。
* **优雅停机 (Graceful Shutdown)**：在收到退出信号时，安全等待运行中的任务完成并回收线程资源，告别数据损坏。
* **优先级引擎**：支持任务优先级排序，确保高优任务在时间到达时优先分发。
* **运行遥测**：内置 `TaskMonitor` 模块，实时聚合任务执行次数与耗时（最大/最小/平均），输出直观的性能诊断报告。

## 🏗架构设计

OneTaskCore 采用了“生产者-消费者”模型，配合两阶段状态锁机制确保极致的并发安全性。

1. **Scheduler (调度器)**：引擎核心大脑。负责依赖拦截、时间轮询、垃圾回收与工作分发。
2. **ThreadPool (线程池)**：负责管理工人线程池，异步执行被领取的任务。
3. **Task (任务基类)**：对外统一接口，开发者通过继承此类并重写 `Execute()` 来定义具体业务逻辑。
4. **TaskMonitor (遥测系统)**：线程安全的打点系统，负责收集各任务执行的性能指标。

##  快速上手

### 1. 定义你的业务任务

```cpp
#include "OneTaskCore.h"
#include <iostream>

using namespace OneTaskCore;

class MyDataTask : public Task {
public:
    MyDataTask(uint32_t id, uint8_t priority = 0) : Task(id, priority) {}

    bool Init() override { return true; }

    TaskResult Execute() override {
        // 配合 Watchdog 检查软取消信号
        if (IsCancelled()) return TaskResult::FAILED;
        
        std::cout << "[Worker] Task " << GetId() << " is running..." << std::endl;
        return TaskResult::SUCCESS;
    }
};
```

### 2. 编排并运行调度器

```cpp
#include "OneTaskCore.h"
#include <thread>

int main() {
    OneTaskCore::Scheduler scheduler;

    auto taskA = std::make_shared<MyDataTask>(1, 10);
    auto taskB = std::make_shared<MyDataTask>(2, 5);

    // 设置 DAG 依赖：taskB 必须等待 taskA 完成
    taskB->AddDependency(1);

    scheduler.AddTask(taskA);
    scheduler.AddTask(taskB);

    // 启动后台调度循环
    std::thread scheduler_thread([&scheduler]() {
        scheduler.Run();
    });

    // ... 运行一段时间后测试优雅停机
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    scheduler.Stop();
    if (scheduler_thread.joinable()) {
        scheduler_thread.join();
    }

    // 打印遥测报告
    scheduler.GetMonitor().PrintAll();

    return 0;
}
```

## 开发计划 (Roadmap)

- [x] 线程池异步并发支持
- [x] 基于条件变量的精确唤醒
- [x] 两阶段状态锁定防重复调度
- [x] **任务依赖管理 (DAG)**：支持 A 任务完成后自动触发 B 任务。
- [x] **任务超时监控 (Watchdog)**：自动检测并处理卡死的任务。
- [x] **优雅停机 (Graceful Shutdown)**：安全回收线程资源。
- [ ] **动态优先级调整**：解决优先级反转 (Priority Inversion) 问题。
- [ ] **C++20 协程支持**：实现无阻塞的极致 I/O 调度。

## 许可证

本项目采用 MIT 许可证。
