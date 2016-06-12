#pragma once
#include <unordered_map>
#include <list>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <deque>
#include <cstdlib>
#include "config.h"
#include "context.h"
#include "task.h"
#include "block_object.h"
#include "co_mutex.h"
#include "timer.h"
#include "io_wait.h"
#include "sleep_wait.h"
#include "processer.h"
#include "debugger.h"

namespace co {

    // Э�����׳�δ�����쳣ʱ�Ĵ���ʽ
    enum class eCoExHandle : uint8_t
    {
        immedaitely_throw,  // �����׳�
        delay_rethrow,      // �ӳٵ�����������ʱ�׳�
        debugger_only,      // ����ӡ������Ϣ
    };

    ///---- ����ѡ��
    struct CoroutineOptions
    {
        /*********************** Debug options **********************/
        // ����ѡ��, ����: dbg_switch �� dbg_hook|dbg_task|dbg_wait
        uint64_t debug = 0;

        // ������Ϣ���λ�ã���д�������������ض������λ��
        FILE* debug_output = stdout;
        /************************************************************/

        /**************** Stack and Exception options ***************/
        // Э�����׳�δ�����쳣ʱ�Ĵ���ʽ
        eCoExHandle exception_handle = eCoExHandle::immedaitely_throw;

        // Э��ջ��С����, ֻ��Ӱ���ڴ�ֵ����֮���´�����P, �������״�Runǰ����.
        // stack_size�������ò�����1MB
        // Linuxϵͳ��, ����2MB��stack_size�ᵼ���ύ�ڴ��ʹ������1MB��stack_size��10��.
        uint32_t stack_size = 1 * 1024 * 1024;

        // û��Э����Ҫ����ʱ, Run������ߵĺ�����(������ʵʱ��ϵͳ���Կ��ǵ������ֵ)
        uint8_t max_sleep_ms = 20;

        // ÿ����ʱ��ÿ֡�������������(Ϊ0��ʾ����, ÿ֡����ǰ���п��Դ��������)
        uint32_t timer_handle_every_cycle = 0;

        // epollÿ�δ�����event����(Windows����Ч)
        uint32_t epoll_event_size = 10240;

        // �Ƿ�����worksteal�㷨
        bool enable_work_steal = true;

        // �Ƿ�����Э��ͳ�ƹ���(����һ���������, Ĭ�ϲ�����)
        bool enable_coro_stat = false;

        // ջ�����ñ����ڴ�ε��ڴ�ҳ����(��linux����Ч)(Ĭ��Ϊ0, ��:������)
        // ��ջ���ڴ������ǰ��ҳ����Ϊprotect����.
        // ���Կ�����ѡ��ʱ, stack_size��������protect_stack_page+1ҳ
        uint32_t & protect_stack_page = StackAllocator::get_protect_stack_page();

        // ����ջ�ڴ����(malloc/free)
        // ʹ��fiber��Э�̵ײ�ʱ��Ч
        stack_malloc_fn_t & stack_malloc_fn = StackAllocator::get_malloc_fn();
        stack_free_fn_t & stack_free_fn = StackAllocator::get_free_fn();
    };
    ///-------------------

    struct ThreadLocalInfo
    {
        int thread_id = -1;     // Run thread index, increment from 1.
        uint8_t sleep_ms = 0;
        Processer *proc = nullptr;
    };

    class ThreadPool;

    class Scheduler
    {
    public:
        // Runʱִ�е�����
        enum eRunFlags
        {
            erf_do_coroutines = 0x1,
            erf_do_timer = 0x2,
            erf_do_sleeper = 0x4,
            erf_do_eventloop = 0x8,
            erf_idle_cpu = 0x10,
            erf_all = 0x7fffffff,
        };

        typedef std::deque<Processer*> ProcList;
        typedef std::pair<uint32_t, TSQueue<Task, false>> WaitPair;
        typedef std::unordered_map<uint64_t, WaitPair> WaitZone;
        typedef std::unordered_map<int64_t, WaitZone> WaitTable;

        static Scheduler& getInstance();

        // ��ȡ����ѡ��
        CoroutineOptions& GetOptions();

        // ����һ��Э��
        void CreateTask(TaskF const& fn, std::size_t stack_size,
            const char* file, int lineno, int dispatch);

        // ��ǰ�Ƿ���Э����
        bool IsCoroutine();

        // �Ƿ�û��Э�̿�ִ��
        bool IsEmpty();

        // ��ǰЭ���ó�ִ��Ȩ
        void CoYield();

        // ���������Ⱥ���, �ڲ�ִ��Э�̡�����Э��
        uint32_t Run(int flags = erf_all);

        // ѭ��Runֱ��û��Э��Ϊֹ
        // @loop_task_count: �������ĳ�פЭ��.
        //    ���磺loop_task_count == 2ʱ, ��ʣ���2��Э�̵�ʱ����������ͻ�return.
        // @remarks: ����ӿڻ�����ִ��һ��Run.
        void RunUntilNoTask(uint32_t loop_task_count = 0);

        // ����ѭ��ִ��Run
        void RunLoop();

        // ��ǰЭ��������
        uint32_t TaskCount();

        // ��ǰЭ��ID, ID��1��ʼ������Э�����򷵻�0��
        uint64_t GetCurrentTaskID();

        // ��ǰЭ���л��Ĵ���
        uint64_t GetCurrentTaskYieldCount();

        // ���õ�ǰЭ�̵�����Ϣ, ��ӡ������Ϣʱ������
        void SetCurrentTaskDebugInfo(std::string const& info);

        // ��ȡ��ǰЭ�̵ĵ�����Ϣ, ���ص����ݰ����û��Զ������Ϣ��Э��ID
        const char* GetCurrentTaskDebugInfo();

        // ��ȡ��ǰ�߳�ID.(��ִ�е��������ȵ�˳���)
        uint32_t GetCurrentThreadID();

        // ��ȡ��ǰ����ID.
        uint32_t GetCurrentProcessID();

    public:
        /// sleep switch
        //  \timeout_ms min value is 0.
        void SleepSwitch(int timeout_ms);

        /// ------------------------------------------------------------------------
        // @{ ��ʱ��
        template <typename DurationOrTimepoint>
        TimerId ExpireAt(DurationOrTimepoint const& dur_or_tp, CoTimer::fn_t const& fn)
        {
            TimerId id = timer_mgr_.ExpireAt(dur_or_tp, fn);
            DebugPrint(dbg_timer, "add timer id=%llu", (long long unsigned)id->GetId());
            return id;
        }

        bool CancelTimer(TimerId timer_id);
        bool BlockCancelTimer(TimerId timer_id);
        // }@
        /// ------------------------------------------------------------------------

        /// ------------------------------------------------------------------------
        // @{ �̳߳�
        ThreadPool& GetThreadPool();
        // }@
        /// ------------------------------------------------------------------------

        // iowait����
        IoWait& GetIoWait() { return io_wait_; }

    public:
        Task* GetCurrentTask();

    private:
        Scheduler();
        ~Scheduler();

        Scheduler(Scheduler const&) = delete;
        Scheduler(Scheduler &&) = delete;
        Scheduler& operator=(Scheduler const&) = delete;
        Scheduler& operator=(Scheduler &&) = delete;

        // ��һ��Э�̼����ִ�ж�����
        void AddTaskRunnable(Task* tk, int dispatch = egod_default);

        // Run������һ����, ����runnable״̬��Э��
        uint32_t DoRunnable(bool allow_steal = true);

        // Run������һ����, ����epoll���
        int DoEpoll(int wait_milliseconds);

        // Run������һ����, ����sleep���
        // @next_ms: ������һ��timer�����ĺ�����
        uint32_t DoSleep(long long &next_ms);

        // Run������һ����, ����ʱ��
        // @next_ms: ������һ��timer�����ĺ�����
        uint32_t DoTimer(long long &next_ms);

        // ��ȡ�ֲ߳̾���Ϣ
        ThreadLocalInfo& GetLocalInfo();

        Processer* GetProcesser(std::size_t index);

        // List of Processer
        LFLock proc_init_lock_;
        ProcList run_proc_list_;
        std::atomic<uint32_t> dispatch_robin_index_{ 0 };

        // io block waiter.
        IoWait io_wait_;

        // sleep block waiter.
        SleepWait sleep_wait_;

        // Timer manager.
        CoTimerMgr timer_mgr_;

        ThreadPool *thread_pool_;

        std::atomic<uint32_t> task_count_{ 0 };
        std::atomic<uint32_t> thread_id_{ 0 };

    private:
        friend class CoMutex;
        friend class BlockObject;
        friend class IoWait;
        friend class SleepWait;
        friend class Processer;
        friend class FileDescriptorCtx;
        friend class CoDebugger;
    };

} //namespace co

#define g_Scheduler ::co::Scheduler::getInstance()

