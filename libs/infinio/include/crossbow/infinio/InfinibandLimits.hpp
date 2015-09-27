/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
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
              fiberCacheSize(50) {
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
     * @brief Maximum size of the recycled fiber cache for each event processor
     */
    size_t fiberCacheSize;
};

} // namespace infinio
} // namespace crossbow
