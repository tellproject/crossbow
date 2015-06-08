#include <crossbow/infinio/Endpoint.hpp>
#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>
#include <crossbow/program_options.hpp>
#include <crossbow/string.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <system_error>

using namespace crossbow::infinio;

class EchoConnection: private InfinibandSocketHandler {
public:
    EchoConnection(InfinibandSocket socket)
            : mSocket(std::move(socket)) {
        mSocket->setHandler(this);
    }

private:
    virtual void onConnected(const crossbow::string& data, const std::error_code& ec) override;

    virtual void onReceive(const void* buffer, size_t length, const std::error_code& ec) override;

    virtual void onDisconnect() override;

    virtual void onDisconnected() override;

    void handleError(std::string message, const std::error_code& ec);

    InfinibandSocket mSocket;
};

void EchoConnection::onConnected(const crossbow::string& data, const std::error_code& ec) {
    std::cout << "Connected [data = \"" << data << "\"]" << std::endl;
}

void EchoConnection::onReceive(const void* buffer, size_t length, const std::error_code& /* ec */) {
    // Acquire buffer with same size
    auto sendbuffer = mSocket->acquireSendBuffer(length);
    if (sendbuffer.id() == InfinibandBuffer::INVALID_ID) {
        handleError("Error acquiring buffer", error::invalid_buffer);
        return;
    }

    // Copy received message into send buffer
    memcpy(sendbuffer.data(), buffer, length);

    // Send incoming message back to client
    std::error_code ec;
    mSocket->send(sendbuffer, 0x0u, ec);
    if (ec) {
        handleError("Send failed", ec);
    }
}

void EchoConnection::onDisconnect() {
    std::cout << "Disconnect" << std::endl;
    try {
        mSocket->disconnect();
    } catch (std::system_error& e) {
        std::cout << "Disconnect failed " << e.code() << " - " << e.what() << std::endl;
    }
}

void EchoConnection::onDisconnected() {
    std::cout << "Disconnected" << std::endl;
}

void EchoConnection::handleError(std::string message, const std::error_code& ec) {
    std::cout << message << " [" << ec << " - " << ec.message() << "]" << std::endl;
    std::cout << "Disconnecting after error" << std::endl;
    try {
        mSocket->disconnect();
    } catch (std::system_error& e) {
        std::cout << "Disconnect failed " << e.code() << " - " << e.what() << std::endl;
    }
}

class EchoAcceptor: protected InfinibandAcceptorHandler {
public:
    EchoAcceptor(InfinibandService& service)
            : mAcceptor(service.createAcceptor()) {
    }

    void open(uint16_t port);

protected:
    virtual void onConnection(InfinibandSocket socket, const crossbow::string& data) override;

private:
    InfinibandAcceptor mAcceptor;

    std::unordered_set<std::unique_ptr<EchoConnection>> mConnections;
};

void EchoAcceptor::open(uint16_t port) {
    // Open socket
    Endpoint ep(Endpoint::ipv4(), port);
    mAcceptor->open();
    mAcceptor->setHandler(this);
    mAcceptor->bind(ep);
    mAcceptor->listen(10);
    std::cout << "Echo server started up" << std::endl;
}

void EchoAcceptor::onConnection(InfinibandSocket socket, const crossbow::string& data) {
    std::cout << "New incoming connection [data = \"" << data << "\"]" << std::endl;

    std::unique_ptr<EchoConnection> con(new EchoConnection(socket));

    try {
        socket->accept("EchoServer", 0);
    } catch (std::system_error& e) {
        std::cout << "Accepting connection failed " << e.code() << " - " << e.what() << std::endl;
        try {
            socket->close();
        } catch (std::system_error& e2) {
            std::cout << "Closing failed socket failed " << e2.code() << " - " << e2.what() << std::endl;
        }
        return;
    }

    mConnections.emplace(std::move(con));
}

int main(int argc, const char** argv) {
    bool help = false;
    uint16_t port = 4488;
    auto opts = crossbow::program_options::create_options(argv[0],
            crossbow::program_options::value<'h'>("help", &help),
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

    std::cout << "Starting echo server" << std::endl;
    InfinibandService service;
    EchoAcceptor echo(service);
    echo.open(port);

    service.run();

    return 0;
}
