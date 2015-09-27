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
#include "AddressHelper.hpp"

#include <sstream>

#include <arpa/inet.h>

namespace crossbow {
namespace infinio {

void printAddress(std::ostream& out, const struct sockaddr* addr) {
    switch (addr->sa_family) {
    case AF_INET: {
        auto addr4 = reinterpret_cast<const struct sockaddr_in*>(addr);
        char host4[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr4->sin_addr, host4, INET_ADDRSTRLEN);
        out << host4 << ":" << ntohs(addr4->sin_port);
    } break;

    case AF_INET6: {
        auto addr6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
        char host6[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr6->sin6_addr, host6, INET6_ADDRSTRLEN);
        out << host6 << ":" << ntohs(addr6->sin6_port);
    } break;

    default:
        break;
    }
}

std::string formatAddress(const struct sockaddr* addr) {
    std::stringstream ss;
    printAddress(ss, addr);
    return ss.str();
}

} // namespace infinio
} // namespace crossbow
