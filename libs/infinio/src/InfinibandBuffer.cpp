#include <crossbow/infinio/InfinibandBuffer.hpp>

#include "DeviceContext.hpp"

#include <crossbow/logger.hpp>

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

LocalMemoryRegion::LocalMemoryRegion(const ProtectionDomain& domain, void* data, size_t length, int access)
        : mDataRegion(ibv_reg_mr(domain.get(), data, length, access)) {
    if (mDataRegion == nullptr) {
        throw std::system_error(errno, std::system_category());
    }
    LOG_TRACE("Created memory region at %1%", data);
}

LocalMemoryRegion::LocalMemoryRegion(const ProtectionDomain& domain, MmapRegion& region, int access)
        : LocalMemoryRegion(domain, region.data(), region.length(), access) {
}

LocalMemoryRegion::~LocalMemoryRegion() {
    if (mDataRegion != nullptr && ibv_dereg_mr(mDataRegion)) {
        std::error_code ec(errno, std::system_category());
        LOG_ERROR("Failed to deregister memory region [error = %1% %2%]", ec, ec.message());
    }
}

LocalMemoryRegion& LocalMemoryRegion::operator=(LocalMemoryRegion&& other) {
    if (mDataRegion != nullptr && ibv_dereg_mr(mDataRegion)) {
        throw std::system_error(errno, std::system_category());
    }

    mDataRegion = other.mDataRegion;
    other.mDataRegion = nullptr;
    return *this;
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
