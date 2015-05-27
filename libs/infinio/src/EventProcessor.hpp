#pragma once

#include <crossbow/singleconsumerqueue.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <system_error>
#include <vector>

namespace crossbow {
namespace infinio {

/**
 * @brief Interface providing an event poller invoked by the EventProcessor
 */
class EventPoll {
public:
    /**
     * @brief Poll and process any new events
     *
     * @return Whether there were any events processed
     */
    virtual bool poll() = 0;

    /**
     * @brief Prepare the poller for epoll sleep
     */
    virtual void prepareSleep() = 0;

    /**
     * @brief Wake up from the epoll sleep
     */
    virtual void wakeup() = 0;
};

/**
 * @brief Class providing a simple poll based event loop
 *
 * After a number of unsuccessful poll rounds the EventProcessor will go into epoll sleep and wake up whenever an event on any fd is triggered.
 */
class EventProcessor {
public:
    EventProcessor(uint64_t pollCycles)
            : mPollCycles(pollCycles),
              mEpoll(-1) {
    }

    /**
     * @brief Initializes the event processor
     *
     * @param ec Error code in case the initialization failed
     */
    void init(std::error_code& ec);

    /**
     * @brief Shuts down the event processor
     *
     * @param ec Error code in case the shutdown failed
     */
    void shutdown(std::error_code& ec);

    /**
     * @brief Register a new event poller
     *
     * @param fd The file descriptor to listen for new events in case the event processor goes to sleep
     * @param poll The event poller polling for new events
     * @param ec Error code in case adding the fd fails
     */
    void registerPoll(int fd, EventPoll* poll, std::error_code& ec);

    /**
     * @brief Start the event loop in its own thread
     */
    void start();

private:
    /**
     * @brief Execute the event loop
     */
    void doPoll();

    /// Number of iterations without action to poll until going to epoll sleep
    uint64_t mPollCycles;

    /// File descriptor for epoll
    int mEpoll;

    /// Thread polling the completion and task queues
    std::thread mPollThread;

    /// Vector containing all registered event poller
    std::vector<EventPoll*> mPoller;
};

/**
 * @brief Event Poller polling a task queue and executing tasks from it
 */
class TaskQueue : private EventPoll {
public:
    TaskQueue(EventProcessor& processor)
            : mProcessor(processor),
              mInterrupt(-1),
              mSleeping(false) {
    }

    void init(std::error_code& ec);

    void shutdown(std::error_code& ec);

    /**
     * @brief Enqueues the given function into the task queue
     *
     * @param fun The function to execute in the poll thread
     */
    void execute(std::function<void()> fun, std::error_code& ec);

private:
    virtual bool poll() final override;

    virtual void prepareSleep() final override;

    virtual void wakeup() final override;

    EventProcessor& mProcessor;

    /// Queue containing function objects to be executed by the poll thread
    crossbow::SingleConsumerQueue<std::function<void()>, 256> mTaskQueue;

    /// Eventfd triggered when the event processor is sleeping and another thread enqueues a task to the queue
    int mInterrupt;

    /// Whether the event processor is sleeping
    std::atomic<bool> mSleeping;
};

} // namespace infinio
} // namespace crossbow
