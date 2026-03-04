#include "core/MainService.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

core::MainService* g_service = nullptr;

void signal_handler(int sig)
{
    printf("\nReceived signal %d, stopping...\n", sig);
    if (g_service)
    {
        g_service->stop();
    }
}

int main(int argc, char* argv[])
{
    const char* configPath = "sniffer.prop";
    if (argc > 1)
    {
        configPath = argv[1];
    }

    printf("Starting RpiCanbusSniffer with config: %s\n", configPath);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    g_service = new core::MainService(configPath);

    // Run the service (blocking)
    g_service->run();

    delete g_service;
    g_service = nullptr;

    printf("Exiting...\n");
    return 0;
}
