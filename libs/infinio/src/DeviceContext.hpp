#pragma once

#include <crossbow/infinio/InfinibandBuffer.hpp>
#include <crossbow/infinio/InfinibandLimits.hpp>
#include <crossbow/concurrent_map.hpp>

#include <atomic>
#include <cstddef>

#include <boost/lockfree/queue.hpp>
#include <boost/system/error_code.hpp>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class DeviceContext;
class EventDispatcher;
class InfinibandBuffer;
class LocalMemoryRegion;
class SocketImplementation;

/**
 * @brief The CompletionContext class manages a completion queue on the device
 *
 * Sets up a completion queue and manages any sockets associated with the completion queue.
 */
class CompletionContext {
public:
    CompletionContext(DeviceContext& device, const InfinibandLimits& limits)
            : mDevice(device),
              mSendQueueLength(limits.sendQueueLength),
              mCompletionQueueLength(limits.completionQueueLength),
              mPollCycles(limits.pollCycles),
              mCompletionQueue(nullptr),
              mShutdown(false) {
    }

    ~CompletionContext() {
    }

    void init(boost::system::error_code& ec);

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

    DeviceContext& mDevice;

    /// Size of the send queue to allocate for each connection
    uint32_t mSendQueueLength;

    /// Size of the completion queue to allocate
    uint32_t mCompletionQueueLength;

    /// Number of iterations to poll when there is no work completion
    uint64_t mPollCycles;

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
    DeviceContext(EventDispatcher& dispatcher, const InfinibandLimits& limits, struct ibv_context* verbs)
            : mDispatcher(dispatcher),
              mReceiveBufferCount(limits.receiveBufferCount),
              mSendBufferCount(limits.sendBufferCount),
              mBufferLength(limits.bufferLength),
              mVerbs(verbs),
              mProtectionDomain(nullptr),
              mDataRegion(nullptr),
              mReceiveData(nullptr),
              mReceiveQueue(nullptr),
              mSendData(nullptr),
              mSendBufferQueue(limits.sendBufferCount),
              mCompletion(*this, limits),
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

    /**
     * @brief Maximum buffer length this buffer manager is able to allocate
     */
    uint32_t bufferLength() const {
        return mBufferLength;
    }

    /**
     * @brief Acquire a send buffer from the shared pool with maximum size
     *
     * @return A newly acquired buffer or a buffer with invalid ID in case of an error
     */
    InfinibandBuffer acquireSendBuffer() {
        return acquireSendBuffer(mBufferLength);
    }

    /**
     * @brief Acquire a send buffer from the shared pool
     *
     * The given length is not allowed to exceed the maximum buffer size returned by DeviceContext::bufferLength();
     *
     * @param length The desired size of the buffer
     * @return A newly acquired buffer or a buffer with invalid ID in case of an error
     */
    InfinibandBuffer acquireSendBuffer(uint32_t length);

    /**
     * @brief Releases the send buffer back to the pool
     *
     * The data referenced by this buffer should not be accessed anymore.
     *
     * Acquired buffers that were sent successfully do not have to be released only call this function if the send
     * failed or you did not need the buffer after all.
     *
     * @param buffer The previously acquired send buffer
     */
    void releaseSendBuffer(InfinibandBuffer& buffer);

    /**
     * @brief Registers a new local memory region
     *
     * @param data Start pointer to the memory region
     * @param length Length of the memory region
     * @param access Infiniband access flags
     * @param ec Error code in case the registration failed
     * @return The newly registered memory region
     */
    LocalMemoryRegion registerMemoryRegion(void* data, size_t length, int access, boost::system::error_code& ec);

private:
    friend class CompletionContext;

    /**
     * @brief Initializes the memory region for the shared receive and send buffers
     *
     * Pushes the IDs for the shared send buffers onto the send buffer queue.
     *
     * @param ec Error code in case the initialization failed
     */
    void initMemoryRegion(boost::system::error_code& ec);

    /**
     * @brief Shutsdown the memory region for the shared receive and send buffers
     *
     * @param ec Error code in case the shutdown failed
     */
    void shutdownMemoryRegion(boost::system::error_code& ec);

    /**
     * @brief Initializes the shared receive queue
     *
     * @param ec Error code in case the initialization failed
     */
    void initReceiveQueue(boost::system::error_code& ec);

    /**
     * @brief Continuously dispatches the Completion Context polling loop
     */
    void doPoll();

    /**
     * @brief The offset into the associated memory region for a buffer with given ID
     */
    uint64_t bufferOffset(uint16_t id) {
        return (static_cast<uint64_t>(id) * static_cast<uint64_t>(mBufferLength));
    }

    /**
     * @brief Releases a send buffer to the shared buffer queue
     *
     * @param id The ID of the send buffer
     */
    void releaseSendBuffer(uint16_t id);

    /**
     * @brief Posts the receive buffer with the given ID to the shared receive queue
     *
     * @param id The ID of the receive buffer
     * @param ec Error code in case the post failed
     */
    void postReceiveBuffer(uint16_t id, boost::system::error_code& ec);

    EventDispatcher& mDispatcher;

    /// Number of shared receive buffers to allocate
    uint16_t mReceiveBufferCount;

    /// Number of shared send buffers to allocated
    uint16_t mSendBufferCount;

    /// Size of the allocated shared buffer
    uint32_t mBufferLength;

    /// Infiniband context associated with this device
    struct ibv_context* mVerbs;

    /// Protection domain associated with this device
    struct ibv_pd* mProtectionDomain;

    /// Memory region registered to the shared receive / send buffer memory block
    /// The block contains the receive buffers first and then the send buffers
    struct ibv_mr* mDataRegion;

    /// Pointer to the shared receive buffer arena
    void* mReceiveData;

    /// Shared receive queue associated with this device
    struct ibv_srq* mReceiveQueue;

    /// Pointer to the shared send buffer arena
    void* mSendData;

    /// Queue containing IDs of send buffers that are not in use
    boost::lockfree::queue<uint16_t, boost::lockfree::fixed_sized<true>> mSendBufferQueue;

    CompletionContext mCompletion;

    std::atomic<bool> mShutdown;
};

} // namespace infinio
} // namespace crossbow
