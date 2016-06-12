#pragma once
#include "task.h"
#include "ts_queue.h"

namespace co {

struct ThreadLocalInfo;

// coroutine executor
// manager a batch of coroutine share stack and schedule, not thread safe
class Processer
    : public TSQueueHook
{
private:
    typedef TSQueue<Task> TaskList;

    Task* current_task_ = nullptr;
    TaskList runnable_list_;
    uint32_t id_;
    static std::atomic<uint32_t> s_id_;

public:
    explicit Processer();

    void AddTaskRunnable(Task *tk);

    uint32_t Run(uint32_t &done_count);

    void CoYield();

    uint32_t GetTaskCount();

    Task* GetCurrentTask();

    std::size_t StealHalf(Processer & other);
};

} //namespace co
