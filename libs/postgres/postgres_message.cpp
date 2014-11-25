#include "postgres_message.h"
#include "postgres.hpp"

#include <cassert>

#include <boost/lexical_cast.hpp>

namespace {

int16_t bswap16(int16_t v) {
#ifdef LITTLE_ENDIAN
    return __builtin_bswap16(v);
#else
    return v;
#endif
}

int32_t bswap32(int32_t v) {
#ifdef LITTLE_ENDIAN
    return __builtin_bswap32(v);
#else
    return v;
#endif
}

int64_t bswap64(int64_t v) {
#ifdef LITTLE_ENDIAN
    return __builtin_bswap64(v);
#else
    return v;
#endif
}

}

namespace crossbow {
namespace postgres {
namespace impl {

using namespace boost::asio;
using namespace boost::system;
using namespace crossbow::postgres::error;

std::vector<char>& postgres_message::get_buf(connection &c) {
    return *c.curr_buf;
}

ip::tcp::socket& postgres_message::get_socket(connection &c) {
    return c.socket;
}

inline void send_helper(ip::tcp::socket& s, std::vector<char>& buf, int32_t length, const std::function<void (const error_code &)>& callback) {
#ifdef NDEBUG
    async_write(get_socket(c), buffer(buf), [callback](const error_code& ec, size_t bt) {
        callback(ec);
    });
#else
    async_write(s, buffer(buf), [callback, length](const error_code& ec, size_t bt) {
        assert(ec || length == bt);
        callback(ec);
    });
#endif
}

void postgres_message::send(const std::function<void (const error_code &)>& callback, connection& c) const {
    callback(error_code(error::postgres_category::SEND_NOT_POSSIBLE, error::category));
}

namespace {
void read_message_impl(const std::function<void (const postgres_message::error_code&)> callback,
                       connection& c,
                       std::vector<char>& buf,
                       ip::tcp::socket& socket,
                       size_t bt = 0)
{
    int32_t length;
    auto cfun = [callback, &buf, &socket, &c](const postgres_message::error_code& e, size_t bt) {
        if (e) {
            callback(e);
            return;
        }
        read_message_impl(callback, c, buf, socket, bt);
    };
    if (bt < 1 + sizeof(length)) {
        buf.resize(1024);
        async_read(socket, buffer(buf), cfun);
    }
    memcpy(&length, buf.data() + 1, sizeof(length));
    length = bswap32(length);
    if (length + 1 > bt) {
        buf.resize(length);
        async_read(socket, buffer(buf.data() + bt, length - bt), cfun);
        return;
    }
    buf.resize(length);
    callback(error_code());
}
} // anonymous namespace

void postgres_message::read_message(const std::function<void (const postgres_message::error_code&, postgres_message::ptr)> callback, connection &c) {
    std::vector<char>& buf = get_buf(c);
    auto& socket = get_socket(c);
    read_message_impl([callback, &buf, &socket, &c](const error_code& e) {
        if (e) {
            callback(e, nullptr);
            return;
        }
        switch (buf[0]) {
        case 'E':
            error_response::recv(callback, c);
        case 'R':
            authentication_message::recv(callback, c);
            return;
        case 'K':
            backend_key_data::recv(callback, c);
            return;
        case 'S':
            parameter_status::recv(callback, c);
        default:
            callback(error_code(postgres_category::UNKNOWN_MESSAGE, category), nullptr);
        }
    }, c, buf, socket);
}

void startup_message::send(const std::function<void (const error_code &)> &callback, connection &c) const {
    std::vector<char>& buf = get_buf(c);
    uint32_t length = 2*sizeof(int32_t);
    int32_t version = bswap32(PROTOCOL_VERSION);
    for (auto p : parameters) {
        length += p.first.size() + 1;
        length += p.second.size() + 1;
    }
    buf.resize(length);
    //serialize to buffer
    int32_t slength = bswap32(length);
    char* l = reinterpret_cast<char*>(&slength);
    char* v = reinterpret_cast<char*>(&version);
    auto iter = buf.begin();
    iter = std::copy(l, l + sizeof(int32_t), iter);
    iter = std::copy(v, v + sizeof(int32_t), iter);
    for (auto p : parameters) {
        iter = std::copy(p.first.begin(), p.first.end(), iter);
        *iter = '\0';
        ++iter;
        iter = std::copy(p.second.begin(), p.second.end(), iter);
        *iter = '\0';
        ++iter;
    }
    send_helper(get_socket(c), buf, length, callback);
}

void error_response::recv(const std::function<void (const postgres_message::error_code&, postgres_message::ptr)> callback, connection& c)
{
    std::vector<char>& buf = get_buf(c);
    ip::tcp::socket& socket = get_socket(c);
    auto iter = buf.data() + 1;
    error_response* res = new error_response();
    char t = *(iter++);
    while (t != '\0') {
        crossbow::string value(iter);
        iter += value.size() + 1;
        switch (t) {
        case 'S':
            res->fields_.insert(std::make_pair("severity", std::move(value)));
            break;
        case 'C':
            res->fields_.insert(std::make_pair("code", std::move(value)));
            break;
        case 'M':
            res->fields_.insert(std::make_pair("message", std::move(value)));
            break;
        case 'D':
            res->fields_.insert(std::make_pair("detail", std::move(value)));
            break;
        case 'H':
            res->fields_.insert(std::make_pair("hint", std::move(value)));
            break;
        case 'P':
            res->fields_.insert(std::make_pair("position", std::move(value)));
            break;
        case 'p':
            res->fields_.insert(std::make_pair("internal position", std::move(value)));
            break;
        case 'q':
            res->fields_.insert(std::make_pair("internal query", std::move(value)));
            break;
        case 'W':
            res->fields_.insert(std::make_pair("where", std::move(value)));
            break;
        case 's':
            res->fields_.insert(std::make_pair("schema name", std::move(value)));
            break;
        case 't':
            res->fields_.insert(std::make_pair("table name", std::move(value)));
            break;
        case 'c':
            res->fields_.insert(std::make_pair("column name", std::move(value)));
            break;
        case 'd':
            res->fields_.insert(std::make_pair("data type name", std::move(value)));
            break;
        case 'n':
            res->fields_.insert(std::make_pair("constraint name", std::move(value)));
            break;
        case 'F':
            res->fields_.insert(std::make_pair("file", std::move(value)));
            break;
        case 'L':
            res->fields_.insert(std::make_pair("line", std::move(value)));
            break;
        case 'R':
            res->fields_.insert(std::make_pair("routine", std::move(value)));
            break;
        default:
            // ignore unknown fields
            break;
        }
        t = *(++iter);
    }
    callback(error_code(), std::unique_ptr<postgres_message>(res));
}

void authentication_message::recv(const std::function<void (const error_code &, ptr)> callback, connection &c) {
    std::vector<char>& buf = get_buf(c);
    auto iter = buf.begin() + 1;
    int32_t length;
    char* l = reinterpret_cast<char*>(&length);
    std::copy(iter, iter + sizeof(int32_t), l);
    iter += sizeof(int32_t);
    length = bswap32(length);
    int32_t info;
    char* i = reinterpret_cast<char*>(&info);
    std::copy(iter, iter + sizeof(int32_t), i);
    iter += sizeof(int32_t);
    info = bswap32(info);
    if (length > 8) {
        std::vector<char> data(length - 8);
        std::copy(iter, iter + (length - 8), data.begin());
        callback(error_code(), std::unique_ptr<postgres_message>(new authentication_message(info, std::move(data))));
    } else {
        callback(error_code(), std::unique_ptr<postgres_message>(new authentication_message(info)));
    }
}

void password_message::send(const std::function<void (const postgres_message::error_code&)>& callback, connection& c) const
{
    std::vector<char>& buf = get_buf(c);
    auto iter = buf.begin();
    int32_t length = password.size() + 1 + sizeof(int32_t);
    buf.resize(length + 1);
    *(iter++) = 'p';
    length = bswap32(length);
    char* l = reinterpret_cast<char*>(&length);
    iter = std::copy(l, l + sizeof(int32_t), iter);
    iter = std::copy(password.begin(), password.end() + 1, iter);
    async_write(get_socket(c), buffer(buf), [callback](const error_code& e, size_t) {
        callback(e);
    });
}

serializer& serializer::operator()(int8_t i)
{
    buf[offset++] = static_cast<char>(i);
    return *this;
}

serializer& serializer::operator()(int16_t i)
{
    i = bswap16(i);
    memcpy(buf.data() + offset, &i, sizeof(int16_t));
    offset += sizeof(int16_t);
    return *this;
}

serializer& serializer::operator()(int32_t i)
{
    i = bswap32(i);
    memcpy(buf.data() + offset, &i, sizeof(int32_t));
    offset += sizeof(int32_t);
    return *this;
}

serializer& serializer::operator()(int64_t i)
{
    i = bswap64(i);
    memcpy(buf.data() + offset, &i, sizeof(int64_t));
    offset += sizeof(int64_t);
    return *this;
}

void serializer::set_size(int32_t& size, size_t off)
{
    size = bswap32(offset);
    memcpy(buf.data() + off, &size, sizeof(size));
    size = bswap32(size);
    buf.resize(size);
}

deserializer& deserializer::operator()(char& i)
{
    i = buf[offset++];
    return *this;
}

deserializer& deserializer::operator ()(int8_t& i) {
    i = int8_t(buf[offset++]);
    return *this;
}

deserializer& deserializer::operator ()(int16_t& i) {
    memcpy(buf.data() + offset, &i, sizeof(i));
    i = bswap16(i);
    return *this;
}

deserializer& deserializer::operator ()(int32_t& i) {
    memcpy(buf.data() + offset, &i, sizeof(i));
    i = bswap32(i);
    return *this;
}

deserializer& deserializer::operator ()(int64_t& i) {
    memcpy(buf.data() + offset, &i, sizeof(i));
    i = bswap64(i);
    return *this;
}

deserializer& deserializer::operator ()(crossbow::string& str) {
    char* begin = buf.data() + offset;
    char* end = begin;
    while (*(end++));
    str = crossbow::string(begin, end);
    return *this;
}

void backend_key_data::recv(const std::function<void (const postgres_message::error_code&, postgres_message::ptr)>& callback, connection& c)
{
    std::vector<char>& buf = get_buf(c);
    deserializer ser(buf);
    ser += 4;
    backend_key_data* res = new backend_key_data();
    ser(res->process_id_)(res->secret_key_);
    callback(error_code(), std::unique_ptr<postgres_message>(res));
}

void parameter_status::recv(const std::function<void (const postgres_message::error_code&, postgres_message::ptr)> callback, connection& c)
{
    std::vector<char>& buf = get_buf(c);
    parameter_status* res = new parameter_status();
    deserializer ser(buf);
    while (ser) {
        crossbow::string k;
        crossbow::string v;
        ser(k)(v);
        res->msg_.insert(std::make_pair(k, v));
    }
    callback(error_code(), postgres_message::ptr(res));
}

} // namespace impl
} // namespace postgres
} // namespace crossbow
