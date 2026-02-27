#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// --- Car State Structure ---
typedef struct {
    // Engine
    double rpm;             // 0 - 8000
    double speed;           // 0 - 240 km/h
    double throttle;        // 0 - 100 %
    double coolant_temp;    // -40 - 120 C
    double fuel_level;      // 0 - 100 %

    // Chassis
    double brake_pressure;  // 0 - 100 Bar
    double steering_angle;  // -500 to +500 degrees
    double steering_torque; // 0 - 10 Nm
    double odometer;        // km

    // Body
    uint8_t door_status;    // Bitmask
    uint8_t lights_status;  // Bitmask

    // Internal Simulation State
    bool engine_running;
    double target_speed;    // For simulation logic

    // Thread Safety
    pthread_mutex_t mutex;
} CarState;

// Global State Instance (defined in main.c)
extern CarState g_carState;

// --- Function Prototypes ---

// Simulation Thread
void* simulation_thread(void* arg);

// Broadcast Thread
void* broadcast_thread(void* arg);

// CAN Utils
int can_open_socket(const char* interface_name);
void can_close_socket(int socket_fd);
int can_send_frame(int socket_fd, uint32_t can_id, const uint8_t* data, uint8_t len);
int can_read_frame(int socket_fd, uint32_t* can_id, uint8_t* data, uint8_t* len);

#endif // EMULATOR_H
