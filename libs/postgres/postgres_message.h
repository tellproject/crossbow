#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include <crossbow/string.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/optional.hpp>

namespace crossbow {
namespace postgres {


class connection;

namespace impl {


class serializer {
    std::vector<char>& buf;
    size_t offset = 1u;
public:
    serializer(std::vector<char>& buf, char mId)
        : buf(buf)
    {
        buf[0] = mId;
    }
    serializer& operator() (int8_t i);
    serializer& operator() (int16_t i);
    serializer& operator() (int32_t i);
    serializer& operator() (int64_t i);
    template<class Iter>
    serializer& operator() (Iter begin, Iter end) {
        auto d = std::distance(begin, end);
        if (offset + d >= buf.size()) {
            buf.resize(offset + d);
        }
        std::copy(begin, end, buf.begin() + offset);
        offset += d;
        buf[offset++] = '\0';
        return *this;
    }
    serializer& operator() (const std::string& str) {
        return (*this)(str.begin(), str.end());
    }
    serializer& operator() (const crossbow::string& str) {
        return (*this)(str.begin(), str.end());
    }

    void set_size(int32_t& size, size_t offset);
};

class deserializer {
    std::vector<char>& buf;
    size_t offset = 1u;
public:
    deserializer(std::vector<char>& buf)
        : buf(buf)
    {}
    operator bool() const {
        return offset < buf.size();
    }
    deserializer& operator+= (size_t of) {
        offset += of;
        return *this;
    }
    deserializer& operator++() {
        ++offset;
        return *this;
    }
    deserializer& operator() (char& i);
    deserializer& operator() (crossbow::string& str);
    deserializer& operator() (int8_t& i);
    deserializer& operator() (int16_t& i);
    deserializer& operator() (int32_t& i);
    deserializer& operator() (int64_t& i);
};

constexpr int32_t PROTOCOL_VERSION = 196608;

enum class MessageType {
    Startup,
    Authentication,
    Error,
    Password,
    BackendKeyData,
    ParameterStatus
};

class postgres_message
{
    MessageType type_;
public: // types
    using ptr = std::unique_ptr<postgres_message>;
    using error_code = boost::system::error_code;
protected:
    static std::vector<char>& get_buf(connection& c);
    static boost::asio::ip::tcp::socket& get_socket(connection& c);
public:
    postgres_message(MessageType type) : type_(type) {}
    MessageType type() const {
        return type_;
    }
    static void read_message(const std::function<void(const error_code&, ptr)> callback, connection& c);
    virtual void send(const std::function<void(const error_code&)>&, connection& c) const;
};

class startup_message : public postgres_message {
    std::unordered_map<crossbow::string, crossbow::string> parameters;
public:
    startup_message() : postgres_message(MessageType::Startup) {}
    template<typename K>
    crossbow::string& operator[] (K&& key) {
        return parameters[std::forward<K>(key)];
    }
    void send(const std::function<void (const error_code &)> &, connection &c) const override;
};

class parameter_status : public postgres_message {
    parameter_status() : postgres_message(MessageType::ParameterStatus)
    {}
    std::unordered_map<crossbow::string, crossbow::string> msg_;
public:
    const std::unordered_map<crossbow::string, crossbow::string> msg() const {
        return msg_;
    }
    static void recv(const std::function<void(const error_code&, ptr)> callback, connection& c);
};

class error_response : public postgres_message {
    std::unordered_map<crossbow::string, crossbow::string> fields_;
    error_response() : postgres_message(MessageType::Error) {}
public: // Static
    static void recv(const std::function<void(const error_code&, ptr)> callback, connection& c);
public: // Getters
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

class password_message : public postgres_message {
    crossbow::string password;
public:
    password_message(const crossbow::string& password)
        : postgres_message(MessageType::Password),
          password(password) {}
    void send(const std::function<void (const error_code &)> &, connection &c) const override;
};

class authentication_message : public postgres_message {
    int32_t info_;
    std::vector<char> data_;
public:
    static void recv(const std::function<void(const error_code&, ptr)> callback, connection& c);
public: // constants
    static constexpr int32_t OK = 0;
    static constexpr int32_t KERBEROS = 2;
    static constexpr int32_t CLEARTEXT = 3;
    static constexpr int32_t MD5 = 5;
    static constexpr int32_t SCM = 6;
    static constexpr int32_t GSS = 7;
    static constexpr int32_t GSSCONTINUE = 8;
    static constexpr int32_t SSPI = 9;
public:
    authentication_message(int info)
        : postgres_message(MessageType::Authentication),
          info_(info)
    {}
    template<class V>
    authentication_message(int info, V&& data)
        : postgres_message(MessageType::Authentication),
          info_(info),
          data_(std::forward<V>(data))
    {}
    int32_t info() const {
        return info_;
    }
    const std::vector<char>& data() const {
        return data_;
    }
};

class backend_key_data : public postgres_message {
    int32_t process_id_;
    int32_t secret_key_;
    backend_key_data()
        : postgres_message(MessageType::BackendKeyData)
    {}
public:
    static void recv(const std::function<void(const error_code&, ptr)>& callback, connection& c);
    int32_t process_id() const { return process_id_; }
    int32_t secret_key() const { return secret_key_; }
};

} // namespace impl
} // namespace postgres
} // namespace crossbow
