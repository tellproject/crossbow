#pragma once

#include <cstdint>

namespace crossbow {
namespace infinio {

/**
 * @brief The InfinibandLimits struct containing configurable limits for the Infiniband devices
 */
struct InfinibandLimits {
public:
    InfinibandLimits()
            : bufferLength(1024),
              bufferCount(8),
              receiveQueueLength(4),
              sendQueueLength(10),
              completionQueueLength(10),
              pollCycles(1000000) {
    }

    /**
     * @brief Number of buffers to be allocated
     */
    uint64_t bufferCount;

    /**
     * @brief Maximum size of any buffer
     */
    uint32_t bufferLength;

    /**
     * @brief Number of buffers to post on the shared receive queue
     */
    uint32_t receiveQueueLength;

    /**
     * @brief Size of the send queue to allocate for each connection
     */
    uint32_t sendQueueLength;

    /**
     * @brief Size of the completion queue to allocate for each completion context
     */
    uint32_t completionQueueLength;

    /**
     * @brief Number of iterations to poll when there is no work completion
     */
    uint64_t pollCycles = 1000000;
};

} // namespace infinio
} // namespace crossbow
