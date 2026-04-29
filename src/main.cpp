#include "core/MainService.h"
#include "utils/LedControllerUtil.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

core::MainService* g_service = nullptr;

void signal_handler(int sig)
{
    utils::LedControllerUtil& ledUtil = utils::LedControllerUtil::getInstance();

#ifdef DEBUG
    printf("\nReceived signal %d, stopping...\n", sig);
#endif

    if (g_service)
    {
        g_service->stop();
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
}

int main(int argc, char* argv[])
{
    const char* configPath = "sniffer.prop";

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

    // Run the service (blocking)
    g_service->run();

    delete g_service;
    g_service = nullptr;

#ifdef DEBUG
    printf("Exiting...\n");
#endif
    return 0;
}
