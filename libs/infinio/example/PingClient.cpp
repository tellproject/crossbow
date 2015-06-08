#include <crossbow/infinio/Endpoint.hpp>
#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>
#include <crossbow/program_options.hpp>
#include <crossbow/string.hpp>

#include <chrono>
#include <iostream>
#include <system_error>

using namespace crossbow::infinio;

class PingConnection: private InfinibandSocketHandler {
public:
    PingConnection(InfinibandService& service, uint64_t maxSend)
            : mService(service),
              mSocket(service.createSocket()),
              mMaxSend(maxSend),
              mSend(0) {
    }

    void open(const crossbow::string& server, uint16_t port);

private:
    virtual void onConnected(const crossbow::string& data, const std::error_code& ec) override;

    virtual void onReceive(const void* buffer, size_t length, const std::error_code& ec) override;

    virtual void onDisconnect() override;

    virtual void onDisconnected() override;

    void handleError(std::string message, const std::error_code& ec);

    void sendMessage();

    InfinibandService& mService;

    InfinibandSocket mSocket;

    uint64_t mMaxSend;
    uint64_t mSend;
};

void PingConnection::open(const crossbow::string& server, uint16_t port) {
    // Open socket
    Endpoint ep(Endpoint::ipv4(), server, port);
    mSocket->open();
    mSocket->setHandler(this);
    mSocket->connect(ep, "PingClient");
    std::cout << "Connecting to server" << std::endl;
}

void PingConnection::onConnected(const crossbow::string& data, const std::error_code& ec) {
    if (ec) {
        std::cout << "Connect failed " << ec << " - " << ec.message() << std::endl;
        return;
    }
    std::cout << "Connected [data = \"" << data << "\"]" << std::endl;

    sendMessage();
}

void PingConnection::onReceive(const void* buffer, size_t length, const std::error_code& /* ec */) {
    // Calculate RTT
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto start = std::chrono::nanoseconds(*reinterpret_cast<const uint64_t*>(buffer));
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now) - start;
    std::cout << "RTT is " << duration.count() << "ns" << std::endl;

    if (mSend < mMaxSend) {
        sendMessage();
    } else {
        std::cout << "Disconnecting after " << mSend << " pings" << std::endl;
        try {
            mSocket->disconnect();
        } catch (std::system_error& e) {
            std::cout << "Disconnect failed " << e.code() << " - " << e.what() << std::endl;
        }
    }
}

void PingConnection::onDisconnect() {
    std::cout << "Disconnect" << std::endl;
    try {
        mSocket->disconnect();
    } catch (std::system_error& e) {
        std::cout << "Disconnect failed " << e.code() << " - " << e.what() << std::endl;
    }
}

void PingConnection::onDisconnected() {
    std::cout << "Disconnected" << std::endl;
    try {
        mSocket->close();
    } catch (std::system_error& e) {
        std::cout << "Close failed " << e.code() << " - " << e.what() << std::endl;
    }

    std::cout << "Stopping ping client" << std::endl;
    mService.shutdown();
}

void PingConnection::handleError(std::string message, const std::error_code& ec) {
    std::cout << message << " [" << ec << " - " << ec.message() << "]" << std::endl;
    std::cout << "Disconnecting after error" << std::endl;
    try {
        mSocket->disconnect();
    } catch (std::system_error& e) {
        std::cout << "Disconnect failed " << e.code() << " - " << e.what() << std::endl;
    }
}

void PingConnection::sendMessage() {
    // Acquire buffer
    auto sbuffer = mSocket->acquireSendBuffer(8);
    if (sbuffer.id() == InfinibandBuffer::INVALID_ID) {
        handleError("Error acquiring buffer", error::invalid_buffer);
        return;
    }

    // Increment send counter
    ++mSend;

    // Write time since epoch in nano seconds to buffer
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    *reinterpret_cast<uint64_t*>(sbuffer.data()) = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

    // Send message to server
    std::error_code ec;
    mSocket->send(sbuffer, 0x0u, ec);
    if (ec) {
        handleError("Send failed", ec);
    }
}

int main(int argc, const char** argv) {
    bool help = false;
    int count = 5;
    crossbow::string server;
    uint16_t port = 4488;
    auto opts = crossbow::program_options::create_options(argv[0],
            crossbow::program_options::value<'h'>("help", &help),
            crossbow::program_options::value<'c'>("count", &count),
            crossbow::program_options::value<'s'>("server", &server),
            crossbow::program_options::value<'p'>("port", &port));

    try {
        crossbow::program_options::parse(opts, argc, argv);
    } catch (crossbow::program_options::argument_not_found e) {
        std::cerr << e.what() << std::endl << std::endl;
        crossbow::program_options::print_help(std::cout, opts);
        return 1;
    }

    if (help) {
        crossbow::program_options::print_help(std::cout, opts);
        return 0;
    }

    std::cout << "Starting ping client" << std::endl;
    InfinibandService service;
    PingConnection con(service, count);
    con.open(server, port);

    service.run();

    return 0;
}
