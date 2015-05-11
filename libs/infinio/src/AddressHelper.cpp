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
