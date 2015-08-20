#pragma once

#include <crossbow/non_copyable.hpp>
#include <crossbow/singleconsumerqueue.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <queue>
#include <system_error>
#include <vector>

namespace crossbow {
namespace infinio {

class Fiber;

/**
 * @brief Interface providing an event poller invoked by the EventProcessor
 */
class EventPoll : private crossbow::non_copyable, crossbow::non_movable {
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
 * After a number of unsuccessful poll rounds the EventProcessor will go into epoll sleep and wake up whenever an event
 * on any fd is triggered.
 */
class EventProcessor : private crossbow::non_copyable, crossbow::non_movable {
public:
    /**
     * @brief Initializes the event processor
     *
     * Starts the event loop in its own thread.
     *
     * @exception std::system_error In case setting up the epoll descriptor failed
     */
    EventProcessor(uint64_t pollCycles, size_t fiberCacheSize);

    /**
     * @brief Shuts down the event processor
     */
    ~EventProcessor();

    /**
     * @brief Register a new event poller
     *
     * @param fd The file descriptor to listen for new events in case the event processor goes to sleep
     * @param poll The event poller polling for new events
     *
     * @exception std::system_error In case adding the fd fails
     */
    void registerPoll(int fd, EventPoll* poll);

    /**
     * @brief Deregister an existing event poller
     *
     * @param fd The file descriptor the poller is registered with
     * @param poll The event poller polling for events
     *
     * @exception std::system_error In case removing the fd fails
     */
    void deregisterPoll(int fd, EventPoll* poll);

    /**
     * @brief Start the event loop in its own thread
     */
    void start();

    /**
     * @brief Queue the function for later execution in the event processor
     *
     * Not thread-safe: Can only be called from within the poll thread.
     *
     * @param fun The function to execute in the event loop
     */
    void execute(std::function<void()> fun) {
        mTaskQueue.push(std::move(fun));
    }

    /**
     * @brief Execute the function as a new fiber
     *
     * The function is executed immediately.
     *
     * Not thread-safe: Can only be called from within the poll thread.
     *
     * @param fun The function to execute in the fiber
     */
    void executeFiber(std::function<void(Fiber&)> fun);

    /**
     * @brief Recycles the fiber for later reuse
     *
     * Adds the fiber to the cache.
     *
     * Not thread-safe: Can only be called from within the poll thread.
     *
     * @param fiber The fiber to recycle
     */
    void recycleFiber(Fiber* fiber);

private:
    /**
     * @brief Execute the event loop
     */
    void doPoll();

    /// Number of iterations without action to poll until going to epoll sleep
    uint64_t mPollCycles;

    /// Maximum size of the recycled fiber cache
    size_t mFiberCacheSize;

    /// File descriptor for epoll
    int mEpoll;

    /// Thread polling the completion and task queues
    std::thread mPollThread;

    /// Vector containing all registered event poller
    std::vector<EventPoll*> mPoller;

    /// Local task queue containing locally enqueued tasks
    std::queue<std::function<void()>> mTaskQueue;

    /// Cache for recycled fibers
    std::queue<Fiber*> mFiberCache;
};

/**
 * @brief Event Poller polling a task queue and executing tasks from it
 */
class TaskQueue : private EventPoll {
public:
    TaskQueue(EventProcessor& processor);

    ~TaskQueue();

    /**
     * @brief Enqueues the given function into the task queue
     *
     * Can only be called from outside the current event processor. In case the task queue reached maximum capacity the
     * call blocks until a free slot opens up in the task queue (this can lead to a deadlock if the queue is full and
     * execute is called from within the event processor).
     *
     * @param fun The function to execute in the poll thread
     */
    void execute(std::function<void()> fun);

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
