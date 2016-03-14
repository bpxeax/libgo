#pragma once
#include <memory>
#include <queue>
#include "block_object.h"
#include "co_mutex.h"
#include "scheduler.h"

namespace co
{

template <typename T>
class Channel
{
private:
    class ChannelImpl;
    mutable std::shared_ptr<ChannelImpl> impl_;

public:
    explicit Channel(std::size_t capacity = 0)
    {
        impl_.reset(new ChannelImpl(capacity));
    }
    ~Channel()
    {
    }

    template <typename U>
    Channel const& operator<<(U && t) const
    {
        (*impl_) << std::forward<U>(t);
        return *this;
    }

    template <typename U>
    Channel const& operator>>(U & t) const
    {
        (*impl_) >> t;
        return *this;
    }

    Channel const& operator>>(nullptr_t ignore) const
    {
        (*impl_) >> ignore;
        return *this;
    }

    template <typename U>
    bool TryPush(U && t) const
    {
        return impl_->TryPush(std::forward<U>(t));
    }

    template <typename U>
    bool TryPop(U & t) const
    {
        return impl_->TryPop(t);
    }

    bool TryPop(nullptr_t ignore) const
    {
        return impl_->TryPop(ignore);
    }

    template <typename U, typename Duration>
    bool TimedPush(U && t, Duration const& timeout) const
    {
        return impl_->TimedPush(std::forward<U>(t), timeout);
    }

    template <typename U, typename Duration>
    bool TimedPop(U & t, Duration const& timeout) const
    {
        return impl_->TimedPop(t, timeout);
    }

    template <typename Duration>
    bool TimedPop(nullptr_t ignore, Duration const& timeout) const
    {
        return impl_->TimedPop(ignore, timeout);
    }

    bool Unique() const
    {
        return impl_.unique();
    }

    bool empty() const
    {
        return impl_->empty();
    }

private:
    class ChannelImpl
    {
        BlockObject write_block_;
        BlockObject read_block_;
        std::queue<T> queue_;
        CoMutex queue_lock_;

    public:
        explicit ChannelImpl(std::size_t capacity)
            : write_block_(capacity)
        {}

        bool empty()
        {
            return !read_block_.IsWakeup();
        }

        // write
        template <typename U>
        void operator<<(U && t)
        {
            write_block_.CoBlockWait();

            {
                std::unique_lock<CoMutex> lock(queue_lock_);
                queue_.push(std::forward<U>(t));
            }

            read_block_.Wakeup();
        }

        // read
        template <typename U>
        void operator>>(U & t)
        {
            write_block_.Wakeup();
            read_block_.CoBlockWait();

            {
                std::unique_lock<CoMutex> lock(queue_lock_);
                t = std::move(queue_.front());
                queue_.pop();
            }
        }

        // read and ignore
        void operator>>(nullptr_t ignore)
        {
            write_block_.Wakeup();
            read_block_.CoBlockWait();

            {
                std::unique_lock<CoMutex> lock(queue_lock_);
                queue_.pop();
            }
        }

        // write
        template <typename U, typename Duration>
        bool TimedPush(U && t, Duration const& dur)
        {
			if (!write_block_.CoBlockWaitTimed(std::chrono::duration_cast<MininumTimeDurationType>(dur)))
                return false;

            {
                std::unique_lock<CoMutex> lock(queue_lock_);
                queue_.push(std::forward<U>(t));
            }

            read_block_.Wakeup();
            return true;
        }

        // read
        template <typename U, typename Duration>
        bool TimedPop(U & t, Duration const& dur)
        {
            write_block_.Wakeup();
			if (!read_block_.CoBlockWaitTimed(std::chrono::duration_cast<MininumTimeDurationType>(dur)))
            {
                if (write_block_.TryBlockWait())
                    return false;
                else
                {
                    while (!read_block_.TryBlockWait())
                        if (write_block_.TryBlockWait())
                            return false;
                        else
                            g_Scheduler.CoYield();
                }
            }

            std::unique_lock<CoMutex> lock(queue_lock_);
            t = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        // read and ignore
        template <typename Duration>
        bool TimedPop(nullptr_t ignore, Duration const& dur)
        {
            write_block_.Wakeup();
            if (!read_block_.CoBlockWaitTimed(std::chrono::duration_cast<MininumTimeDurationType>(dur)))
            {
                if (write_block_.TryBlockWait())
                    return false;
                else
                {
                    while (!read_block_.TryBlockWait())
                        if (write_block_.TryBlockWait())
                            return false;
                        else
                            g_Scheduler.CoYield();
                }
            }

            std::unique_lock<CoMutex> lock(queue_lock_);
            queue_.pop();
            return true;
        }


        // try write
        template <typename U>
        bool TryPush(U && t)
        {
            if (!write_block_.TryBlockWait())
                return false;

            {
                std::unique_lock<CoMutex> lock(queue_lock_);
                queue_.push(std::forward<U>(t));
            }

            read_block_.Wakeup();
            return true;
        }

        // try read
        template <typename U>
        bool TryPop(U & t)
        {
            write_block_.Wakeup();
            while (!read_block_.TryBlockWait())
                if (write_block_.TryBlockWait())
                    return false;
                else
                    g_Scheduler.CoYield();

            {
                std::unique_lock<CoMutex> lock(queue_lock_);
                t = std::move(queue_.front());
                queue_.pop();
            }
            return true;
        }

        // try read and ignore
        bool TryPop(nullptr_t ignore)
        {
            write_block_.Wakeup();
            while (!read_block_.TryBlockWait())
                if (write_block_.TryBlockWait())
                    return false;
                else
                    g_Scheduler.CoYield();

            {
                std::unique_lock<CoMutex> lock(queue_lock_);
                queue_.pop();
            }
            return true;
        }
    };


};


template <>
class Channel<void>
{
private:
    class ChannelImpl;
    mutable std::shared_ptr<ChannelImpl> impl_;

public:
    explicit Channel(std::size_t capacity = 0)
    {
        impl_.reset(new ChannelImpl(capacity));
    }
    ~Channel()
    {
    }

    Channel const& operator<<(nullptr_t ignore) const
    {
        (*impl_) << ignore;
        return *this;
    }

    Channel const& operator>>(nullptr_t ignore) const
    {
        (*impl_) >> ignore;
        return *this;
    }

    bool TryPush(nullptr_t ignore) const
    {
        return impl_->TryPush(ignore);
    }

    bool TryPop(nullptr_t ignore) const
    {
        return impl_->TryPop(ignore);
    }

    template <typename Duration>
    bool TimedPush(nullptr_t ignore, Duration const& timeout) const
    {
        return impl_->TimedPush(ignore, timeout);
    }

    template <typename Duration>
    bool TimedPop(nullptr_t ignore, Duration const& timeout) const
    {
        return impl_->TimedPop(ignore, timeout);
    }

    bool Unique() const
    {
        return impl_.unique();
    }

    bool empty() const
    {
        return impl_->empty();
    }

private:
    class ChannelImpl
    {
        BlockObject write_block_;
        BlockObject read_block_;

    public:
        explicit ChannelImpl(std::size_t capacity)
            : write_block_(capacity)
        {}

        bool empty()
        {
            return !read_block_.IsWakeup();
        }

        // write
        void operator<<(nullptr_t ignore)
        {
            write_block_.CoBlockWait();
            read_block_.Wakeup();
        }

        // read and ignore
        void operator>>(nullptr_t ignore)
        {
            write_block_.Wakeup();
            read_block_.CoBlockWait();
        }

        // write
        template <typename Duration>
        bool TimedPush(nullptr_t ignore, Duration const& dur)
        {
			if (!write_block_.CoBlockWaitTimed(std::chrono::duration_cast<MininumTimeDurationType>(dur)))
                return false;

            read_block_.Wakeup();
            return true;
        }

        // read
        template <typename Duration>
        bool TimedPop(nullptr_t ignore, Duration const& dur)
        {
            write_block_.Wakeup();
			if (!read_block_.CoBlockWaitTimed(std::chrono::duration_cast<MininumTimeDurationType>(dur)))
            {
                if (write_block_.TryBlockWait())
                    return false;
                else
                {
                    while (!read_block_.TryBlockWait())
                        if (write_block_.TryBlockWait())
                            return false;
                        else
                            g_Scheduler.CoYield();
                }
            }

            return true;
        }

        // try write
        bool TryPush(nullptr_t ignore)
        {
            if (!write_block_.TryBlockWait())
                return false;

            read_block_.Wakeup();
            return true;
        }

        // try read and ignore
        bool TryPop(nullptr_t ignore)
        {
            write_block_.Wakeup();
            while (!read_block_.TryBlockWait())
                if (write_block_.TryBlockWait())
                    return false;
                else
                    g_Scheduler.CoYield();

            return true;
        }
    };
};

} //namespace co