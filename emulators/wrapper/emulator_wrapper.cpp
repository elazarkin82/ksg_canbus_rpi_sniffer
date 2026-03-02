#include "CarComputer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Car System C API (from main.c) ---
extern "C" {
    void start_car_system(const char* interface_name);
    void stop_car_system();
    double get_rpm();
    double get_speed();
    double get_throttle();
    double get_brake();
    double get_steering();
}

// --- Global Instances ---
static CarComputer* g_carComputer = nullptr;

// --- Wrapper API for Python ---
extern "C" {

    void start_system_emulator(const char* interface_name) {
        start_car_system(interface_name);
    }

    void stop_system_emulator() {
        stop_car_system();
    }

    void start_computer_emulator(const char* interface_name) {
        if (g_carComputer) return;
        g_carComputer = new CarComputer(interface_name);
        g_carComputer->start();
    }

    void stop_computer_emulator() {
        if (g_carComputer) {
            delete g_carComputer;
            g_carComputer = nullptr;
        }
    }

    // Getters for System State (Reflection)
    double wrapper_get_rpm() { return get_rpm(); }
    double wrapper_get_speed() { return get_speed(); }
    double wrapper_get_throttle() { return get_throttle(); }
    double wrapper_get_brake() { return get_brake(); }
    double wrapper_get_steering() { return get_steering(); }

}
