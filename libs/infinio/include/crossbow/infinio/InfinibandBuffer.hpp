#pragma once

#include <cstddef>
#include <cstring>
#include <limits>
#include <system_error>
#include <vector>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class MmapRegion;
class ProtectionDomain;

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

    /**
     * @brief The number of buffers
     */
    size_t count() const {
        return 1;
    }

    /**
     * @brief The total data length of all buffers
     */
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
    friend class ScatterGatherBuffer;

    uint32_t lkey() const {
        return mHandle.lkey;
    }

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
    LocalMemoryRegion()
            : mDataRegion(nullptr) {
    }

    /**
     * @brief Registers the memory region
     *
     * @param domain The protection domain to register this region with
     * @param data The start pointer of the memory region
     * @param length The length of the memory region
     * @param access Access flags for the Infiniband Adapter
     */
    LocalMemoryRegion(const ProtectionDomain& domain, void* data, size_t length, int access);

    LocalMemoryRegion(const ProtectionDomain& domain, MmapRegion& region, int access);

    /**
     * @brief Deregisters the memory region
     *
     * The data referenced by this memory region should not be used in any further RDMA operations.
     */
    ~LocalMemoryRegion();

    LocalMemoryRegion(const LocalMemoryRegion&) = delete;
    LocalMemoryRegion& operator=(const LocalMemoryRegion&) = delete;

    LocalMemoryRegion(LocalMemoryRegion&& other)
            : mDataRegion(other.mDataRegion) {
        other.mDataRegion = nullptr;
    }

    LocalMemoryRegion& operator=(LocalMemoryRegion&& other);

    uintptr_t address() const {
        return reinterpret_cast<uintptr_t>(mDataRegion->addr);
    }

    size_t length() const {
        return mDataRegion->length;
    }

    uint32_t rkey() const {
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
    InfinibandBuffer acquireBuffer(uint16_t id, size_t offset, uint32_t length);

    /**
     * @brief Whether the buffer was acquired from this memory region
     */
    bool belongsToRegion(InfinibandBuffer& buffer) {
        return buffer.handle()->lkey == mDataRegion->lkey;
    }

private:
    friend class ScatterGatherBuffer;

    uint32_t lkey() const {
        return mDataRegion->lkey;
    }

    struct ibv_mr* mDataRegion;
};

/**
 * @brief The RemoteMemoryRegion class provides access to a memory region on a remote host
 *
 * Can be used to read and write from/to the remote memory.
 */
class RemoteMemoryRegion {
public:
    RemoteMemoryRegion()
            : mAddress(0x0u),
              mLength(0x0u),
              mKey(0x0u) {
    }

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

/**
 * @brief Buffer class wrapping many buffers into a single one to be used in scatter / gather operations
 */
class ScatterGatherBuffer {
public:
    ScatterGatherBuffer(uint16_t id)
            : mId(id),
              mLength(0) {
    }

    uint16_t id() const {
        return mId;
    }

    /**
     * @brief The number of buffers
     */
    size_t count() const {
        return mHandle.size();
    }

    /**
     * @brief The total data length of all buffers
     */
    size_t length() const {
        return mLength;
    }

    void add(const LocalMemoryRegion& region, const void* addr, uint32_t length);

    void add(const LocalMemoryRegion& region, size_t offset, uint32_t length) {
        add(region, reinterpret_cast<const void*>(region.address() + offset), length);
    }

    void add(const InfinibandBuffer& buffer, size_t offset, uint32_t length);

    void* data(size_t index) {
        return const_cast<void*>(const_cast<const ScatterGatherBuffer*>(this)->data(index));
    }

    const void* data(size_t index) const {
        return reinterpret_cast<const void*>(mHandle.at(index).addr);
    }

    struct ibv_sge* handle() {
        return mHandle.data();
    }

    /**
     * @brief Clears all buffers
     */
    void reset() {
        mHandle.clear();
        mLength = 0;
    }

private:
    std::vector<struct ibv_sge> mHandle;
    uint64_t mLength;

    uint16_t mId;
};

} // namespace infinio
} // namespace crossbow
