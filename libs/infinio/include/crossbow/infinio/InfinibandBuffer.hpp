#pragma once

#include <cstddef>
#include <cstring>
#include <limits>
#include <system_error>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

/**
 * @brief Buffer class pointing to a buffer space registered with an Infiniband device
 *
 * Each buffer has an unique ID associated with itself.
 */
class InfinibandBuffer {
public:
    static constexpr uint16_t INVALID_ID = std::numeric_limits<uint16_t>::max();

    InfinibandBuffer(uint16_t id)
            : mId(id) {
        memset(&mHandle, 0, sizeof(mHandle));
    }

    uint16_t id() const {
        return mId;
    }

    uint32_t length() const {
        return mHandle.length;
    }

    /**
     * @brief Whether the buffer is valid
     */
    bool valid() const {
        return (mId != INVALID_ID);
    }

    /**
     * @brief Shrinks the buffer space to the given length
     *
     * The new buffer length has to be smaller than the current length.
     *
     * @param length The new length of the buffer
     */
    void shrink(uint32_t length) {
        if (mHandle.length <= length) {
            return;
        }
        mHandle.length = length;
    }

    void* data() {
        return const_cast<void*>(const_cast<const InfinibandBuffer*>(this)->data());
    }

    const void* data() const {
        return reinterpret_cast<void*>(mHandle.addr);
    }

    struct ibv_sge* handle() {
        return &mHandle;
    }

private:
    struct ibv_sge mHandle;

    uint16_t mId;
};

/**
 * @brief The LocalMemoryRegion class provides access to a memory region on the local host
 *
 * Can be used to let a remote host read and write to the memory region or read/write data from this region to a remote
 * memory region.
 */
class LocalMemoryRegion {
public:
    LocalMemoryRegion(struct ibv_mr* dataRegion)
            : mDataRegion(dataRegion) {
    }

    ~LocalMemoryRegion() {
        std::error_code ec;
        releaseMemoryRegion(ec);
        if (ec) {
            // TODO Log error?
        }
    }

    /**
     * @brief Deregisters the memory region
     *
     * The data referenced by this memory region should not be used in any further RDMA operations.
     */
    void releaseMemoryRegion(std::error_code& ec);

    uintptr_t address() const {
        return reinterpret_cast<uintptr_t>(mDataRegion->addr);
    }

    size_t length() const {
        return mDataRegion->length;
    }

    uint32_t key() const {
        return mDataRegion->rkey;
    }

    /**
     * @brief Acquire a buffer from the memory region for use in RDMA read and write requests
     *
     * The user has to take care that buffers do not overlap or read/writes are serialized.
     *
     * @param id User specified buffer ID
     * @param offset Offset into the memory region the buffer should start
     * @param length Length of the buffer
     * @return A newly acquired buffer or a buffer with invalid ID in case of an error
     */
    InfinibandBuffer acquireBuffer(uint16_t id, uint64_t offset, uint32_t length);

private:
    struct ibv_mr* mDataRegion;
};

/**
 * @brief The RemoteMemoryRegion class provides access to a memory region on a remote host
 *
 * Can be used to read and write from/to the remote memory.
 */
class RemoteMemoryRegion {
public:
    RemoteMemoryRegion(uintptr_t address, size_t length, uint32_t key)
            : mAddress(address),
              mLength(length),
              mKey(key) {
    }

    uintptr_t address() const {
        return mAddress;
    }

    size_t length() const {
        return mLength;
    }

    uint32_t key() const {
        return mKey;
    }

private:
    uintptr_t mAddress;
    size_t mLength;
    uint32_t mKey;
};

} // namespace infinio
} // namespace crossbow
