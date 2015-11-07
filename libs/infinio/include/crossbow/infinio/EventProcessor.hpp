/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
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
    EventProcessor(uint64_t pollCycles);

    /**
     * @brief Shuts down the event processor
     */
    ~EventProcessor();

    /**
     * @brief The ID of the polling thread
     */
    std::thread::id threadId() const {
        return mPollThread.get_id();
    }

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
    std::atomic<bool> mShutdown;
};

/**
 * @brief Event Poller polling a task queue and executing tasks from it
 *
 * The associated queue can only be accessed from outside the polling thread.
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

/**
 * @brief Event Poller polling a task queue and executing tasks from it
 *
 * The associated queue can only be accessed from inside the polling thread.
 */
class LocalTaskQueue : private EventPoll {
public:
    LocalTaskQueue(EventProcessor& processor);

    ~LocalTaskQueue();

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

private:
    virtual bool poll() final override;

    virtual void prepareSleep() final override;

    virtual void wakeup() final override;

    EventProcessor& mProcessor;

    /// Local task queue containing locally enqueued tasks
    std::queue<std::function<void()>> mTaskQueue;
};

} // namespace infinio
} // namespace crossbow
