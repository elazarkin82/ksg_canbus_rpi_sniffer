#include "communication/TcpCommunication.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace communication;
using namespace base;

// --- Simple TCP Echo Server for Testing ---
class TestServer {
public:
    TestServer(int port) : m_port(port), m_running(false), m_serverFd(-1) {}

    ~TestServer() { stop(); }

    void start() {
        m_running = true;
        m_thread = std::thread(&TestServer::run, this);
    }

    void stop() {
        m_running = false;
        if (m_serverFd >= 0) {
            shutdown(m_serverFd, SHUT_RDWR);
            close(m_serverFd);
            m_serverFd = -1;
        }
        if (m_thread.joinable()) m_thread.join();
    }

private:
    void run() {
        m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_serverFd < 0) {
            perror("Server socket failed");
            return;
        }

        int opt = 1;
        setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(m_port);

        if (bind(m_serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            perror("Server bind failed");
            return;
        }

        if (listen(m_serverFd, 3) < 0) {
            perror("Server listen failed");
            return;
        }

        std::cout << "[Server] Listening on port " << m_port << std::endl;

        while (m_running) {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int clientFd = accept(m_serverFd, (struct sockaddr*)&clientAddr, &addrLen);

            if (clientFd < 0) {
                if (m_running) perror("Server accept failed");
                break;
            }

            std::cout << "[Server] Client connected" << std::endl;
            handleClient(clientFd);
            std::cout << "[Server] Client disconnected" << std::endl;
        }
    }

    void handleClient(int clientFd) {
        char buffer[1024];
        while (m_running) {
            ssize_t valread = read(clientFd, buffer, 1024);
            if (valread <= 0) {
                close(clientFd);
                break;
            }
            // Echo back
            send(clientFd, buffer, valread, 0);
        }
    }

    int m_port;
    std::atomic<bool> m_running;
    int m_serverFd;
    std::thread m_thread;
};

// --- Test Listener ---
class TestListener : public ICommunicationListener {
public:
    void onDataReceived(const uint8_t* data, size_t length) override {
        std::string msg((const char*)data, length);
        std::cout << "[Client] Received: " << msg << " (" << length << " bytes)" << std::endl;
        m_receivedCount++;
    }

    void onError(int32_t errorCode) override {
        std::cerr << "[Client] Error: " << errorCode << std::endl;
    }

    std::atomic<int> m_receivedCount{0};
};

int main() {
    const int PORT = 8080;
    const std::string IP = "127.0.0.1";

    std::cout << "--- Starting TCP Communication Test ---" << std::endl;

    // 1. Start Server
    TestServer server(PORT);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for server

    // 2. Create Client
    TestListener listener;
    TcpCommunication client(listener, IP, PORT);

    // 3. Test Connection
    std::cout << "\n[Test] Connecting..." << std::endl;
    if (client.start()) {
        std::cout << "[Test] Connected successfully!" << std::endl;
    } else {
        std::cerr << "[Test] Failed to connect!" << std::endl;
        return 1;
    }

    // 4. Test Send/Receive (Echo)
    std::cout << "\n[Test] Sending 'Hello World'..." << std::endl;
    const char* msg = "Hello World";
    client.send((const uint8_t*)msg, strlen(msg));

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for echo

    if (listener.m_receivedCount > 0) {
        std::cout << "[Test] Echo received successfully!" << std::endl;
    } else {
        std::cerr << "[Test] Echo FAILED!" << std::endl;
    }

    // 5. Test Stress (Flood)
    std::cout << "\n[Test] Starting Stress Test (100 messages)..." << std::endl;
    listener.m_receivedCount = 0;
    for (int i = 0; i < 100; ++i) {
        std::string s = "Msg " + std::to_string(i);
        client.send((const uint8_t*)s.c_str(), s.length());
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Small delay
    }

    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for all echoes

    std::cout << "[Test] Received " << listener.m_receivedCount << "/100 messages back." << std::endl;

    // 6. Cleanup
    std::cout << "\n[Test] Stopping..." << std::endl;
    client.stop();
    server.stop();

    std::cout << "--- Test Finished ---" << std::endl;
    return 0;
}
