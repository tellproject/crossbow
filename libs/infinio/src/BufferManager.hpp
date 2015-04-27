#pragma once

#include <crossbow/infinio/InfinibandBuffer.hpp>

#include <boost/lockfree/queue.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <cstring>
#include <limits>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

/**
 * @brief The BufferManager class allocates and provides a pool of buffers registered with a specific NIC
 *
 * Allocates a single big memory block from where multiple smaller buffers are created. Each buffer is assigned a
 * unique ID which determines the offset into the memory block.
 */
class BufferManager {
public:
    /**
     * @brief Maximum size of any buffer
     */
    static constexpr size_t gBufferLength = 1024;

    /**
     * @brief Number of buffers to be allocated
     */
    static constexpr size_t gBufferCount = 8;

    BufferManager()
            : mData(nullptr),
              mDataRegion(nullptr) {
    }

    ~BufferManager() {
        boost::system::error_code ec;
        shutdown(ec);
        if (ec) {
            // TODO Log error?
        }
    }

    /**
     * @brief Initializes the buffer pool
     *
     * Requests memory from the kernel and registers the memory with the given protection domain.
     *
     * @param protectionDomain Protection domain to associate the buffers with
     * @param ec Error in case the manager was unable to acquire memory
     */
    void init(struct ibv_pd* protectionDomain, boost::system::error_code& ec);

    /**
     * @brief Releases the memory associated with the buffer pool
     *
     * Any data references to previously acquired buffers are invalid and will lead to memory corruption or segmentation
     * faults.
     *
     * @param Error in case the manager was unable to acquire memory
     */
    void shutdown(boost::system::error_code& ec);

    /**
     * @brief Acquire a buffer from the pool
     *
     * The given length is not allowed to exceed the maximum buffer size specified in gBufferLength.
     *
     * Buffers acquired in this way have to be released to the pool using the releaseBuffer(uint64_t id) function when
     * they are no longer needed.
     *
     * @param length The desired size of the buffer
     *
     * @return A newly acquired buffer or a buffer with invalid ID in case of an error
     */
    InfinibandBuffer acquireBuffer(uint32_t length);

    /**
     * @brief Acquire a buffer with a specific ID from the pool
     *
     * It is not checked if the buffer is still available or already in use by another user.
     *
     * @param id Target ID of the buffer to acquire
     * @param length The desired size of the buffer
     *
     * @return The buffer with the given ID or a buffer with invalid ID in case of an error
     */
    InfinibandBuffer acquireBuffer(uint64_t id, uint32_t length);

    /**
     * @brief Releases the buffer with given ID back to the pool
     *
     * The data referenced by this buffer should not be accessed anymore.
     *
     * @param id Target ID of the buffer to release
     */
    void releaseBuffer(uint64_t id);

private:
    /// Pointer to the memory block acquired from the kernel
    void* mData;

    /// Memory region registered to the memory block
    struct ibv_mr* mDataRegion;

    /// Queue containing IDs of buffers that are not in use
    boost::lockfree::queue<uint64_t, boost::lockfree::capacity<gBufferCount>> mBufferQueue;
};

} // namespace infinio
} // namespace crossbow
