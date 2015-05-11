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
            : receiveBufferCount(4),
              sendBufferCount(8),
              bufferLength(1024),
              sendQueueLength(10),
              completionQueueLength(10),
              pollCycles(1000000) {
    }

    /**
     * @brief Number of shared receive buffers to allocate
     */
    uint16_t receiveBufferCount;

    /**
     * @brief Number of shared send buffers to allocate
     */
    uint16_t sendBufferCount;

    /**
     * @brief Maximum size of any buffer
     */
    uint32_t bufferLength;

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
