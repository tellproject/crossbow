#pragma once

#include <cstdint>
#include <system_error>
#include <type_traits>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {
namespace error {

/**
 * @brief RPC errors related to actions on the RPC interface
 */
enum rpc_errors {
    /// No response received
    no_response = 1,

    /// Received message is invalid.
    invalid_message,

    /// Received message of wrong type.
    wrong_type,
};

/**
 * @brief Category for RPC errors
 */
class rpc_category : public std::error_category {
public:
    const char* name() const noexcept {
        return "infinio.rpc";
    }

    std::string message(int value) const {
        switch (value) {
        case error::no_response:
            return "No response received";

        case error::invalid_message:
            return "Received message is invalid";

        case error::wrong_type:
            return "Received message of wrong type";


        default:
            return "infinio.rpc error";
        }
    }
};

inline const std::error_category& get_rpc_category() {
    static rpc_category instance;
    return instance;
}

inline std::error_code make_error_code(rpc_errors e) {
    return std::error_code(static_cast<int>(e), get_rpc_category());
}

/**
 * @brief Network errors related to actions on sockets
 */
enum network_errors {
    /// Already open.
    already_open = 1,

    /// Address resolution failed.
    address_resolution,

    /// Route resolution failed.
    route_resolution,

    /// Remote unreachable.
    unreachable,

    /// Connection error.
    connection_error,

    /// Buffer is invalid.
    invalid_buffer,

    /// Memory access out of range
    out_of_range,
};

/**
 * @brief Category for network errors
 */
class network_category : public std::error_category {
public:
    const char* name() const noexcept {
        return "infinio.network";
    }

    std::string message(int value) const {
        switch (value) {
        case error::already_open:
            return "Already open";

        case error::address_resolution:
            return "Address resolution failed";

        case error::route_resolution:
            return "Route resolution failed";

        case error::unreachable:
            return "Remote unreachable";

        case error::connection_error:
            return "Connection error";

        case error::invalid_buffer:
            return "Buffer is invalid";

        case error::out_of_range:
            return "Memory access out of range";

        default:
            return "infinio.network error";
        }
    }
};

inline const std::error_category& get_network_category() {
    static network_category instance;
    return instance;
}

inline std::error_code make_error_code(network_errors e) {
    return std::error_code(static_cast<int>(e), get_network_category());
}

/**
 * @brief Category ibverbs ibv_wc_status error codes
 */
class work_completion_category : public std::error_category {
public:
    const char* name() const noexcept {
        return "infinio.wc";
    }

    std::string message(int value) const {
        return ibv_wc_status_str(static_cast<ibv_wc_status>(value));
    }
};

inline const std::error_category& get_work_completion_category() {
    static work_completion_category instance;
    return instance;
}

inline std::error_code make_error_code(ibv_wc_status e) {
    return std::error_code(static_cast<int>(e), get_work_completion_category());
}

} // namespace error
} // namespace infinio
} // namespace crossbow

namespace std {

template<>
struct is_error_code_enum<crossbow::infinio::error::rpc_errors> : public std::true_type {
};

template<>
struct is_error_code_enum<crossbow::infinio::error::network_errors> : public std::true_type {
};

template<>
struct is_error_code_enum<ibv_wc_status> : public std::true_type {
};

} // namespace std
