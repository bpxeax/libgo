#pragma once
#include <deque>
#include <mutex>
#include <map>
#include <vector>
#include "spinlock.h"
#include "util.h"

#if defined(__GNUC__)
#include <cxxabi.h>
#include <stdlib.h>
#endif 

namespace co
{

struct ThreadLocalInfo;

// libgo debug tool
class CoDebugger
{
public:
    typedef std::atomic<uint64_t> count_t;
    typedef std::deque<std::pair<std::string, count_t>> object_counts_t;
    typedef std::deque<std::pair<std::string, uint64_t>> object_counts_result_t;

    template <typename Drived>
    struct DebuggerBase
    {
#if ENABLE_DEBUGGER
        DebuggerBase()
        {
            object_creator_.do_nothing();
            ++Count();
        }
        DebuggerBase(const DebuggerBase&)
        {
            object_creator_.do_nothing();
            ++Count();
        }
        ~DebuggerBase()
        {
            --Count();
        }

        static count_t& Count();

        struct object_creator
        {
            object_creator()
            {
                DebuggerBase<Drived>::Count();
            }
            inline void do_nothing() {}
        };

        static object_creator object_creator_;
#endif
    };

public:
    static CoDebugger& getInstance();

    std::string GetAllInfo();

    uint32_t TaskCount();

	// id start from 1, no coroutine return 0
    uint64_t GetCurrentTaskID();

    uint64_t GetCurrentTaskYieldCount();

    void SetCurrentTaskDebugInfo(std::string const& info);

    const char* GetCurrentTaskDebugInfo();

    uint32_t GetCurrentThreadID();

    uint32_t GetCurrentProcessID();

    uint64_t GetTimerCount();

    uint64_t GetSleepTimerCount();

    std::map<SourceLocation, uint32_t> GetTasksInfo();
    std::vector<std::map<SourceLocation, uint32_t>> GetTasksStateInfo();

#if __linux__
    /// ------------ Linux -------------
    std::string GetFdInfo();

    uint32_t GetEpollWaitCount();
#endif

    object_counts_result_t GetDebuggerObjectCounts();

    ThreadLocalInfo& GetLocalInfo();

private:
    CoDebugger() = default;
    ~CoDebugger() = default;
    CoDebugger(CoDebugger const&) = delete;
    CoDebugger& operator=(CoDebugger const&) = delete;

    template <typename Drived>
    std::size_t GetDebuggerDrivedIndex()
    {
        static std::size_t s_index = s_debugger_drived_type_index_++;
        return s_index;
    }

private:
    LFLock object_counts_spinlock_;
    object_counts_t object_counts_;
    std::atomic<std::size_t> s_debugger_drived_type_index_{0};
};

template <typename T>
struct real_typename_helper {};

template <typename T>
std::string real_typename()
{
#if defined(__GNUC__)
    /// gcc.
    int s;
    char * realname = abi::__cxa_demangle(typeid(real_typename_helper<T>).name(), 0, 0, &s);
    std::string result(realname);
    free(realname);
#else
    std::string result(typeid(real_typename_helper<T>).name());
#endif 
    std::size_t start = result.find_first_of('<') + 1;
    std::size_t end = result.find_last_of('>');
    return result.substr(start, end - start);
}

#if ENABLE_DEBUGGER
template <typename Drived>
inline CoDebugger::count_t& CoDebugger::DebuggerBase<Drived>::Count()
{
    std::size_t index = CoDebugger::getInstance().GetDebuggerDrivedIndex<Drived>();
    auto &objs = CoDebugger::getInstance().object_counts_;
    if (objs.size() > index)
        return objs[index].second;

    std::unique_lock<LFLock> lock(CoDebugger::getInstance().object_counts_spinlock_);
    if (objs.size() > index)
        return objs[index].second;

    objs.resize(index + 1);
    objs[index].first = real_typename<Drived>();
    return objs[index].second;
}

template <typename Drived>
typename CoDebugger::DebuggerBase<Drived>::object_creator
    CoDebugger::DebuggerBase<Drived>::object_creator_;

#endif

} //namespace co
