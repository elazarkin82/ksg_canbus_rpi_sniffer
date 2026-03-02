#include "emulator.h"
#include "base/ObdPids.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// Helper to encode values
void encode_rpm(uint8_t* data, double rpm) {
    uint16_t val = (uint16_t)(rpm * 4);
    data[0] = (val >> 8) & 0xFF;
    data[1] = val & 0xFF;
}

void encode_speed(uint8_t* data, double speed) {
    data[0] = (uint8_t)speed;
}

void* broadcast_thread(void* arg) {
    int socket_fd = *(int*)arg;
    printf("[Broad] Broadcast thread started\n");

    uint8_t data[8];
    int counter_10ms = 0;

    while (1) {
        pthread_mutex_lock(&g_carState.mutex);

        // --- 10ms Tasks (High Priority) ---
        // RPM (ID 0x100 - just an example ID)
        memset(data, 0, 8);
        encode_rpm(data, g_carState.rpm);
        can_send_frame(socket_fd, 0x100, data, 2);

        // Torque (ID 0x101)
        data[0] = (uint8_t)g_carState.steering_torque;
        can_send_frame(socket_fd, 0x101, data, 1);

        // --- 50ms Tasks ---
        if (counter_10ms % 5 == 0) {
            // Speed (ID 0x200)
            memset(data, 0, 8);
            encode_speed(data, g_carState.speed);
            can_send_frame(socket_fd, 0x200, data, 1);

            // Steering Angle (ID 0x201)
            int16_t angle = (int16_t)(g_carState.steering_angle + 32768); // Offset encoding
            data[0] = (angle >> 8) & 0xFF;
            data[1] = angle & 0xFF;
            can_send_frame(socket_fd, 0x201, data, 2);
        }

        // --- 1000ms Tasks ---
        if (counter_10ms % 100 == 0) {
            // Temp & Fuel (ID 0x300)
            data[0] = (uint8_t)(g_carState.coolant_temp + 40);
            data[1] = (uint8_t)((g_carState.fuel_level * 255) / 100);
            can_send_frame(socket_fd, 0x300, data, 2);

            // Status (ID 0x301)
            data[0] = g_carState.door_status;
            data[1] = g_carState.lights_status;
            can_send_frame(socket_fd, 0x301, data, 2);
        }

        pthread_mutex_unlock(&g_carState.mutex);

        counter_10ms++;
        usleep(10000); // 10ms
    }
    return NULL;
}
