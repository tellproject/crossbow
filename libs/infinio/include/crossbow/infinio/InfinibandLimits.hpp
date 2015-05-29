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
            : receiveBufferCount(64),
              sendBufferCount(64),
              bufferLength(256),
              sendQueueLength(64),
              maxScatterGather(1),
              completionQueueLength(128),
              pollCycles(1000000),
              contextThreads(2) {
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
     * @brief Maximum supported number of scatter gather elements
     */
    uint32_t maxScatterGather;

    /**
     * @brief Size of the completion queue to allocate for each completion context
     */
    uint32_t completionQueueLength;

    /**
     * @brief Number of iterations without action to poll until going to epoll sleep
     */
    uint64_t pollCycles;

    /**
     * @brief Number of poll threads each with their own completion context
     */
    uint64_t contextThreads;
};

} // namespace infinio
} // namespace crossbow
