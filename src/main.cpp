#include "core/MainService.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

core::MainService* g_service = nullptr;

void signal_handler(int sig)
{
#ifdef DEBUG
    printf("\nReceived signal %d, stopping...\n", sig);
#endif
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
