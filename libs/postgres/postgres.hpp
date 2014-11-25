#pragma once

#include "postgres_message.h"

#include <functional>
#include <cstdint>

#include <crossbow/string.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

namespace crossbow {
namespace postgres {

namespace impl {

class error_response;

} // namespace impl

namespace error {

class postgres_category : public boost::system::error_category {
    std::vector<std::string> msgs;
public:
    static constexpr int AUTH_NOT_SUPPORTED = 1;
    static constexpr int UNEXPECTED_RESPONSE = 2;
    static constexpr int POSTGRESQL_ERROR = 3;
    static constexpr int SEND_NOT_POSSIBLE = 4;
    static constexpr int UNKNOWN_MESSAGE = 5;
    postgres_category() {
        msgs.push_back("Authentication method not supported");
        msgs.push_back("Unexpected Response");
        msgs.push_back("Received an error from postgresql - check the error object in the connection object");
        msgs.push_back("This message type can not be sent");
        msgs.push_back("Received an unknown message from server");
    }

    const char* name() const noexcept override {
        return "postgres";
    }
    std::string message(int ev) const noexcept override {
        return msgs[ev - 1];
    }
};

class postgres_error {
    std::unordered_map<crossbow::string, crossbow::string> fields_;
public:
    postgres_error(const impl::error_response* r);
    boost::optional<crossbow::string> operator [](const crossbow::string& key) const {
        auto iter = fields_.find(key);
        if (iter != fields_.end()) {
            return iter->second;
        }
        return boost::none;
    }
    const decltype(fields_)& fields() const {
        return fields_;
    }
};

extern postgres_category category;

} // namespace error

class connection {
    friend class impl::postgres_message;
    boost::asio::io_service& service;
    boost::asio::ip::tcp::socket socket;
    boost::asio::ip::tcp::resolver resolver;
    crossbow::string host;
    crossbow::string port;
    crossbow::string db;
    crossbow::string username;
    crossbow::string password;
    std::unique_ptr<std::vector<char>> curr_buf;
    std::unique_ptr<error::postgres_error> current_error_;
public:
    connection(boost::asio::io_service& service,
               const crossbow::string& host,
               const crossbow::string& port,
               const crossbow::string& db,
               const crossbow::string& username,
               const crossbow::string& password)
            : service(service),
              socket(service),
              resolver(service),
              host(host),
              port(port),
              db(db),
              username(username),
              password(password)
    {
    }

    void async_connect(const std::function<void(const boost::system::error_code&)>& callback);
private: // helpers to establish connection
    void auth(const std::function<void(const boost::system::error_code&)>& callback);
    void bystand(const std::function<void(const boost::system::error_code&)>& callback);
    void cleartext_auth(const std::function<void(const boost::system::error_code&)>& callback);
    void md5_auth(const std::function<void(const boost::system::error_code&)>& callback, const std::vector<char>& salt);
};

} // namespace postgres
} // namespace crossbow
