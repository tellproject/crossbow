#include <crossbow/infinio/Endpoint.hpp>

#include "AddressHelper.hpp"

#include <cstring>

#include <arpa/inet.h>

namespace crossbow {
namespace infinio {

Endpoint::Endpoint(int family, const crossbow::string& host, uint16_t port) {
    switch (family) {
    case AF_INET: {
        memset(&mAddress.ipv4, 0, sizeof(mAddress.ipv4));
        mAddress.ipv4.sin_family = AF_INET;
        inet_pton(AF_INET, host.c_str(), &mAddress.ipv4.sin_addr);
        mAddress.ipv4.sin_port = htons(port);
    } break;
    case AF_INET6: {
        memset(&mAddress.ipv6, 0, sizeof(mAddress.ipv6));
        mAddress.ipv6.sin6_family = AF_INET6;
        inet_pton(AF_INET6, host.c_str(), &mAddress.ipv6.sin6_addr);
        mAddress.ipv6.sin6_port = htons(port);
    } break;
    default:
        break;
    }
}

Endpoint::Endpoint(int family, uint16_t port) {
    switch (family) {
    case AF_INET: {
        memset(&mAddress.ipv4, 0, sizeof(mAddress.ipv4));
        mAddress.ipv4.sin_family = AF_INET;
        mAddress.ipv4.sin_port = htons(port);
    } break;
    case AF_INET6: {
        memset(&mAddress.ipv6, 0, sizeof(mAddress.ipv6));
        mAddress.ipv6.sin6_family = AF_INET6;
        mAddress.ipv6.sin6_port = htons(port);
    } break;
    default:
        break;
    }
}

Endpoint::Endpoint(sockaddr* addr) {
    switch (addr->sa_family) {
    case AF_INET: {
        memcpy(&mAddress.ipv4, addr, sizeof(struct sockaddr_in));
    } break;
    case AF_INET6: {
        memcpy(&mAddress.ipv6, addr, sizeof(struct sockaddr_in6));
    } break;
    default:
        break;
    }
}

std::ostream& operator<<(std::ostream& out, const Endpoint& rhs) {
    printAddress(out, reinterpret_cast<const struct sockaddr*>(&rhs.mAddress));
    return out;
}

} // namespace infinio
} // namespace crossbow
