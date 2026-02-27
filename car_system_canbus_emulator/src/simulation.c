#include "emulator.h"
#include "base/ObdPids.h"
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

// Simulation Constants
#define UPDATE_INTERVAL_MS 10
#define MAX_SPEED 120.0
#define IDLE_RPM 800.0
#define MAX_RPM 6000.0
#define WARMUP_RATE 0.05 // Degrees per tick
#define FUEL_CONSUMPTION 0.001 // Percent per tick

void* simulation_thread(void* arg) {
    (void)arg;
    printf("[Sim] Simulation thread started\n");

    double time_counter = 0.0;
    int state = 0; // 0: Idle, 1: Accel, 2: Cruise, 3: Brake

    while (1) {
        pthread_mutex_lock(&g_carState.mutex);

        // --- Engine Logic ---
        if (g_carState.engine_running) {
            // Warmup
            if (g_carState.coolant_temp < 90.0) {
                g_carState.coolant_temp += WARMUP_RATE;
            }

            // Fuel
            if (g_carState.fuel_level > 0) {
                g_carState.fuel_level -= FUEL_CONSUMPTION;
            }

            // Driving Cycle State Machine
            time_counter += 0.01; // 10ms
            if (time_counter > 10.0) { // Change state every 10s
                state = (state + 1) % 4;
                time_counter = 0.0;

                // Set targets based on state
                switch (state) {
                    case 0: // Idle
                        g_carState.target_speed = 0;
                        g_carState.throttle = 0;
                        g_carState.brake_pressure = 0;
                        break;
                    case 1: // Accel
                        g_carState.target_speed = 80;
                        g_carState.throttle = 60;
                        g_carState.brake_pressure = 0;
                        break;
                    case 2: // Cruise
                        g_carState.target_speed = 80;
                        g_carState.throttle = 20; // Just enough to maintain
                        g_carState.brake_pressure = 0;
                        break;
                    case 3: // Brake
                        g_carState.target_speed = 0;
                        g_carState.throttle = 0;
                        g_carState.brake_pressure = 30;
                        break;
                }
            }

            // Physics: Speed & RPM
            if (g_carState.speed < g_carState.target_speed) {
                g_carState.speed += 0.5; // Accel
            } else if (g_carState.speed > g_carState.target_speed) {
                g_carState.speed -= 0.8; // Decel
            }

            // RPM follows speed roughly (simplified gear logic)
            if (g_carState.speed < 1.0) {
                g_carState.rpm = IDLE_RPM;
            } else {
                g_carState.rpm = IDLE_RPM + (g_carState.speed * 40.0);
            }

            // Add some noise
            g_carState.rpm += (rand() % 50 - 25);

            // Odometer
            g_carState.odometer += (g_carState.speed / 3600.0) * 0.01; // km per 10ms

            // --- Steering Logic ---
            // Sine wave steering
            g_carState.steering_angle = sin(time_counter) * 30.0; // +/- 30 degrees
            g_carState.steering_torque = fabs(g_carState.steering_angle) / 10.0;

            // --- Lights Logic ---
            if (state == 3) { // Braking
                g_carState.lights_status |= 0x10; // Brake lights
            } else {
                g_carState.lights_status &= ~0x10;
            }

            if (fabs(g_carState.steering_angle) > 10.0) {
                if (g_carState.steering_angle > 0) g_carState.lights_status |= 0x08; // Right turn
                else g_carState.lights_status |= 0x04; // Left turn
            } else {
                g_carState.lights_status &= ~(0x0C);
            }

        }

        pthread_mutex_unlock(&g_carState.mutex);
        usleep(UPDATE_INTERVAL_MS * 1000);
    }
    return NULL;
}
