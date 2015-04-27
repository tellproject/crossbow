#pragma once

#include <boost/system/error_code.hpp>

#include <cstdint>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {
namespace error {

/**
 * @brief Basic errors that can happen everywhere
 */
enum basic_errors {
    already_initialized,
};

/**
 * @brief Category for basic errors
 */
class basic_category : public boost::system::error_category {
public:
    const char* name() const noexcept {
        return "infinio.basic";
    }

    std::string message(int value) const {
        if (value == error::already_initialized)
            return "Already initialized";
        return "infinio.basic error";
    }
};

inline const boost::system::error_category& get_basic_category() {
    static basic_category instance;
    return instance;
}

inline boost::system::error_code make_error_code(basic_errors e) {
    return boost::system::error_code(static_cast<int>(e), get_basic_category());
}

/**
 * @brief Network errors related to actions on sockets
 */
enum network_errors {
    /// Already open.
    already_open = 1,

    /// Bad socket ID.
    bad_descriptor,

    /// Address resolution failed.
    address_resolution,

    /// Route resolution failed.
    route_resolution,

    /// Remote unreachable.
    unreachable,

    /// Connection error.
    connection_error,

    /// Connection rejected.
    connection_rejected,

    /// Connection disconnected
    disconnected,
};

/**
 * @brief Category for network errors
 */
class network_category : public boost::system::error_category {
public:
    const char* name() const noexcept {
        return "infinio.network";
    }

    std::string message(int value) const {
        switch (value) {
        case error::already_open:
            return "Already open";

        case error::bad_descriptor:
            return "Bad socket ID";

        case error::address_resolution:
            return "Address resolution failed";

        case error::route_resolution:
            return "Route resolution failed";

        case error::connection_error:
            return "Connection error";

        case error::connection_rejected:
            return "Connection rejected";

        case error::disconnected:
            return "Connection disconnected";

        default:
            return "infinio.network error";
        }
    }
};

inline const boost::system::error_category& get_network_category() {
    static network_category instance;
    return instance;
}

inline boost::system::error_code make_error_code(network_errors e) {
    return boost::system::error_code(static_cast<int>(e), get_network_category());
}

/**
 * @brief Category ibverbs ibv_wc_status error codes
 */
class work_completion_category : public boost::system::error_category {
public:
    const char* name() const noexcept {
        return "infinio.wc";
    }

    std::string message(int value) const {
        return ibv_wc_status_str(static_cast<ibv_wc_status>(value));
    }
};

inline const boost::system::error_category& get_work_completion_category() {
    static work_completion_category instance;
    return instance;
}

inline boost::system::error_code make_error_code(ibv_wc_status e) {
    return boost::system::error_code(static_cast<int>(e), get_work_completion_category());
}

} // namespace error
} // namespace infinio
} // namespace crossbow

namespace boost {
namespace system {

template<>
struct is_error_code_enum<crossbow::infinio::error::basic_errors> {
    static const bool value = true;
};

template<>
struct is_error_code_enum<crossbow::infinio::error::network_errors> {
    static const bool value = true;
};

template<>
struct is_error_code_enum<ibv_wc_status> {
    static const bool value = true;
};

} // namespace system
} // namespace boost
