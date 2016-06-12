#pragma once
#include "ts_queue.h"
#include "task.h"

namespace co
{

// �źŹ������
// @�̰߳�ȫ

class BlockObject
{
protected:
    friend class Processer;
    std::size_t wakeup_;        // ��ǰ�ź�����
    std::size_t max_wakeup_;    // ���Ի��۵��ź���������
    TSQueue<Task, false> wait_queue_;   // �ȴ��źŵ�Э�̶���
    LFLock lock_;

public:
    explicit BlockObject(std::size_t init_wakeup = 0, std::size_t max_wakeup = -1);
    ~BlockObject();

    // ����ʽ�ȴ��ź�
    void CoBlockWait();

    // ����ʱ������ʽ�ȴ��ź�
    // @returns: �Ƿ�ɹ��ȵ��ź�
	bool CoBlockWaitTimed(MininumTimeDurationType timeo);

    template <typename R, typename P>
    bool CoBlockWaitTimed(std::chrono::duration<R, P> duration)
    {
        return CoBlockWaitTimed(std::chrono::duration_cast<MininumTimeDurationType>(duration));
    }

    template <typename Clock, typename Dur>
    bool CoBlockWaitTimed(std::chrono::time_point<Clock, Dur> const& deadline)
    {
        auto now = Clock::now();
        if (deadline < now)
            return CoBlockWaitTimed(MininumTimeDurationType(0));

        return CoBlockWaitTimed(std::chrono::duration_cast<MininumTimeDurationType>
                (deadline - Clock::now()));
    }

    bool TryBlockWait();

    bool Wakeup();

    bool IsWakeup();

private:
    void CancelWait(Task* tk, uint32_t block_sequence, bool in_timer = false);

    bool AddWaitTask(Task* tk);
};

} //namespace co
