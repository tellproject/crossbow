#pragma once

#include <rdma/rdma_cma.h>

#include <cstddef>
#include <cstring>
#include <limits>

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

} // namespace infinio
} // namespace crossbow
