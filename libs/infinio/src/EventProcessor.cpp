#include <crossbow/infinio/EventProcessor.hpp>

#include <crossbow/infinio/Fiber.hpp>
#include <crossbow/logger.hpp>

#include <algorithm>
#include <cerrno>

#include <sys/epoll.h>
#include <sys/eventfd.h>

namespace crossbow {
namespace infinio {

EventProcessor::EventProcessor(uint64_t pollCycles, size_t fiberCacheSize)
        : mPollCycles(pollCycles),
          mFiberCacheSize(fiberCacheSize) {
    LOG_TRACE("Creating epoll file descriptor");
    mEpoll = epoll_create1(EPOLL_CLOEXEC);
    if (mEpoll == -1) {
        throw std::system_error(errno, std::generic_category());
    }
}

EventProcessor::~EventProcessor() {
    // TODO We have to join the poll thread
    // We are not allowed to call join in the same thread as the poll loop is

    LOG_TRACE("Destroying epoll file descriptor");
    if (close(mEpoll)) {
        std::error_code ec(errno, std::generic_category());
        LOG_ERROR("Failed to close the epoll descriptor [error = %1% %2%]", ec, ec.message());
    }
}

void EventProcessor::registerPoll(int fd, EventPoll* poll) {
    LOG_TRACE("Register event poller");
    struct epoll_event event;
    event.data.ptr = poll;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(mEpoll, EPOLL_CTL_ADD, fd, &event)) {
        throw std::system_error(errno, std::generic_category());
    }

    mPoller.emplace_back(poll);
}

void EventProcessor::deregisterPoll(int fd, EventPoll* poll) {
    LOG_TRACE("Deregister event poller");
    auto i = std::find(mPoller.begin(), mPoller.end(), poll);
    if (i == mPoller.end()) {
        return;
    }
    mPoller.erase(i);

    if (epoll_ctl(mEpoll, EPOLL_CTL_DEL, fd, nullptr)) {
        throw std::system_error(errno, std::generic_category());
    }
}

void EventProcessor::start() {
    LOG_TRACE("Starting event processor");
    mPollThread = std::thread([this] () {
        while (true) {
            doPoll();
        }
    });
}

void EventProcessor::executeFiber(std::function<void(Fiber&)> fun) {
    Fiber* fiber;
    if (mFiberCache.empty()) {
        fiber = Fiber::create(*this);
    } else {
        fiber = mFiberCache.front();
        mFiberCache.pop();
    }
    fiber->execute(std::move(fun));
}

void EventProcessor::recycleFiber(Fiber* fiber) {
    LOG_ASSERT(fiber != nullptr, "Fiber must be non-null");
    LOG_ASSERT(fiber->empty(), "Fiber to recycle not empty");
    if (mFiberCache.size() < mFiberCacheSize) {
        // Add fiber to cache
        mFiberCache.emplace(fiber);
    } else {
        // Queue fiber for delete (recycle function might be called from within the fiber)
        execute([fiber] () {
            delete fiber;
        });
    }
}

void EventProcessor::doPoll() {
    for (decltype(mPollCycles) i = 0; i < mPollCycles; ++i) {
        for (auto poller : mPoller) {
            if (poller->poll()) {
                i = 0;
            }
        }
        if (!mTaskQueue.empty()) {
            decltype(mTaskQueue) taskQueue;
            taskQueue.swap(mTaskQueue);
            do {
                taskQueue.front()();
                taskQueue.pop();
            } while (!taskQueue.empty());
            i = 0;
        }
    }

    for (auto poller : mPoller) {
        poller->prepareSleep();
    }

    LOG_TRACE("Going to epoll sleep");
    struct epoll_event events[mPoller.size()];
    auto num = epoll_wait(mEpoll, events, 10, -1);
    LOG_TRACE("Wake up from epoll sleep with %1% events", num);

    for (int i = 0; i < num; ++i) {
        if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
            LOG_ERROR("Error has occured on fd");
            continue;
        }

        auto poller = reinterpret_cast<EventPoll*>(events[i].data.ptr);
        poller->wakeup();
    }
}

TaskQueue::TaskQueue(EventProcessor& processor)
        : mProcessor(processor),
          mSleeping(false) {
    mInterrupt = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (mInterrupt == -1) {
        throw std::system_error(errno, std::generic_category());
    }

    mProcessor.registerPoll(mInterrupt, this);
}

TaskQueue::~TaskQueue() {
    try {
        mProcessor.deregisterPoll(mInterrupt, this);
    } catch (std::system_error& e) {
        LOG_ERROR("Failed to deregister from EventProcessor [error = %1% %2%]", e.code(), e.what());
    }

    if (close(mInterrupt)) {
        std::error_code ec(errno, std::generic_category());
        LOG_ERROR("Failed to close the event descriptor [error = %1% %2%]", ec, ec.message());
    }
}

void TaskQueue::execute(std::function<void()> fun) {
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
