#include "emulator.h"
#include "base/ObdPids.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

// Global State
CarState g_carState;
volatile sig_atomic_t g_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

void init_state() {
    pthread_mutex_init(&g_carState.mutex, NULL);
    g_carState.engine_running = true;
    g_carState.rpm = 800;
    g_carState.speed = 0;
    g_carState.coolant_temp = 25;
    g_carState.fuel_level = 100;
    g_carState.door_status = 0;
    g_carState.lights_status = 0;
}

void handle_obd_request(int socket_fd, const uint8_t* req_data, uint8_t len) {
    // Debug print
    // printf("[Main] Handling request: len=%d, data=[%02X %02X %02X...]\n", len, req_data[0], req_data[1], req_data[2]);

    if (len < 3) return; // Mode + PID needed

    uint8_t mode = req_data[0];
    uint8_t pci_len = req_data[0];
    uint8_t service = req_data[1];
    uint8_t pid = req_data[2];

    if (service != 0x01) {
        // printf("[Main] Ignored service 0x%02X\n", service);
        return;
    }

    uint8_t resp[8];
    memset(resp, 0, 8);
    resp[0] = 0x00; // Length (will be set later)
    resp[1] = 0x41; // Response Mode (0x01 + 0x40)
    resp[2] = pid;

    pthread_mutex_lock(&g_carState.mutex);

    switch (pid) {
        case PID_RPM:
            {
                uint16_t val = (uint16_t)(g_carState.rpm * 4);
                resp[0] = 4; // Length (Mode + PID + A + B)
                resp[3] = (val >> 8) & 0xFF;
                resp[4] = val & 0xFF;
            }
            break;
        case PID_SPEED:
            resp[0] = 3; // Length (Mode + PID + A)
            resp[3] = (uint8_t)g_carState.speed;
            break;
        case PID_THROTTLE_POS:
            resp[0] = 3;
            resp[3] = (uint8_t)((g_carState.throttle * 255) / 100);
            break;
        case PID_COOLANT_TEMP:
            resp[0] = 3;
            resp[3] = (uint8_t)(g_carState.coolant_temp + 40);
            break;
        case PID_FUEL_LEVEL:
            resp[0] = 3;
            resp[3] = (uint8_t)((g_carState.fuel_level * 255) / 100);
            break;
        // Proprietary PIDs
        case PID_BRAKE_PRESSURE:
            resp[0] = 3;
            resp[3] = (uint8_t)g_carState.brake_pressure;
            break;
        case PID_STEERING_ANGLE:
            {
                int16_t val = (int16_t)(g_carState.steering_angle + 32768);
                resp[0] = 4;
                resp[3] = (val >> 8) & 0xFF;
                resp[4] = val & 0xFF;
            }
            break;
        default:
            // Unsupported PID
            // printf("[Main] Unsupported PID 0x%02X\n", pid);
            pthread_mutex_unlock(&g_carState.mutex);
            return;
    }

    pthread_mutex_unlock(&g_carState.mutex);

    // Simulate processing delay
    usleep(5000); // 5ms

    can_send_frame(socket_fd, CAN_ID_RESPONSE, resp, 8); // Send full 8 bytes usually
    // printf("[Main] Sent response for PID 0x%02X\n", pid);
}

// --- Library Interface ---
#ifdef BUILD_AS_LIBRARY

pthread_t g_main_thread;
int g_socket_fd = -1;

void* car_system_loop(void* arg) {
    const char* iface = (const char*)arg;

    init_state();
    printf("[Lib] Starting Car System Emulator on %s...\n", iface);

    g_socket_fd = can_open_socket(iface);
    if (g_socket_fd < 0) return NULL;

    pthread_t sim_thread, broad_thread;
    pthread_create(&sim_thread, NULL, simulation_thread, NULL);
    pthread_create(&broad_thread, NULL, broadcast_thread, &g_socket_fd);

    uint32_t can_id;
    uint8_t data[8];
    uint8_t len;

    while (g_running) {
        int res = can_read_frame(g_socket_fd, &can_id, data, &len);
        if (res > 0) {
            if (can_id == CAN_ID_REQUEST) {
                handle_obd_request(g_socket_fd, data, len);
            }
        }
    }

    printf("[Lib] Stopping Car System...\n");
    can_close_socket(g_socket_fd);
    pthread_cancel(sim_thread);
    pthread_cancel(broad_thread);
    pthread_join(sim_thread, NULL);
    pthread_join(broad_thread, NULL);
    pthread_mutex_destroy(&g_carState.mutex);
    return NULL;
}

#ifdef __cplusplus
extern "C" {
#endif

    void start_car_system(const char* interface_name) {
        g_running = 1;
        char* iface = strdup(interface_name);
        pthread_create(&g_main_thread, NULL, car_system_loop, iface);
    }

    void stop_car_system() {
        g_running = 0;
        pthread_join(g_main_thread, NULL);
    }

    // Accessors for Python
    double get_rpm() { return g_carState.rpm; }
    double get_speed() { return g_carState.speed; }
    double get_throttle() { return g_carState.throttle; }
    double get_brake() { return g_carState.brake_pressure; }
    double get_steering() { return g_carState.steering_angle; }

#ifdef __cplusplus
}
#endif

#else

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <can_interface>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_sigint);
    init_state();

    printf("[Main] Starting Car System Emulator on %s...\n", argv[1]);

    int socket_fd = can_open_socket(argv[1]);
    if (socket_fd < 0) return 1;

    pthread_t sim_thread, broad_thread;
    pthread_create(&sim_thread, NULL, simulation_thread, NULL);
    pthread_create(&broad_thread, NULL, broadcast_thread, &socket_fd);

    // Main Loop: Listen for Requests
    uint32_t can_id;
    uint8_t data[8];
    uint8_t len;

    while (g_running) {
        int res = can_read_frame(socket_fd, &can_id, data, &len);
        if (res > 0) {
            // Debug print
            // printf("[Main] Received frame ID 0x%X DLC %d\n", can_id, len);

            if (can_id == CAN_ID_REQUEST) {
                handle_obd_request(socket_fd, data, len);
            }
        }
    }

    printf("\n[Main] Stopping...\n");
    can_close_socket(socket_fd);
    pthread_cancel(sim_thread); // Or use a flag
    pthread_cancel(broad_thread);
    pthread_join(sim_thread, NULL);
    pthread_join(broad_thread, NULL);
    pthread_mutex_destroy(&g_carState.mutex);

    return 0;
}

#endif
