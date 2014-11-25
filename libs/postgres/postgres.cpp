#include "postgres.hpp"
#include "postgres_message.h"

#include <boost/cast.hpp>
#if defined( __APPLE__ )

    #include <CommonCrypto/CommonDigest.h>

    #ifdef MD5_DIGEST_LENGTH

        #undef MD5_DIGEST_LENGTH

    #endif

    #define MD5_Init            CC_MD5_Init
    #define MD5_Update          CC_MD5_Update
    #define MD5_Final           CC_MD5_Final
    #define MD5_DIGEST_LENGTH   CC_MD5_DIGEST_LENGTH
    #define MD5_CTX             CC_MD5_CTX

#else

    #include <openssl/md5.h>

#endif

namespace crossbow {

namespace postgres {

namespace error {

postgres_category category;

postgres_error::postgres_error(const impl::error_response* r)
    : fields_(r->fields())
{
}

} // namespace error

using namespace impl;
using namespace boost;
using namespace boost::asio;
using namespace boost::system;

void connection::async_connect(const std::function<void (const boost::system::error_code &)> &callback) {
    boost::asio::ip::tcp::resolver::query q(host.c_str(), port.c_str());
    resolver.async_resolve(q,
            [this, callback](const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator begin)
    {
        if (error) {
            callback(error);
            return;
        }
        boost::asio::async_connect(socket, begin, [this, callback](const boost::system::error_code& error, decltype(begin) iter){
            if (error) {
                callback(error);
                return;
            }
            impl::startup_message msg;
            msg["user"] = username;
            msg["database"] = db;
            msg.send([this, callback](const error_code& e){
                if (e) {
                    callback(e);
                    return;
                }
                auth(callback);
            }, *this);
        });
    });
}

void connection::auth(const std::function<void (const error_code&)>& callback) {
    postgres_message::read_message([this, callback](const error_code& e, postgres_message::ptr ptr) {
        if (e) {
            return callback(e);
        }
        switch (ptr->type()) {
        case MessageType::Error:
            current_error_.reset(new error::postgres_error(polymorphic_downcast<impl::error_response*>(ptr.release())));
            break;
        case MessageType::Authentication:
        {
            impl::authentication_message* msg = polymorphic_downcast<impl::authentication_message*>(ptr.get());
            switch (msg->info()) {
            case impl::authentication_message::OK:
                bystand(callback);
                break;
            case impl::authentication_message::CLEARTEXT:
                cleartext_auth(callback);
                break;
            case impl::authentication_message::MD5:
                md5_auth(callback, msg->data());
                break;
            default:
                callback(error_code(error::postgres_category::AUTH_NOT_SUPPORTED, error::category));
            }
        }
            break;
        default:
            assert(false);
        }
    }, *this);
}

void connection::bystand(const std::function<void (const error_code&)>& callback)
{
    postgres_message::read_message([this, callback](const error_code& e, postgres_message::ptr ptr) {
        if (e) {
            callback(e);
        }
        switch (ptr->type()) {
        case MessageType::Error:
            current_error_.reset(new error::postgres_error(polymorphic_downcast<impl::error_response*>(ptr.release())));
            callback(error_code(error::postgres_category::POSTGRESQL_ERROR, error::category));
            return;
        case MessageType::BackendKeyData:
            break;
        default:
            callback(error_code(error::postgres_category::AUTH_NOT_SUPPORTED, error::category));
            return;
        }
        bystand(callback);
    }, *this);
}

void connection::cleartext_auth(const std::function<void (const error_code&)>& callback) {
    password_message msg(password);
    msg.send([this, callback](const error_code& ec){
        if (ec) {
            callback(ec);
            return;
        }
        auth(callback);
    }, *this);
}

namespace {
void bytesToHex(const unsigned char* ptr, size_t length, std::vector<char>& result, int offset) {
    std::vector<char> lookup = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                'a', 'b', 'c', 'd', 'e', 'f'};
    int i, c, j, pos = offset;

    for (i = 0; i < length; i++)
    {
        c = ptr[i] & 0xFF;
        j = c >> 4;
        result[pos++] = lookup[j];
        j = (c & 0xF);
        result[pos++] = lookup[j];
    }
}
}

void connection::md5_auth(const std::function<void (const error_code&)>& callback, const std::vector<char>& salt) {
    assert(salt.size() == 4);
    unsigned char md[MD5_DIGEST_LENGTH];
    std::vector<char> result(MD5_DIGEST_LENGTH*2 + 3);
    {
        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, password.data(), password.size());
        MD5_Update(&ctx, username.data(), username.size());
        MD5_Final(md, &ctx);
        bytesToHex(md, MD5_DIGEST_LENGTH, result, 0);
    }
    {
        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, result.data(), result.size());
        MD5_Update(&ctx, salt.data(), salt.size());
        MD5_Final(md, &ctx);
        bytesToHex(md, MD5_DIGEST_LENGTH, result, 3);
    }
    result[0] = 'm';
    result[1] = 'd';
    result[2] = '5';
    crossbow::string pass(result.data(), result.size());
    password_message msg(pass);
    msg.send([this, callback](const error_code& e){
        if (e) {
            callback(e);
            return;
        }
        auth(callback);
    }, *this);
}

} // namespace postgres

} // namespace crossbow
