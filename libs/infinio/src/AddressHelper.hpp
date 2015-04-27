#pragma once

#include <rdma/rdma_cma.h>

#include <string>

namespace crossbow {
namespace infinio {

/**
 * @brief Returns the address in a human readable format
 */
std::string formatAddress(struct sockaddr* addr);

/**
 * @brief Returns the address of the remote host in a human readable format
 */
inline std::string formatRemoteAddress(struct rdma_cm_id* id) {
    return formatAddress(rdma_get_peer_addr(id));
}

} // namespace infinio
} // namespace crossbow
