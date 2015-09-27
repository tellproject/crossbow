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

#include <rdma/rdma_cma.h>

#include <iosfwd>
#include <string>

namespace crossbow {
namespace infinio {

/**
 * @brief Returns the address in a human readable format
 */
void printAddress(std::ostream& out, const struct sockaddr* addr);

/**
 * @brief Returns the address in a human readable format
 */
std::string formatAddress(const struct sockaddr* addr);

/**
 * @brief Returns the address of the remote host in a human readable format
 */
inline std::string formatRemoteAddress(struct rdma_cm_id* id) {
    return formatAddress(rdma_get_peer_addr(id));
}

} // namespace infinio
} // namespace crossbow
