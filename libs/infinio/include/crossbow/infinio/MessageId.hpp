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
 * @brief The MessageId class encodes various information into a single RPC 64 bit message ID
 *
 * The upper 32 bits contain a user supplied ID, the lower 32 bits encode whether the message is asynchronous.
 */
class MessageId {
public:
    MessageId()
            : mId(0u) {
    }

    MessageId(uint64_t id)
            : mId(id) {
    }

    MessageId(uint32_t userId, bool async)
            : mId(buildId(userId, async)) {
    }

    /**
     * @brief The RPC message ID
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
     * @brief Whether the message is sent asynchronous
     */
    bool isAsync() const {
        return (static_cast<uint32_t>(mId & 0xFFFFFFFFu) != 0x0u);
    }

private:
    static uint64_t buildId(uint32_t userId, bool async) {
        return ( (static_cast<uint64_t>(userId) << 32)
               | (static_cast<uint64_t>(async ? 0x1u : 0x0u)));
    }

    /// The RPC message ID
    uint64_t mId;
};

} // namespace infinio
} // namespace crossbow
