#include "core/MainService.h"
#include "utils/LedControllerUtil.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <chrono>

core::MainService* g_service = nullptr;
static std::atomic<bool> g_keep_running(true);

void signal_handler(int sig)
{
    (void)sig;
    g_keep_running = false;
}

int main(int argc, char* argv[])
{
    const char* configPath = "sniffer.prop";
    utils::LedControllerUtil& ledUtil = utils::LedControllerUtil::getInstance();

    if (argc > 1)
    {
        configPath = argv[1];
    }

#ifdef DEBUG
    printf("Starting RpiCanbusSniffer with config: %s\n", configPath);
#endif

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    g_service = new core::MainService(configPath);

    // Start running the service in a non-blocking way if possible, 
    // or run it in a thread while we wait here.
    // Looking at MainService::run(), it's blocking.
    
    std::thread serviceThread([]() {
        g_service->run();
    });

    // Wait for signal
    while (g_keep_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

#ifdef DEBUG
    printf("\nShutdown initiated, stopping service...\n");
#endif

    if (g_service)
    {
        g_service->stop();
    }

    if (serviceThread.joinable())
    {
        serviceThread.join();
    }

    // Reset PWR and ACT LEDs if they exist
    if (ledUtil.exists("PWR"))
    {
        ledUtil.takeControl("PWR");
        ledUtil.turnOn("PWR");
    }

    if (ledUtil.exists("ACT"))
    {
        ledUtil.takeControl("ACT");
        ledUtil.turnOff("ACT");
    }

    delete g_service;
    g_service = nullptr;

#ifdef DEBUG
    printf("Exiting...\n");
#endif
    return 0;
}
