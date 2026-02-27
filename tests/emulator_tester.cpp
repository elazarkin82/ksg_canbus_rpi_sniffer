#include "canbus_communication/ObdCanbusCommunication.h"
#include "base/ObdPids.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <vector>

using namespace canbus_communication;
using namespace base;

class EmulatorListener : public ICommunicationListener {
public:
    void onDataReceived(const uint8_t* data, size_t length) override {
        if (length < sizeof(struct can_frame)) return;

        struct can_frame* frame = (struct can_frame*)data;

        // Debug print
        // std::cout << "[Tester] Rx ID: 0x" << std::hex << frame->can_id << std::dec << std::endl;

        // Check for Broadcasts
        if (frame->can_id == 0x100) { // RPM Broadcast
            m_broadcastReceived = true;
        }

        // Check for Response
        if (frame->can_id == CAN_ID_RESPONSE) {
            uint8_t pid = frame->data[2];
            std::cout << "[Tester] Got Response for PID 0x" << std::hex << (int)pid << std::dec << std::endl;

            if (pid == m_expectedPid) {
                m_responseReceived = true;
                m_lastValue = frame->data[3]; // Simplified value check
            }
        }
    }

    void onError(int32_t errorCode) override {
        std::cerr << "Error: " << errorCode << std::endl;
    }

    void reset(uint8_t expectedPid = 0) {
        m_broadcastReceived = false;
        m_responseReceived = false;
        m_expectedPid = expectedPid;
        m_lastValue = 0;
    }

    std::atomic<bool> m_broadcastReceived{false};
    std::atomic<bool> m_responseReceived{false};
    uint8_t m_expectedPid{0};
    uint8_t m_lastValue{0};
};

void printResult(const std::string& name, bool passed) {
    if (passed) {
        std::cout << "TEST: " << name << " -> \033[32mPASSED\033[0m" << std::endl;
    } else {
        std::cout << "TEST: " << name << " -> \033[31mFAILED\033[0m" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    const char* iface = "vcan0";
    if (argc > 1) iface = argv[1];

    std::cout << "Connecting to " << iface << "..." << std::endl;

    EmulatorListener listener;
    ObdCanbusCommunication comm(listener, iface);

    if (!comm.start()) {
        std::cerr << "Failed to start communication on " << iface << std::endl;
        return 1;
    }

    std::cout << "=== Starting Emulator Tests ===" << std::endl;

    // 1. Test Broadcasts
    listener.reset();
    std::cout << "Waiting for broadcasts..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    printResult("Broadcast Reception", listener.m_broadcastReceived);

    // 2. Test Request/Response (RPM)
    listener.reset(PID_RPM);
    struct can_frame req;
    req.can_id = CAN_ID_REQUEST;
    req.can_dlc = 8;
    memset(req.data, 0, 8);
    req.data[0] = 0x02; // PCI Length
    req.data[1] = 0x01; // Service 01
    req.data[2] = PID_RPM;

    std::cout << "Sending Request for PID_RPM..." << std::endl;
    comm.send((const uint8_t*)&req, sizeof(req));
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Increased wait time
    printResult("PID_RPM Request", listener.m_responseReceived);

    // 3. Test Request/Response (Speed)
    listener.reset(PID_SPEED);
    req.data[2] = PID_SPEED;
    std::cout << "Sending Request for PID_SPEED..." << std::endl;
    comm.send((const uint8_t*)&req, sizeof(req));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    printResult("PID_SPEED Request", listener.m_responseReceived);

    comm.stop();
    return 0;
}
