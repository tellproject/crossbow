#pragma once

#include <crossbow/infinio/InfinibandBuffer.hpp>
#include <crossbow/infinio/InfinibandLimits.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>

#include "EventProcessor.hpp"

#include <sparsehash/dense_hash_map>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <stack>
#include <system_error>
#include <vector>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class DeviceContext;
class InfinibandBuffer;
class LocalMemoryRegion;

/**
 * @brief The CompletionContext class manages a completion queue on the device
 *
 * Sets up a completion queue and manages any sockets associated with the completion queue. Starts an event processor
 * used to poll the queue and execute all callbacks to sockets.
 */
class CompletionContext : private EventPoll {
public:
    CompletionContext(DeviceContext& device, const InfinibandLimits& limits)
            : mDevice(device),
              mProcessor(limits.pollCycles),
              mTaskQueue(mProcessor),
              mSendBufferCount(limits.sendBufferCount),
              mSendBufferLength(limits.bufferLength),
              mSendQueueLength(limits.sendQueueLength),
              mCompletionQueueLength(limits.completionQueueLength),
              mSendDataRegion(nullptr),
              mSendData(nullptr),
              mCompletionChannel(nullptr),
              mCompletionQueue(nullptr),
              mSleeping(false),
              mShutdown(false) {
        mSocketMap.set_empty_key(0x1u << 25);
        mSocketMap.set_deleted_key(0x1u << 26);
    }

    ~CompletionContext() {
    }

    void init(std::error_code& ec);

    void shutdown(std::error_code& ec);

    /**
     * @brief Executes the function in the event loop
     *
     * The function will be queued and executed by the poll thread.
     *
     * @param fun The function to execute in the poll thread
     * @param ec Error code in case enqueueing the function failed
     */
    void execute(std::function<void()> fun, std::error_code& ec) {
        mTaskQueue.execute(std::move(fun), ec);
    }

    /**
     * @brief Add a new connection to completion queue
     *
     * Creates queue pairs for the new connection using the shared receive and completion queue and a dedicated send
     * queue.
     *
     * The connection has to be associated with the device this context belongs to.
     *
     * @param id The RDMA ID associated with the socket
     * @param socket The socket to add
     * @param ec Error code in case the initialization failed
     */
    void addConnection(struct rdma_cm_id* id, InfinibandSocket socket, std::error_code& ec);

    /**
     * @brief Drains any events from a previously added connection
     *
     * Processes all pending work completions from the connections completion queue. After all handler finished their
     * execution the connection gets removed. This function may only be called when the queue or the associated
     * completion queue will no longer receive any work completions (i.e. when disconnecting the connection).
     *
     * @param socket The socket to drain
     */
    void drainConnection(InfinibandSocket socket);

    /**
     * @brief Removes a previously added connection from the completion queue
     *
     * Immediately destroys the queue pair associated with the connection. This function may only be called when the
     * queue or the associated completion queue has no pending events (i.e. in case the connection failed before it was
     * established correctly).
     *
     * @param id The RDMA ID to remove
     * @param ec Error in case the removal failed
     */
    void removeConnection(struct rdma_cm_id* id, std::error_code& ec);

    /**
     * @brief Maximum buffer length this buffer manager is able to allocate
     */
    uint32_t bufferLength() const {
        return mSendBufferLength;
    }

    /**
     * @brief Acquire a send buffer from the shared pool with maximum size
     *
     * @return A newly acquired buffer or a buffer with invalid ID in case of an error
     */
    InfinibandBuffer acquireSendBuffer() {
        return acquireSendBuffer(mSendBufferLength);
    }

    /**
     * @brief Acquire a send buffer from the shared pool
     *
     * The given length is not allowed to exceed the maximum buffer size returned by CompletionContext::bufferLength();
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

private:
    /**
     * @brief Poll the completion queue and process any work completions
     */
    virtual bool poll() final override;

    /**
     * @brief Activates the completion channel
     */
    virtual void prepareSleep() final override;

    /**
     * @brief Invoked when the completion channel is readable
     *
     * Read all events and acknowledge them (the actual work completion events will be handled by the call to poll).
     */
    virtual void wakeup() final override;

    /**
     * @brief The offset into the associated memory region for a buffer with given ID
     */
    uint64_t bufferOffset(uint16_t id) {
        return (static_cast<uint64_t>(id) * static_cast<uint64_t>(mSendBufferLength));
    }

    /**
     * @brief Releases a send buffer to the shared buffer queue
     *
     * @param id The ID of the send buffer
     */
    void releaseSendBuffer(uint16_t id);

    /**
     * @brief Process a single work completion
     *
     * @param wc Work completion to process
     */
    void processWorkComplete(struct ibv_wc* wc);

    DeviceContext& mDevice;

    /// The event loop processor handling all Infiniband events
    EventProcessor mProcessor;

    /// Task queue to execute functions in the poll thread
    TaskQueue mTaskQueue;

    /// Number of shared send buffers to allocated
    uint16_t mSendBufferCount;

    /// Size of the allocated shared buffer
    uint32_t mSendBufferLength;

    /// Size of the send queue to allocate for each connection
    uint32_t mSendQueueLength;

    /// Size of the completion queue to allocate
    uint32_t mCompletionQueueLength;

    /// Memory region registered to the shared send buffer memory block
    struct ibv_mr* mSendDataRegion;

    /// Pointer to the shared send buffer arena
    void* mSendData;

    /// Stack containing IDs of send buffers that are not in use
    std::stack<uint16_t> mSendBufferQueue;

    /// Completion channel used to poll from epoll after a period of inactivity
    struct ibv_comp_channel* mCompletionChannel;

    /// Completion queue of this context
    struct ibv_cq* mCompletionQueue;

    /// Map from queue number to the associated socket
    google::dense_hash_map<uint32_t, InfinibandSocket> mSocketMap;

    /// Vector containing connections waiting to be drained
    std::vector<InfinibandSocket> mDrainingQueue;

    /// Whether the poller is sleeping (i.e. activated the completion channel)
    bool mSleeping;

    /// Whether the system is shutting down
    std::atomic<bool> mShutdown;
};

/**
 * @brief The DeviceContext class handles all activities related to a NIC
 *
 * Sets up the protection domain of the NIC and a shared receive queue for all connections. Also allocates a number of
 * CompletionContexts.
 */
class DeviceContext {
public:
    DeviceContext(const InfinibandLimits& limits, struct ibv_context* verbs)
            : mReceiveBufferCount(limits.receiveBufferCount),
              mReceiveBufferLength(limits.bufferLength),
              mContextThreads(limits.contextThreads),
              mVerbs(verbs),
              mProtectionDomain(nullptr),
              mReceiveDataRegion(nullptr),
              mReceiveData(nullptr),
              mReceiveQueue(nullptr),
              mShutdown(false) {
        mCompletion.reserve(mContextThreads);
        for (uint64_t i = 0; i < mContextThreads; ++i) {
            mCompletion.emplace_back(new CompletionContext(*this, limits));
        }
    }

    ~DeviceContext() {
        std::error_code ec;
        shutdown(ec);
        if (ec) {
            // TODO Log error?
        }
    }

    CompletionContext* context(uint64_t num) {
        return mCompletion.at(num % mContextThreads).get();
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
    void init(std::error_code& ec);

    /**
     * @brief Shuts the device down and destroys all associated ressources
     *
     * @param ec Error code in case the initialization failed
     */
    void shutdown(std::error_code& ec);

    /**
     * @brief Registers a new local memory region
     *
     * @param data Start pointer to the memory region
     * @param length Length of the memory region
     * @param access Infiniband access flags
     * @param ec Error code in case the registration failed
     * @return The newly registered memory region
     */
    LocalMemoryRegion registerMemoryRegion(void* data, size_t length, int access, std::error_code& ec);

private:
    friend class CompletionContext;

    /**
     * @brief Initializes the memory region for the shared receive and send buffers
     *
     * Pushes the IDs for the shared send buffers onto the send buffer queue.
     *
     * @param ec Error code in case the initialization failed
     */
    struct ibv_mr* allocateMemoryRegion(size_t length, std::error_code& ec);

    /**
     * @brief Shutsdown the memory region for the shared receive and send buffers
     *
     * @param ec Error code in case the shutdown failed
     */
    void destroyMemoryRegion(ibv_mr* memoryRegion, std::error_code& ec);

    /**
     * @brief Initializes the shared receive queue
     *
     * @param ec Error code in case the initialization failed
     */
    void initReceiveQueue(std::error_code& ec);

    /**
     * @brief The offset into the associated memory region for a buffer with given ID
     */
    uint64_t bufferOffset(uint16_t id) {
        return (static_cast<uint64_t>(id) * static_cast<uint64_t>(mReceiveBufferLength));
    }

    /**
     * @brief Posts the receive buffer with the given ID to the shared receive queue
     *
     * @param id The ID of the receive buffer
     * @param ec Error code in case the post failed
     */
    void postReceiveBuffer(uint16_t id, std::error_code& ec);

    /// Number of shared receive buffers to allocate
    uint16_t mReceiveBufferCount;

    /// Size of the allocated shared buffer
    uint32_t mReceiveBufferLength;

    /// Number of poll threads each with their own completion context
    uint64_t mContextThreads;

    /// Infiniband context associated with this device
    struct ibv_context* mVerbs;

    /// Protection domain associated with this device
    struct ibv_pd* mProtectionDomain;

    /// Memory region registered to the shared receive / send buffer memory block
    /// The block contains the receive buffers first and then the send buffers
    struct ibv_mr* mReceiveDataRegion;

    /// Pointer to the shared receive buffer arena
    void* mReceiveData;

    /// Shared receive queue associated with this device
    struct ibv_srq* mReceiveQueue;

    std::vector<std::unique_ptr<CompletionContext>> mCompletion;

    std::atomic<bool> mShutdown;
};

} // namespace infinio
} // namespace crossbow
