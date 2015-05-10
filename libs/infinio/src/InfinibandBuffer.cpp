#include <crossbow/infinio/InfinibandBuffer.hpp>

#include "Logging.hpp"

#define REGION_LOG(...) INFINIO_LOG("[LocalMemoryRegion] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

void LocalMemoryRegion::releaseMemoryRegion(boost::system::error_code& ec) {
    if (!mDataRegion) {
        return;
    }

    REGION_LOG("Releasing memory region at %1%", mDataRegion->addr);
    if (auto res = ibv_dereg_mr(mDataRegion) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }
    mDataRegion = nullptr;
}

InfinibandBuffer LocalMemoryRegion::acquireBuffer(uint16_t id, uint64_t offset, uint32_t length) {
    if (offset + length > mDataRegion->length) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);;
    }

    InfinibandBuffer buffer(id);
    buffer.handle()->addr = reinterpret_cast<uintptr_t>(mDataRegion->addr) + offset;
    buffer.handle()->length = length;
    buffer.handle()->lkey = mDataRegion->lkey;
    return buffer;
}

} // namespace infinio
} // namespace crossbow
