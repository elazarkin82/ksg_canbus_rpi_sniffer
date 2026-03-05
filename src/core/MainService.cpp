#include "core/MainService.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <chrono>

namespace core
{

// Default configuration
static const char* DEFAULT_PARAMS =
    "car_system_can_name=vcan0\n"
    "car_computer_can_name=vcan1\n"
    "external_service_port=9095\n";

MainService::MainService(const char* configPath)
    : m_params(nullptr),
      m_sniffer(nullptr),
      m_running(false),
      m_restartRequested(false)
{
    strncpy(m_configPath, configPath, sizeof(m_configPath) - 1);
    m_configPath[sizeof(m_configPath) - 1] = '\0';
}

MainService::~MainService()
{
    stop();
    destroySniffer();
    if (m_params)
    {
        delete m_params;
        m_params = nullptr;
    }
}

int MainService::run()
{
    if (m_running) return 0;

    // Load Params
    m_params = new utils::Params(m_configPath, DEFAULT_PARAMS);

    m_running = true;

    while (m_running)
    {
        if (!m_sniffer)
        {
            createSniffer();
        }

        // Main Loop Sleep
        // Check for restart request periodically
        for (int i = 0; i < 10; ++i) // 1 second total
        {
            if (!m_running) break;
            if (m_restartRequested) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (m_restartRequested)
        {
#ifdef DEBUG
            printf("[MainService] Restart requested. Reloading configuration...\n");
#endif
            destroySniffer();
            m_restartRequested = false;
            // Params are updated via onSystemCommand, but we might want to reload from file to be sure?
            // Actually, update() saves to file, so the object is up to date.
        }
    }

    destroySniffer();
    return 0;
}

void MainService::stop()
{
    m_running = false;
}

void MainService::onSystemCommand(uint32_t cmd, const uint8_t* data, size_t len)
{
    if (cmd == communication::CMD_SET_PARAMS)
    {
        if (len > 0 && m_params)
        {
            // Ensure null termination for string parsing
            char* buffer = new char[len + 1];
            memcpy(buffer, data, len);
            buffer[len] = '\0';

#ifdef DEBUG
            printf("[MainService] Received new parameters:\n%s\n", buffer);
#endif
            m_params->update(buffer);

            delete[] buffer;

            // Trigger restart
            m_restartRequested = true;
        }
    }
}

void MainService::createSniffer()
{
    if (m_sniffer) return;

#ifdef DEBUG
    printf("[MainService] Starting Sniffer...\n");
#endif

    sniffer::SnifferParams params;

    const char* sysName = m_params->get("car_system_can_name");
    const char* compName = m_params->get("car_computer_can_name");
    int port = m_params->getInt("external_service_port", 9095);

    strncpy(params.car_system_can_name, sysName ? sysName : "vcan0", 16);
    strncpy(params.car_computer_can_name, compName ? compName : "vcan1", 16);
    params.external_service_port = (uint16_t)port;

    m_sniffer = new sniffer::Sniffer(params);
    m_sniffer->setSystemCallback(this);

    if (!m_sniffer->start())
    {
        fprintf(stderr, "[MainService] Failed to start Sniffer!\n");
        delete m_sniffer;
        m_sniffer = nullptr;
        // Wait a bit before retrying to avoid busy loop
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void MainService::destroySniffer()
{
    if (m_sniffer)
    {
#ifdef DEBUG
        printf("[MainService] Stopping Sniffer...\n");
#endif
        m_sniffer->stop();
        delete m_sniffer;
        m_sniffer = nullptr;
    }
}

} // namespace core
