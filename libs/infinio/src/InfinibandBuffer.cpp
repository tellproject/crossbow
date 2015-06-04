#include <crossbow/infinio/InfinibandBuffer.hpp>

#include "Logging.hpp"

#define REGION_LOG(...) INFINIO_LOG("[LocalMemoryRegion] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

void ScatterGatherBuffer::add(const LocalMemoryRegion& region, const void* addr, uint32_t length) {
    // Range and access checks are made when sending the buffer so we skip them here
    struct ibv_sge element;
    element.addr = reinterpret_cast<uintptr_t>(addr);
    element.length = length;
    element.lkey = region.lkey();
    mHandle.push_back(element);
    mLength += length;
}

void ScatterGatherBuffer::add(const InfinibandBuffer& buffer, size_t offset, uint32_t length) {
    if (offset + length > buffer.length()) {
        // TODO Error handling
        return;
    }

    struct ibv_sge element;
    element.addr = reinterpret_cast<uintptr_t>(buffer.data()) + offset;
    element.length = length;
    element.lkey = buffer.lkey();
    mHandle.push_back(element);
    mLength += length;
}

void LocalMemoryRegion::releaseMemoryRegion(std::error_code& ec) {
    if (!mDataRegion) {
        return;
    }

    REGION_LOG("Releasing memory region at %1%", mDataRegion->addr);
    if (auto res = ibv_dereg_mr(mDataRegion)) {
        ec = std::error_code(res, std::system_category());
        return;
    }
    mDataRegion = nullptr;
}

InfinibandBuffer LocalMemoryRegion::acquireBuffer(uint16_t id, size_t offset, uint32_t length) {
    if (offset + length > mDataRegion->length) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }

    InfinibandBuffer buffer(id);
    buffer.handle()->addr = reinterpret_cast<uintptr_t>(mDataRegion->addr) + offset;
    buffer.handle()->length = length;
    buffer.handle()->lkey = mDataRegion->lkey;
    return buffer;
}

} // namespace infinio
} // namespace crossbow
