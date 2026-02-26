#include "communication/TcpCommunication.h"
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
#include <sstream>

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

    // Helper to send data directly to a connected client (for stress test)
    // Note: This is a simplified server that handles one client at a time in the main loop.
    // For more complex scenarios, we might need a list of clients.
    // But for this test, the server just echoes.
    // To simulate server-initiated flood, we can add a method here if needed,
    // but the current tests rely on client sending first or echo.
    // Let's add a method to flood the last connected client.
    void floodClient(int count, int delayMs) {
        // This is tricky because the server loop is blocking on accept/read.
        // For the latency test, we want the SERVER to send data fast.
        // We can do this by having a separate thread or just modifying the handleClient logic.
        // For simplicity, let's make the client send the flood requests, and server echoes.
        // OR, we can make a special "Flood" command.
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

        while (m_running) {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int clientFd = accept(m_serverFd, (struct sockaddr*)&clientAddr, &addrLen);

            if (clientFd < 0) {
                if (m_running) perror("Server accept failed");
                break;
            }

            handleClient(clientFd);
        }
    }

    void handleClient(int clientFd) {
        char buffer[4096];
        while (m_running) {
            ssize_t valread = read(clientFd, buffer, sizeof(buffer));
            if (valread <= 0) {
                close(clientFd);
                break;
            }

            // Check for special "FLOOD" command
            if (valread >= 5 && strncmp(buffer, "FLOOD", 5) == 0) {
                // Send 1000 messages back fast
                for (int i = 0; i < 1000; ++i) {
                    std::string msg = "FloodMsg " + std::to_string(i) + "\n";
                    send(clientFd, msg.c_str(), msg.length(), MSG_NOSIGNAL);
                    // No delay here to simulate burst
                }
            } else {
                // Echo back
                send(clientFd, buffer, valread, MSG_NOSIGNAL);
            }
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
    TestListener() : m_receivedCount(0), m_processingDelayMs(0), m_lastSeq(-1), m_drops(0) {}

    void onDataReceived(const uint8_t* data, size_t length) override {
        std::string msg((const char*)data, length);

        // Simulate processing delay
        if (m_processingDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_processingDelayMs));
        }

        // Check for sequence number in flood messages
        if (msg.find("FloodMsg") == 0) {
            try {
                int seq = std::stoi(msg.substr(9));
                if (m_lastSeq != -1 && seq != m_lastSeq + 1) {
                    // Gap detected!
                    m_drops += (seq - m_lastSeq - 1);
                }
                m_lastSeq = seq;
            } catch (...) {}
        }

        m_receivedCount++;
    }

    void onError(int32_t errorCode) override {
        std::cerr << "[Client] Error: " << errorCode << std::endl;
    }

    void reset() {
        m_receivedCount = 0;
        m_processingDelayMs = 0;
        m_lastSeq = -1;
        m_drops = 0;
    }

    std::atomic<int> m_receivedCount;
    std::atomic<int> m_processingDelayMs;
    int m_lastSeq;
    int m_drops;
};

// --- Helper Functions ---
void printTestResult(const std::string& testName, bool passed) {
    std::cout << "--------------------------------------------------" << std::endl;
    if (passed) {
        std::cout << "TEST: " << testName << " -> \033[32mPASSED\033[0m" << std::endl;
    } else {
        std::cout << "TEST: " << testName << " -> \033[31mFAILED\033[0m" << std::endl;
    }
    std::cout << "--------------------------------------------------" << std::endl;
}

// --- Test Cases ---

bool testConnection(TcpCommunication& client) {
    std::cout << "[Test] Attempting to connect..." << std::endl;
    if (client.start()) {
        std::cout << "[Test] Connected." << std::endl;
        return true;
    }
    std::cerr << "[Test] Connection failed." << std::endl;
    return false;
}

bool testEcho(TcpCommunication& client, TestListener& listener) {
    listener.reset();
    const char* msg = "Hello World";
    std::cout << "[Test] Sending: " << msg << std::endl;

    client.send((const uint8_t*)msg, strlen(msg));

    // Wait for echo
    for (int i = 0; i < 10; ++i) {
        if (listener.m_receivedCount > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (listener.m_receivedCount == 1) {
        return true;
    }
    return false;
}

bool testStress(TcpCommunication& client, TestListener& listener) {
    listener.reset();
    int count = 100;
    std::cout << "[Test] Sending " << count << " messages..." << std::endl;

    for (int i = 0; i < count; ++i) {
        std::string s = "Msg " + std::to_string(i);
        client.send((const uint8_t*)s.c_str(), s.length());
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Small delay to let server breathe
    }

    // Wait for echoes
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "[Test] Received " << listener.m_receivedCount << "/" << count << std::endl;
    return listener.m_receivedCount == count;
}

bool testLatencyRecovery(TcpCommunication& client, TestListener& listener) {
    listener.reset();

    // 1. Set artificial delay in listener (simulating slow processing)
    // The server will send 1000 messages very fast.
    // If we process each message for 10ms, we can only handle 100 msg/sec.
    // The buffer should fill up and drop packets.
    listener.m_processingDelayMs = 5;

    std::cout << "[Test] Triggering server flood (1000 msgs)..." << std::endl;
    std::cout << "[Test] Client processing delay set to " << listener.m_processingDelayMs << "ms per message." << std::endl;

    // Send command to server to start flooding
    std::string cmd = "FLOOD";
    client.send((const uint8_t*)cmd.c_str(), cmd.length());

    // Wait enough time for all processing to finish (or be dropped)
    // 1000 msgs * 5ms = 5 seconds if processed serially.
    // But many will be dropped.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "[Test] Received: " << listener.m_receivedCount << std::endl;
    std::cout << "[Test] Drops detected (seq gaps): " << listener.m_drops << std::endl;

    // Criteria for success:
    // 1. We received SOME messages (not 0).
    // 2. We dropped SOME messages (because we were slow).
    // 3. The client is still alive and can receive normal messages.

    bool passed = (listener.m_receivedCount > 0) && (listener.m_drops > 0);

    if (passed) {
        std::cout << "[Test] Recovery check: Sending normal message..." << std::endl;
        listener.m_processingDelayMs = 0; // Remove delay
        listener.m_receivedCount = 0;

        std::string s = "RecoveryMsg";
        client.send((const uint8_t*)s.c_str(), s.length());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (listener.m_receivedCount == 1) {
            std::cout << "[Test] Recovery successful." << std::endl;
        } else {
            std::cerr << "[Test] Recovery FAILED (Client stuck?)" << std::endl;
            passed = false;
        }
    }

    return passed;
}

int main() {
    const int PORT = 8081; // Use different port
    const char* IP = "127.0.0.1"; // Changed to const char*

    // Start Server
    TestServer server(PORT);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Create Client
    TestListener listener;
    // Use smaller buffer to force drops easier in latency test
    TcpCommunication client(listener, IP, PORT, 4096 * 10);

    std::cout << "=== Starting TCP Communication Tests ===" << std::endl;

    // Run Tests
    bool res;

    res = testConnection(client);
    printTestResult("Connection", res);

    if (res) {
        res = testEcho(client, listener);
        printTestResult("Echo", res);

        res = testStress(client, listener);
        printTestResult("Stress (100 msgs)", res);

        res = testLatencyRecovery(client, listener);
        printTestResult("Latency & Recovery", res);
    }

    client.stop();
    server.stop();

    std::cout << "=== All Tests Finished ===" << std::endl;
    return 0;
}
