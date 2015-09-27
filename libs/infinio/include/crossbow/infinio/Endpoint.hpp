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

#include <crossbow/string.hpp>

#include <cstdint>
#include <iosfwd>

#include <netinet/in.h>

namespace crossbow {
namespace infinio {

/**
 * @brief The Endpoint class encapsulates network addresses
 */
class Endpoint {
public:
    /**
     * @brief The IPv4 protocol family
     */
    static int ipv4() {
        return AF_INET;
    }

    /**
     * @brief The IPv6 protocol family
     */
    static int ipv6() {
        return AF_INET6;
    }

    /**
     * @brief Creates a new empty endpoint
     */
    Endpoint();

    /**
     * @brief Creates a new endpoint of the given protocol family, host and port
     *
     * @param family The protocol family
     * @param host IP Address and port of the target address in the format IP:Port
     */
    Endpoint(int family, const crossbow::string& host);

    /**
     * @brief Creates a new endpoint of the given protocol family, host and port
     *
     * @param family The protocol family
     * @param host IP Address of the target address
     * @param port Port number of the target address
     */
    Endpoint(int family, const crossbow::string& host, uint16_t port);

    /**
     * @brief Creates a new endpoint of the given protocol family and port
     *
     * @param family The protocol family
     * @param port Port number of the target address
     */
    Endpoint(int family, uint16_t port);

    /**
     * @brief Creates a new endpoint from the given sockaddr struct
     */
    Endpoint(struct sockaddr* addr);

    struct sockaddr* handle() {
        return reinterpret_cast<struct sockaddr*>(&mAddress);
    }

private:
    friend std::ostream& operator<<(std::ostream&, const Endpoint&);

    void setAddress(int family, const crossbow::string& host, uint16_t port);

    union {
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } mAddress;
};

std::ostream& operator<<(std::ostream& out, const Endpoint& rhs);

} // namespace infinio
} // namespace crossbow
