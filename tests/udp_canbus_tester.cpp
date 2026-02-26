#include "communication/UdpCanbusCommunication.h"
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

// --- Mock Listeners ---
class MockCanListener : public ICommunicationListener {
public:
    void onDataReceived(const uint8_t* data, size_t length) override {
        m_receivedCount++;
        m_lastLength = length;
        if (length > 0) m_lastByte = data[0];
    }

    void onError(int32_t errorCode) override {
        std::cerr << "CanListener Error: " << errorCode << std::endl;
    }

    void reset() {
        m_receivedCount = 0;
        m_lastLength = 0;
        m_lastByte = 0;
    }

    std::atomic<int> m_receivedCount{0};
    size_t m_lastLength{0};
    uint8_t m_lastByte{0};
};

class MockCommandListener : public ICommandListener {
public:
    void onCommandReceived(uint32_t command, const uint8_t* data, size_t length) override {
        m_receivedCount++;
        m_lastCommand = command;
    }

    void reset() {
        m_receivedCount = 0;
        m_lastCommand = 0;
    }

    std::atomic<int> m_receivedCount{0};
    uint32_t m_lastCommand{0};
};

// --- Mock UDP Server (Sender) ---
class MockUdpSender {
public:
    MockUdpSender(const char* ip, int port) : m_ip(ip), m_port(port), m_sockfd(-1) {
        m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    ~MockUdpSender() {
        if (m_sockfd >= 0) close(m_sockfd);
    }

    void sendV1(const char* magic, uint32_t cmd, const uint8_t* data, uint32_t len) {
        ExternalMessageV1 msg;
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.magic_key, magic, 4);
        msg.command = cmd;
        msg.data_size = len;
        if (len > 0 && len <= 1024) memcpy(msg.data, data, len);
        sendRaw(&msg, sizeof(msg));
    }

    void sendCanfd(const char* magic, const struct canfd_frame& frame) {
        ExternalCanfdMessage msg;
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.magic_key, magic, 4);
        msg.frame = frame;
        sendRaw(&msg, sizeof(msg));
    }

    void sendRaw(const void* data, size_t len) {
        struct sockaddr_in destAddr;
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(m_port);
        inet_pton(AF_INET, m_ip, &destAddr.sin_addr);

        sendto(m_sockfd, data, len, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
    }

private:
    const char* m_ip;
    int m_port;
    int m_sockfd;
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

bool testV1Normal(MockUdpSender& sender, MockCanListener& listener) {
    listener.reset();
    uint8_t payload[] = {0xAA, 0xBB};
    sender.sendV1("v1.00", CMD_CANBUS_DATA, payload, sizeof(payload));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return (listener.m_receivedCount == 1 && listener.m_lastByte == 0xAA);
}

bool testV1Command(MockUdpSender& sender, MockCommandListener& listener) {
    listener.reset();
    uint8_t payload[] = {0x11};
    sender.sendV1("v1.00", CMD_CANBUS_TO_SYSTEM, payload, sizeof(payload));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return (listener.m_receivedCount == 1 && listener.m_lastCommand == CMD_CANBUS_TO_SYSTEM);
}

bool testCanfd(MockUdpSender& sender, MockCanListener& listener) {
    listener.reset();
    struct canfd_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x123;
    frame.len = 64;

    sender.sendCanfd("canf", frame);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return (listener.m_receivedCount == 1 && listener.m_lastLength == sizeof(struct canfd_frame));
}

bool testBadMagic(MockUdpSender& sender, MockCanListener& listener) {
    listener.reset();
    uint8_t payload[] = {0x00};
    sender.sendV1("BAD!", CMD_CANBUS_DATA, payload, 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return (listener.m_receivedCount == 0);
}

int main() {
    const int CLIENT_PORT = 9092;
    const int REMOTE_PORT = 9093; // Not used for receiving, but required by constructor
    const char* IP = "127.0.0.1";

    MockCanListener canListener;
    MockCommandListener cmdListener;
    UdpCanbusCommunication client(canListener, IP, REMOTE_PORT, CLIENT_PORT);
    client.setCommandListener(&cmdListener);

    if (!client.start()) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    // Sender sends TO the client port
    MockUdpSender sender(IP, CLIENT_PORT);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "=== Starting UDP Canbus Tests ===" << std::endl;

    printResult("V1 Normal Data", testV1Normal(sender, canListener));
    printResult("V1 Command Routing", testV1Command(sender, cmdListener));
    printResult("CANFD Protocol", testCanfd(sender, canListener));
    printResult("Bad Magic Key", testBadMagic(sender, canListener));

    client.stop();
    return 0;
}
