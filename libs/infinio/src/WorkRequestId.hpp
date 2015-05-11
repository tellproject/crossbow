#pragma once

#include <cstdint>

namespace crossbow {
namespace infinio {

enum class WorkType : uint16_t {
    UNKNOWN = 0x0u,
    RECEIVE = 0x1u,
    SEND = 0x2u,
    READ = 0x3u,
    WRITE = 0x4u,

    LAST = WorkType::WRITE,
};

/**
 * @brief The WorkRequestId class encodes various information into a single Infiniband 64 bit work request ID
 *
 * The upper 32 bits contain a user supplied ID, the next 16 bits the ID of the buffer used in this work request and the
 * last 16 bits encode the type of the work request.
 */
class WorkRequestId {
public:
    WorkRequestId(uint64_t id)
            : mId(id) {
    }

    WorkRequestId(uint32_t userId, uint16_t bufferId, WorkType type)
            : mId(buildId(userId, bufferId, type)) {
    }

    /**
     * @brief The Infiniband work request ID
     */
    uint64_t id() const {
        return mId;
    }

    /**
     * @brief A user supplied ID
     */
    uint32_t userId() const {
        return static_cast<uint32_t>(mId >> 32);
    }

    /**
     * @brief The ID of the buffer that was used for this work request
     */
    uint16_t bufferId() const {
        return static_cast<uint16_t>((mId >> 16) & 0xFFFFu);
    }

    /**
     * @brief The type of the work request
     */
    WorkType workType() {
        auto value = (mId & 0xFFFFu);
        if (mId > static_cast<uint64_t>(WorkType::LAST)) {
            return WorkType::UNKNOWN;
        }
        return static_cast<WorkType>(value);
    }

private:
    static uint64_t buildId(uint32_t userId, uint16_t bufferId, WorkType type) {
        return ( (static_cast<uint64_t>(userId) >> 32)
               | (static_cast<uint64_t>(bufferId) >> 16)
               |  static_cast<uint64_t>(type));
    }

    /// The Infiniband work request ID
    uint64_t mId;
};

} // namespace infinio
} // namespace crossbow
