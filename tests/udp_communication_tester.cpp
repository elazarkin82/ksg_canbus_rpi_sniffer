#include "communication/UdpCommunication.h"
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

// --- Mock UDP Echo Server ---
class MockUdpServer {
public:
    MockUdpServer(int port) : m_port(port), m_running(false), m_sockfd(-1) {}

    ~MockUdpServer() { stop(); }

    void start() {
        m_running = true;
        m_thread = std::thread(&MockUdpServer::run, this);
    }

    void stop() {
        m_running = false;
        if (m_sockfd >= 0) {
            shutdown(m_sockfd, SHUT_RDWR);
            close(m_sockfd);
            m_sockfd = -1;
        }
        if (m_thread.joinable()) m_thread.join();
    }

private:
    void run() {
        m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_sockfd < 0) {
            perror("UDP Server socket failed");
            return;
        }

        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(m_port);

        if (bind(m_sockfd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            perror("UDP Server bind failed");
            return;
        }

        char buffer[4096];
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);

        while (m_running) {
            ssize_t n = recvfrom(m_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddr, &len);
            if (n > 0) {
                // Echo back
                sendto(m_sockfd, buffer, n, 0, (struct sockaddr*)&clientAddr, len);
            }
        }
    }

    int m_port;
    std::atomic<bool> m_running;
    int m_sockfd;
    std::thread m_thread;
};

// --- Test Listener ---
class TestListener : public ICommunicationListener {
public:
    void onDataReceived(const uint8_t* data, size_t length) override {
        m_receivedCount++;
        m_lastMsg = std::string((const char*)data, length);
    }

    void onError(int32_t errorCode) override {
        std::cerr << "Error: " << errorCode << std::endl;
    }

    void onStatusChanged(base::CommunicationStatus status) override {}

    void reset() {
        m_receivedCount = 0;
        m_lastMsg = "";
    }

    std::atomic<int> m_receivedCount{0};
    std::string m_lastMsg;
};

// --- Helper ---
void printResult(const std::string& name, bool passed) {
    if (passed) {
        std::cout << "TEST: " << name << " -> \033[32mPASSED\033[0m" << std::endl;
    } else {
        std::cout << "TEST: " << name << " -> \033[31mFAILED\033[0m" << std::endl;
    }
}

// --- Tests ---

bool testEcho(UdpCommunication& client, TestListener& listener) {
    listener.reset();
    const char* msg = "Hello UDP";
    client.send((const uint8_t*)msg, strlen(msg));

    // Wait for echo
    for (int i = 0; i < 10; ++i) {
        if (listener.m_receivedCount > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return (listener.m_receivedCount == 1 && listener.m_lastMsg == "Hello UDP");
}

bool testStress(UdpCommunication& client, TestListener& listener) {
    listener.reset();
    int count = 50;
    for (int i = 0; i < count; ++i) {
        std::string s = "Msg " + std::to_string(i);
        client.send((const uint8_t*)s.c_str(), s.length());
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Small delay to avoid packet loss on localhost
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // UDP is unreliable, but on localhost we expect most packets
    std::cout << "Received " << listener.m_receivedCount << "/" << count << std::endl;
    return (listener.m_receivedCount >= count * 0.9); // Allow 10% loss
}

int main() {
    const int SERVER_PORT = 9090;
    const int CLIENT_PORT = 9091;
    const char* IP = "127.0.0.1";

    MockUdpServer server(SERVER_PORT);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TestListener listener;
    UdpCommunication client(listener, IP, SERVER_PORT, CLIENT_PORT);

    if (!client.start()) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    std::cout << "=== Starting UDP Communication Tests ===" << std::endl;

    printResult("Echo Test", testEcho(client, listener));
    printResult("Stress Test", testStress(client, listener));

    client.stop();
    server.stop();
    return 0;
}