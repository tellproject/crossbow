#pragma once

#include <atomic>
#include <cstdint>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class DeviceContext;
class InfinibandSocketHandler;

/**
 * @brief State of a socket
 */
enum class ConnectionState {
    OPEN,
    CLOSED,
    LISTENING,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    DRAINING,
    DISCONNECTED,
};

/**
 * @brief The SocketImplementation struct contains all information associated with a specific socket
 */
struct SocketImplementation {
    SocketImplementation()
            : id(nullptr),
              device(nullptr),
              handler(nullptr),
              work(0),
              state(ConnectionState::CLOSED) {
    }

    SocketImplementation(struct rdma_cm_id* id, DeviceContext* device)
            : id(id),
              device(device),
              handler(nullptr),
              work(0),
              state(ConnectionState::CONNECTING) {
    }

    /// RDMA ID of this connection
    struct rdma_cm_id* id;

    /// The device context of the underlying NIC
    DeviceContext* device;

    /// Callback handlers for events occuring on this socket
    InfinibandSocketHandler* handler;

    /// Number of handlers pending their execution
    std::atomic<uint64_t> work;

    /// State of the socket
    std::atomic<ConnectionState> state;
};

} // namespace infinio
} // namespace crossbow
