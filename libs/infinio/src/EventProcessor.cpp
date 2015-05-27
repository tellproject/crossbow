#include "EventProcessor.hpp"

#include "Logging.hpp"

#include <cerrno>

#include <sys/epoll.h>
#include <sys/eventfd.h>

#define PROCESSOR_LOG(...) INFINIO_LOG("[EventProcessor] " __VA_ARGS__)
#define PROCESSOR_ERROR(...) INFINIO_ERROR("[EventProcessor] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

void EventProcessor::init(std::error_code& ec) {
    PROCESSOR_LOG("Creating epoll file descriptor");
    errno = 0;
    mEpoll = epoll_create1(EPOLL_CLOEXEC);
    if (mEpoll == -1) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
}

void EventProcessor::shutdown(std::error_code& ec) {
    if (mEpoll == -1) {
        return;
    }

    auto res = close(mEpoll);
    if (res != 0) {
        ec = std::error_code(res, std::system_category());
        return;
    }

    // TODO We have to join the poll thread
    // We are not allowed to call join in the same thread as the poll loop is
}

void EventProcessor::registerPoll(int fd, EventPoll* poll, std::error_code& ec) {
    PROCESSOR_LOG("Register event poller");

    struct epoll_event event;
    event.data.ptr = poll;
    event.events = EPOLLIN | EPOLLET;

    errno = 0;
    auto res = epoll_ctl(mEpoll, EPOLL_CTL_ADD, fd, &event);
    if (res == -1) {
        ec = std::error_code(errno, std::system_category());
        return;
    }

    mPoller.emplace_back(poll);
}

void EventProcessor::start() {
    mPollThread = std::thread([this] () {
        while (true) {
            doPoll();
        }
    });
}

void EventProcessor::doPoll() {
    for (uint32_t i = 0; i < mPollCycles; ++i) {
        for (auto poller : mPoller) {
            if (poller->poll()) {
                i = 0;
            }
        }
    }

    for (auto poller : mPoller) {
        poller->prepareSleep();
    }

    PROCESSOR_LOG("Going to epoll sleep");
    struct epoll_event events[mPoller.size()];
    auto num = epoll_wait(mEpoll, events, 10, -1);
    PROCESSOR_LOG("Wake up from epoll sleep with %1% events", num);

    for (int i = 0; i < num; ++i) {
        if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
            PROCESSOR_ERROR("Error has occured on fd");
            continue;
        }

        auto poller = reinterpret_cast<EventPoll*>(events[i].data.ptr);
        poller->wakeup();
    }
}

void TaskQueue::init(std::error_code& ec) {
    errno = 0;
    mInterrupt = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (mInterrupt == -1) {
        ec = std::error_code(errno, std::system_category());
        return;
    }

    mProcessor.registerPoll(mInterrupt, this, ec);
}

void TaskQueue::shutdown(std::error_code& ec) {
    if (mInterrupt == -1) {
        return;
    }

    auto res = close(mInterrupt);
    if (res != 0) {
        ec = std::error_code(res, std::system_category());
        return;
    }
}

void TaskQueue::execute(std::function<void ()> fun, std::error_code& ec) {
    // TODO This can lead to a deadlock because write blocks when the queue is full
    mTaskQueue.write(std::move(fun));
    if (mSleeping.load()) {
        uint64_t counter = 0x1u;
        write(mInterrupt, &counter, sizeof(uint64_t));
    }
}

bool TaskQueue::poll() {
    bool result = false;

    // Process all task from the task queue
    std::function<void()> fun;
    while (mTaskQueue.read(fun)) {
        result = true;
        fun();
    }

    return result;
}

void TaskQueue::prepareSleep() {
    auto wasSleeping = mSleeping.exchange(true);
    if (wasSleeping) {
        return;
    }

    // Poll once more for tasks enqueud in the meantime
    poll();
}

void TaskQueue::wakeup() {
    mSleeping.store(false);

    uint64_t counter = 0;
    read(mInterrupt, &counter, sizeof(uint64_t));
}

} // namespace infinio
} // namespace crossbow
