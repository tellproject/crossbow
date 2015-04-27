#include "AddressHelper.hpp"

#include <sstream>

#include <arpa/inet.h>

namespace crossbow {
namespace infinio {

std::string formatAddress(sockaddr* addr) {
    std::stringstream ss;
    switch (addr->sa_family) {
    case AF_INET: {
        auto addr4 = reinterpret_cast<struct sockaddr_in*>(addr);
        char host4[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr4->sin_addr, host4, INET_ADDRSTRLEN);
        ss << host4 << ":" << ntohs(addr4->sin_port);
    } break;

    case AF_INET6: {
        auto addr6 = reinterpret_cast<struct sockaddr_in6*>(addr);
        char host6[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr6->sin6_addr, host6, INET6_ADDRSTRLEN);
        ss << host6 << ":" << ntohs(addr6->sin6_port);
    } break;

    default:
        break;
    }
    return ss.str();
}

} // namespace infinio
} // namespace crossbow
