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

/**
 * @brief Wrapper managing a mmap memory region
 */
class MmapRegion {
public:
    /**
     * @brief Acquires a memory region
     *
     * @param length Length of the memory region to mmap
     *
     * @exception std::system_error In case mapping the memory failed
     */
    MmapRegion(size_t length);

    /**
     * @brief Releases the memory region
     */
    ~MmapRegion();

    MmapRegion(const MmapRegion&) = delete;
    MmapRegion& operator=(const MmapRegion&) = delete;

    MmapRegion(MmapRegion&& other)
            : mData(other.mData),
              mLength(other.mLength) {
        other.mData = nullptr;
        other.mLength = 0;
    }

    MmapRegion& operator=(MmapRegion&& other);

    void* data() {
        return const_cast<void*>(const_cast<const MmapRegion*>(this)->data());
    }

    const void* data() const {
        return mData;
    }

    size_t length() const {
        return mLength;
    }

private:
    void* mData;
    size_t mLength;
};

/**
 * @brief Wrapper managing a protection domain
 */
class ProtectionDomain {
public:
    /**
     * @brief Allocates a protection domain
     *
     * @param context Verbs context to allocate the protection domain with
     *
     * @exception std::system_error In case allocating the domain failed
     */
    ProtectionDomain(struct ibv_context* context);

    /**
     * @brief Deallocates the protection domain
     */
    ~ProtectionDomain();

    ProtectionDomain(const ProtectionDomain&) = delete;
    ProtectionDomain& operator=(const ProtectionDomain&) = delete;

    ProtectionDomain(ProtectionDomain&& other)
            : mDomain(other.mDomain) {
        other.mDomain = nullptr;
    }

    ProtectionDomain& operator=(ProtectionDomain&& other);

    struct ibv_pd* get() const {
        return mDomain;
    }

private:
    struct ibv_pd* mDomain;
};

/**
 * @brief Wrapper managing a shared receive queue
 */
class SharedReceiveQueue {
public:
    /**
     * @brief Creates a shared receive queue
     *
     * @param domain Protection domain to associate the queue with
     * @param length Length of the queue (i.e. maximum number of work requests posted to the queue)
     *
     * @exception std::system_error In case creating the queue failed
     */
    SharedReceiveQueue(const ProtectionDomain& domain, uint32_t length);

    /**
     * @brief Destroys the shared receive queue
     */
    ~SharedReceiveQueue();

    SharedReceiveQueue(const SharedReceiveQueue&) = delete;
    SharedReceiveQueue& operator=(const SharedReceiveQueue&) = delete;

    SharedReceiveQueue(SharedReceiveQueue&& other)
            : mQueue(other.mQueue) {
        other.mQueue = nullptr;
    }

    SharedReceiveQueue& operator=(SharedReceiveQueue&& other);

    struct ibv_srq* get() const {
        return mQueue;
    }

    /**
     * @brief Post the buffer to the shared receive queue
     *
     * @param buffer The buffer to post on the queue
     * @param ec Error code in case posting the buffer failed
     */
    void postBuffer(InfinibandBuffer& buffer, std::error_code& ec);

private:
    struct ibv_srq* mQueue;
};

/**
 * @brief Wrapper managing a completion channel
 */
class CompletionChannel {
public:
    /**
     * @brief Creates a completion channel
     *
     * @param context Verbs context to allocate the completion channel with
     *
     * @exception std::system_error In case allocating the channel failed
     */
    CompletionChannel(struct ibv_context* context);

    /**
     * @brief Destroys the completion channel
     */
    ~CompletionChannel();

    CompletionChannel(const CompletionChannel&) = delete;
    CompletionChannel& operator=(const CompletionChannel&) = delete;

    CompletionChannel(CompletionChannel&& other)
            : mChannel(other.mChannel) {
        other.mChannel = nullptr;
    }

    CompletionChannel& operator=(CompletionChannel&& other);

    int fd() const {
        return mChannel->fd;
    }

    /**
     * @brief Sets the non blocking mode of the channel's file descriptor.
     *
     * @param mode Whether to set the fd into nonblocking or blocking mode
     */
    void nonBlocking(bool mode);

    /**
     * @brief Retrieve pending events from the completion channel
     *
     * @param cq
     * @return Number of events retrieved
     */
    int retrieveEvents(struct ibv_cq** cq);

    struct ibv_comp_channel* get() const {
        return mChannel;
    }

private:
    struct ibv_comp_channel* mChannel;
};

/**
 * @brief Wrapper managing a completion queue
 */
class CompletionQueue {
public:
    /**
     * @brief Creates a completion queue
     *
     * @param context Verbs context to allocate the completion queue with
     * @param channel Completino channel to associate the queue with
     * @param length Length of the queue (i.e. maximum number of work completions pending on the queue)
     *
     * @exception std::system_error In case allocating the queue failed
     */
    CompletionQueue(struct ibv_context* context, const CompletionChannel& channel, int length);

    /**
     * @brief Destroys the completion queue
     */
    ~CompletionQueue();

    CompletionQueue(const CompletionQueue&) = delete;
    CompletionQueue& operator=(const CompletionQueue&) = delete;

    CompletionQueue(CompletionQueue&& other)
            : mQueue(other.mQueue) {
        other.mQueue = nullptr;
    }

    CompletionQueue& operator=(CompletionQueue&& other);

    /**
     * @brief Poll the queue for new work completions
     *
     * @param num Maximum number of work completions to retrieve
     * @param wc Array where to store the retrieved work completions
     * @return Number of retrieved work completions
     */
    int poll(int num, struct ibv_wc* wc) {
        return ibv_poll_cq(mQueue, num, wc);
    }

    /**
     * @brief Acknowledge events from the completion channel
     *
     * @param num Number of events to acknowledge
     */
    void ackEvents(int num) {
        ibv_ack_cq_events(mQueue, num);
    }

    /**
     * @brief Request a event notification on the completion channel for the next work completion event
     *
     * @param ec Error code in case requesting the notification failed
     */
    void requestEvent(std::error_code& ec);

    struct ibv_cq* get() const {
        return mQueue;
    }

private:
    struct ibv_cq* mQueue;
};

/**
 * @brief The CompletionContext class manages a completion queue on the device
 *
 * Sets up a completion queue and manages any sockets associated with the completion queue. Starts an event processor
 * used to poll the queue and execute all callbacks to sockets.
 */
class CompletionContext : private EventPoll {
public:
    CompletionContext(DeviceContext& device, const InfinibandLimits& limits);

    ~CompletionContext();

    void shutdown();

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
     *
     * @exception std::system_error In case adding the connection failed
     */
    void addConnection(struct rdma_cm_id* id, InfinibandSocket socket);

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
     */
    void removeConnection(struct rdma_cm_id* id);

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

    /// Maximum supported number of scatter gather elements
    uint32_t mMaxScatterGather;

    /// Size of the completion queue to allocate
    uint32_t mCompletionQueueLength;

    /// Pointer to the shared send buffer arena
    MmapRegion mSendData;

    /// Memory region registered to the shared send buffer memory block
    LocalMemoryRegion mSendDataRegion;

    /// Stack containing IDs of send buffers that are not in use
    std::stack<uint16_t> mSendBufferQueue;

    /// Completion channel used to poll from epoll after a period of inactivity
    CompletionChannel mCompletionChannel;

    /// Completion queue of this context
    CompletionQueue mCompletionQueue;

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
    /**
     * @brief Initializes the Infiniband device
     *
     * Initializes the protection domain for this device, allocates a buffer pool for use with this device, creates a
     * shared receive queue to handle all incoming requests and starts a completion context for all processor threads.
     */
    DeviceContext(const InfinibandLimits& limits, struct ibv_context* verbs);

    CompletionContext* context(uint64_t num) {
        return mCompletion.at(num % mCompletion.size()).get();
    }

    /**
     * @brief Shuts the device down
     */
    void shutdown();

    /**
     * @brief Registers a new local memory region
     *
     * @param data Start pointer to the memory region
     * @param length Length of the memory region
     * @param access Infiniband access flags
     * @return The newly registered memory region
     */
    LocalMemoryRegion registerMemoryRegion(void* data, size_t length, int access) {
        return LocalMemoryRegion(mProtectionDomain, data, length, access);
    }

    LocalMemoryRegion registerMemoryRegion(MmapRegion& region, int access) {
        return LocalMemoryRegion(mProtectionDomain, region, access);
    }

    CompletionChannel createCompletionChannel() {
        return CompletionChannel(mVerbs);
    }

    CompletionQueue createCompletionQueue(const CompletionChannel& channel, int length) {
        return CompletionQueue(mVerbs, channel, length);
    }

    struct ibv_pd* protectionDomain() {
        return mProtectionDomain.get();
    }

    struct ibv_srq* receiveQueue() {
        return mReceiveQueue.get();
    }

    InfinibandBuffer acquireReceiveBuffer(uint16_t id) {
        auto offset = static_cast<uint64_t>(id) * static_cast<uint64_t>(mReceiveBufferLength);
        return mReceiveDataRegion.acquireBuffer(id, offset, mReceiveBufferLength);
    }

    /**
     * @brief Posts the receive buffer to the shared receive queue
     *
     * @param buffer The receive buffer to post
     */
    void postReceiveBuffer(InfinibandBuffer& buffer);

    /**
     * @brief Posts the receive buffer with the given ID to the shared receive queue
     *
     * @param id The ID of the receive buffer
     */
    void postReceiveBuffer(uint16_t id) {
        auto buffer = acquireReceiveBuffer(id);
        postReceiveBuffer(buffer);
    }

private:
    /// Number of shared receive buffers to allocate
    uint16_t mReceiveBufferCount;

    /// Size of the allocated shared buffer
    uint32_t mReceiveBufferLength;

    /// Infiniband context associated with this device
    struct ibv_context* mVerbs;

    /// Protection domain associated with this device
    ProtectionDomain mProtectionDomain;

    /// Pointer to the shared receive buffer arena
    MmapRegion mReceiveData;

    /// Memory region registered to the shared receive buffer memory block
    LocalMemoryRegion mReceiveDataRegion;

    /// Shared receive queue associated with this device
    SharedReceiveQueue mReceiveQueue;

    std::vector<std::unique_ptr<CompletionContext>> mCompletion;

    std::atomic<bool> mShutdown;
};

} // namespace infinio
} // namespace crossbow
