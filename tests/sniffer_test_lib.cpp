#include "sniffer/Sniffer.h"
#include "communication/CanbusProtocol.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace sniffer;
using namespace communication;

// --- Helper Functions ---

int open_can_socket(const char* iface) {
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) return -1;

    struct ifreq ifr;
    strcpy(ifr.ifr_name, iface);
    ioctl(s, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }
    return s;
}

bool read_can_frame(int s, uint32_t expected_id, uint8_t* out_data, int timeout_ms) {
    struct can_frame frame;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeout_ms) {
            return false; // Timeout
        }

        // Non-blocking read check (using select or just MSG_DONTWAIT if set, but here we use simple loop with small sleep)
        // Better to use select/poll
        fd_set rdfs;
        FD_ZERO(&rdfs);
        FD_SET(s, &rdfs);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms

        if (select(s + 1, &rdfs, NULL, NULL, &tv) > 0) {
            int nbytes = read(s, &frame, sizeof(frame));
            if (nbytes > 0) {
                if (frame.can_id == expected_id) {
                    if (out_data) memcpy(out_data, frame.data, frame.can_dlc);
                    return true;
                }
            }
        }
    }
}

void send_udp_command(uint32_t cmd, const void* data, size_t len, int port) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    ExternalMessageV1 msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.magic_key, "v1.00", 8);
    msg.command = cmd;
    msg.data_size = len;
    if (len > 0) memcpy(msg.data, data, len);

    // Calculate actual size to send
    size_t send_len = sizeof(msg) - sizeof(msg.data) + len;
    sendto(s, &msg, send_len, 0, (struct sockaddr*)&addr, sizeof(addr));
    close(s);
}

// --- Test Cases ---

bool test_passthrough(int s_sys, int s_comp) {
    std::cout << "[TEST] Passthrough: Checking if RPM (0x100) passes from System to Computer..." << std::endl;
    // RPM is sent by emulator on vcan0 (System). Sniffer should forward to vcan1 (Computer).
    // We listen on vcan1 (s_comp).
    if (read_can_frame(s_comp, 0x100, NULL, 2000)) {
        std::cout << " -> PASSED" << std::endl;
        return true;
    }
    std::cout << " -> FAILED" << std::endl;
    return false;
}

bool test_drop_rule(int s_sys, int s_comp, int port) {
    std::cout << "[TEST] DROP Rule: Blocking Speed (0x200)..." << std::endl;

    // Create Rule
    CanFilterRule rule;
    memset(&rule, 0, sizeof(rule));
    rule.can_id = 0x200;
    rule.action_type = 1; // DROP
    rule.target = 0; // BOTH (or TO_CAR)

    send_udp_command(CMD_SET_FILTERS, &rule, sizeof(rule), port);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if 0x200 is received on vcan1
    if (!read_can_frame(s_comp, 0x200, NULL, 1000)) {
        std::cout << " -> PASSED (Frame blocked)" << std::endl;
        return true;
    }
    std::cout << " -> FAILED (Frame received)" << std::endl;
    return false;
}

bool test_modify_rule(int s_sys, int s_comp, int port) {
    std::cout << "[TEST] MODIFY Rule: Changing RPM (0x100) byte 0 to 0xFF..." << std::endl;

    // Create Rule
    CanFilterRule rule;
    memset(&rule, 0, sizeof(rule));
    rule.can_id = 0x100;
    rule.action_type = 2; // MODIFY
    rule.modification_mask[0] = 0xFF;
    rule.modification_data[0] = 0xFF;

    send_udp_command(CMD_SET_FILTERS, &rule, sizeof(rule), port);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint8_t data[8];
    if (read_can_frame(s_comp, 0x100, data, 2000)) {
        if (data[0] == 0xFF) {
            std::cout << " -> PASSED (Data modified)" << std::endl;
            return true;
        }
        std::cout << " -> FAILED (Data: " << std::hex << (int)data[0] << ")" << std::endl;
    } else {
        std::cout << " -> FAILED (Frame not received)" << std::endl;
    }
    return false;
}

// --- Main Entry Point ---

extern "C" int run_all_sniffer_tests() {
    std::cout << "=== Starting Sniffer Integration Tests ===" << std::endl;

    // 1. Setup Sniffer
    SnifferParams params;
    strncpy(params.car_system_can_name, "vcan0", 16);
    strncpy(params.car_computer_can_name, "vcan1", 16);
    params.external_service_port = 9095;

    Sniffer sniffer(params);
    if (!sniffer.start()) {
        std::cerr << "Failed to start Sniffer" << std::endl;
        return 1;
    }

    // 2. Open Verification Sockets
    int s_sys = open_can_socket("vcan0");
    int s_comp = open_can_socket("vcan1");
    if (s_sys < 0 || s_comp < 0) {
        std::cerr << "Failed to open CAN sockets" << std::endl;
        return 1;
    }

    // 3. Run Tests
    bool all_passed = true;

    if (!test_passthrough(s_sys, s_comp)) all_passed = false;

    // Clear rules before next test (send empty rule set? or just overwrite)
    // For now, test_drop overwrites.
    if (!test_drop_rule(s_sys, s_comp, params.external_service_port)) all_passed = false;

    if (!test_modify_rule(s_sys, s_comp, params.external_service_port)) all_passed = false;

    // 4. Cleanup
    sniffer.stop();
    close(s_sys);
    close(s_comp);

    std::cout << "=== Tests Finished: " << (all_passed ? "PASSED" : "FAILED") << " ===" << std::endl;
    return all_passed ? 0 : 1;
}
