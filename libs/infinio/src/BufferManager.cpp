#include "BufferManager.hpp"

#include "ErrorCode.hpp"
#include "Logging.hpp"

#include <cerrno>

#include <sys/mman.h>

#define BUFFER_LOG(...) INFINIO_LOG("[BufferManager] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

void BufferManager::init(struct ibv_pd* protectionDomain, boost::system::error_code& ec) {
    // Check if already initialized
    if (mData) {
        ec = error::already_open;
        return;
    }

    auto dataLength = mBufferCount * mBufferLength;

    BUFFER_LOG("Map %1% bytes of buffer space", dataLength);
    errno = 0;
    mData = mmap(nullptr, dataLength, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (mData == MAP_FAILED) {
        mData = nullptr;
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    BUFFER_LOG("Create memory region at %1%", mData);
    errno = 0;
    mDataRegion = ibv_reg_mr(protectionDomain, mData, dataLength, IBV_ACCESS_LOCAL_WRITE);
    if (mDataRegion == nullptr) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    BUFFER_LOG("Add %1% buffers to buffer queue", mBufferCount);
    for (uint32_t id = 0x0u; id < mBufferCount; ++id) {
        mBufferQueue.push(id);
    }

    ec = boost::system::error_code();
}

void BufferManager::shutdown(boost::system::error_code& ec) {
    if (!mData) {
        return;
    }

    BUFFER_LOG("Destroy memory region at %1%", mData);
    if (auto res = ibv_dereg_mr(mDataRegion) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }

    // TODO Size has to be a multiple of the page size
    BUFFER_LOG("Unmap buffer space at %1%", mData);
    errno = 0;
    if (munmap(mData, mBufferCount * mBufferLength) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
    }
    mData = nullptr;

    ec = boost::system::error_code();
}

InfinibandBuffer BufferManager::acquireBuffer(uint32_t length) {
    uint64_t id = 0x0u;
    if (!mBufferQueue.pop(id)) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }
    return acquireBuffer(id, length);
}

InfinibandBuffer BufferManager::acquireBuffer(uint64_t id, uint32_t length) {
    if (id >= mBufferCount) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }
    if (length > mBufferLength) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }
    InfinibandBuffer buffer(id);
    buffer.handle()->addr = reinterpret_cast<uintptr_t>(mData) + (id * mBufferLength);
    buffer.handle()->length = length;
    buffer.handle()->lkey = mDataRegion->lkey;
    return buffer;
}

void BufferManager::releaseBuffer(uint64_t id) {
    mBufferQueue.push(id);
}

} // namespace infinio
} // namespace crossbow
