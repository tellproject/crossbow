#pragma once

#include "BufferManager.hpp"

#include <crossbow/concurrent_map.hpp>

#include <atomic>
#include <cstddef>

#include <boost/lockfree/queue.hpp>
#include <boost/system/error_code.hpp>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class EventDispatcher;
class SocketImplementation;

/**
 * @brief The CompletionContext class manages a completion queue on the device
 *
 * Sets up a completion qeueu and manages any sockets associated with the completion queue.
 */
class CompletionContext {
public:
    CompletionContext(BufferManager& bufferManager)
            : mBufferManager(bufferManager),
              mProtectionDomain(nullptr),
              mReceiveQueue(nullptr),
              mCompletionQueue(nullptr),
              mShutdown(false) {
    }

    ~CompletionContext() {
    }

    void init(struct ibv_pd* protectionDomain, struct ibv_srq* receiveQueue, boost::system::error_code& ec);

    void shutdown(boost::system::error_code& ec);

    /**
     * @brief Add a new connection to completion queue
     *
     * Creates queue pairs for the new connection using the shared receive and completion queue and a dedicated send
     * queue.
     *
     * The connection has to be associated with the device this context belongs to.
     *
     * @param impl The socket implementation to add
     * @param ec Error code in case the initialization failed
     */
    void addConnection(SocketImplementation* impl, boost::system::error_code& ec);

    /**
     * @brief Drains any events from a previously added connection
     *
     * Processes all pending work completions from the connections completion queue. After all handler finished their
     * execution the connection gets removed. This function may only be called when the queue or the associated
     * completion queue will no longer receive any work completions (i.e. when disconnecting the connection).
     *
     * @param impl The socket implementation to drain
     * @param ec Error code in case the draining failed
     */
    void drainConnection(SocketImplementation* impl, boost::system::error_code& ec);

    /**
     * @brief Removes a previously added connection from the completion queue
     *
     * Immediately destroys the queue pair associated with the connection. This function may only be called when the
     * queue or the associated completion queue has no pending events (i.e. in case the connection failed before it was
     * established correctly).
     *
     * @param impl The socket implementation to remove
     * @param ec Error in case the removal failed
     */
    void removeConnection(SocketImplementation* impl, boost::system::error_code& ec);

    /**
     * @brief Poll the completion queue and process any work completions
     *
     * @param dispatcher The dispatcher to execute the callback functions
     * @param ec Error in case the polling process failed
     */
    void poll(EventDispatcher& dispatcher, boost::system::error_code& ec);

private:
    /**
     * @brief Process a single work completion
     *
     * @param dispatcher The dispatcher to execute the callback functions
     * @param wc Work completion to process
     */
    void processWorkComplete(EventDispatcher& dispatcher, struct ibv_wc* wc);

    /**
     * @brief Mark a new handler as active
     *
     * This function should be called before a handler associated with a callback on the socket is dispatched to the
     * EventDispatcher.
     *
     * @param impl Socket implementation the handler is associated with
     */
    void addWork(SocketImplementation* impl);

    /**
     * @brief Mark an active handler as completed
     *
     * This function should be called after the callback on the socket was executed.
     *
     * Tries to shutdown the connection in case the connection is draining and no more handlers are active.
     *
     * @param impl Socket implementation the handler was associated with
     */
    void removeWork(SocketImplementation* impl);

    /**
     * @brief Process a connection that was drained (i.e. has no more work completions on the completion queue)
     *
     * Tries to shutdown the connection in case the connection is draining and no more handlers are active.
     *
     * @param dispatcher The dispatcher to execute the callback functions
     * @param impl Socket implementation that was drained
     * @param ec Error in case the removal failed
     */
    void processDrainedConnection(EventDispatcher& dispatcher, SocketImplementation* impl,
            boost::system::error_code& ec);

    BufferManager& mBufferManager;

    struct ibv_pd* mProtectionDomain;
    struct ibv_srq* mReceiveQueue;
    struct ibv_cq* mCompletionQueue;

    /// Map from queue number to the associated socket implementation
    crossbow::concurrent_map<uint32_t, SocketImplementation*> mSocketMap;

    // TODO Use datastructure where capacity can be unbounded
    /// Queue containing connections waiting to be drained
    boost::lockfree::queue<SocketImplementation*, boost::lockfree::capacity<1024>> mDrainingQueue;

    std::atomic<bool> mShutdown;
};

/**
 * @brief The DeviceContext class handles all activities related to a NIC
 *
 * Sets up the protection domain of the NIC and a shared receive queue for all connections. Also allocates a
 * BufferManager and CompletionContext.
 */
class DeviceContext {
public:
    DeviceContext(EventDispatcher& dispatcher, struct ibv_context* verbs)
            : mDispatcher(dispatcher),
              mVerbs(verbs),
              mProtectionDomain(nullptr),
              mReceiveQueue(nullptr),
              mBufferManager(),
              mCompletion(mBufferManager),
              mShutdown(false) {
    }

    ~DeviceContext() {
        boost::system::error_code ec;
        shutdown(ec);
        if (ec) {
            // TODO Log error?
        }
    }

    struct ibv_context* context() {
        return mVerbs;
    }

    /**
     * @brief Initializes the device
     *
     * Initializes the protection domain for this device, allocates a buffer pool for use with this device, creates a
     * shared receive queue to handle all incoming requests and adds a completion queue to handle all future events.
     * After the succesfull initialization it starts polling the completion queue for entries on the event dispatcher.
     *
     * This function has to be called before using the device.
     *
     * @param ec Error code in case the initialization failed
     */
    void init(boost::system::error_code& ec);

    /**
     * @brief Shuts the device down and destroys all associated ressources
     *
     * @param ec Error code in case the initialization failed
     */
    void shutdown(boost::system::error_code& ec);

    void addConnection(SocketImplementation* impl, boost::system::error_code& ec) {
        mCompletion.addConnection(impl, ec);
    }

    void drainConnection(SocketImplementation* impl, boost::system::error_code& ec) {
        // TODO This function has to be invoked on the completion context associated with impl
        mCompletion.drainConnection(impl, ec);
    }

    void removeConnection(SocketImplementation* impl, boost::system::error_code& ec) {
        // TODO This function has to be invoked on the completion context associated with impl
        mCompletion.removeConnection(impl, ec);
    }

    InfinibandBuffer acquireBuffer(size_t length) {
        return mBufferManager.acquireBuffer(length);
    }

    void releaseBuffer(uint64_t id) {
        mBufferManager.releaseBuffer(id);
    }

private:
    /**
     * @brief Continously dispatches the Completion Context polling loop
     */
    void doPoll();

    EventDispatcher& mDispatcher;

    struct ibv_context* mVerbs;
    struct ibv_pd* mProtectionDomain;

    struct ibv_srq* mReceiveQueue;

    BufferManager mBufferManager;
    CompletionContext mCompletion;

    std::atomic<bool> mShutdown;
};

} // namespace infinio
} // namespace crossbow
