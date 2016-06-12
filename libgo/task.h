#pragma once
#include <stddef.h>
#include <functional>
#include <exception>
#include <vector>
#include <list>
#include <set>
#include "config.h"
#include "context.h"
#include "ts_queue.h"
#include "timer.h"
#include <string.h>
#include "fd_context.h"
#include "util.h"

namespace co
{

enum class TaskState
{
    init,
    runnable,
    io_block,       // write, writev, read, select, poll, ...
    sys_block,      // co_mutex, ...
    sleep,          // sleep, nanosleep, poll(NULL, 0, timeout)
    done,
    fatal,
};

std::string GetTaskStateName(TaskState state);

typedef std::function<void()> TaskF;

class BlockObject;
class Processer;

struct Task
    : public TSQueueHook, public RefObject
{
    uint64_t id_;
    TaskState state_ = TaskState::init;
    uint64_t yield_count_ = 0;
    Processer* proc_ = NULL;
    Context ctx_;
    std::string debug_info_;
    TaskF fn_;
    SourceLocation location_;
    std::exception_ptr eptr_;           // ����exception��ָ��

    // Network IO block���������
    // shared_ptr�������̰߳�ȫ��, ֻ����Э���к�SchedulerSwitch��ʹ��.
    IoSentryPtr io_sentry_;     

    BlockObject* block_ = nullptr;      // sys_block�ȴ���block����
    uint32_t block_sequence_ = 0;       // sys_block�ȴ����(��������ʱУ��)
    CoTimerPtr block_timer_;         // sys_block����ʱ�ȴ����õ�timer
    MininumTimeDurationType block_timeout_{ 0 }; // sys_block��ʱʱ��
    bool is_block_timeout_ = false;     // sys_block�ĵȴ��Ƿ�ʱ

    int sleep_ms_ = 0;                  // ˯��ʱ��

    explicit Task(TaskF const& fn, std::size_t stack_size,
            const char* file, int lineno);
    ~Task();

    void InitLocation(const char* file, int lineno);

    bool SwapIn();
    bool SwapOut();

    void SetDebugInfo(std::string const& info);
    const char* DebugInfo();

    void Task_CB();

    static uint64_t s_id;
    static std::atomic<uint64_t> s_task_count;

    static uint64_t GetTaskCount();

    static LFLock s_stat_lock;
    static std::set<Task*> s_stat_set;
    static std::map<SourceLocation, uint32_t> GetStatInfo();
    static std::vector<std::map<SourceLocation, uint32_t>> GetStateInfo();
};

} //namespace co
