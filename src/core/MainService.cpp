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
    "external_service_port=9095\n"
    "external_client_port=9096\n"; // Added default client port

struct CanInterfaceConfig
{
    char interfaceName[64];
    char usbId[64];
    char ledName[64];
};

static inline void parseCanConfig(const char* rawConfig, CanInterfaceConfig* outConfig)
{
    outConfig->interfaceName[0] = '\0';
    outConfig->usbId[0] = '\0';
    outConfig->ledName[0] = '\0';

    if (!rawConfig || strlen(rawConfig) == 0)
    {
        return;
    }

    const char* firstHash = strchr(rawConfig, '#');

    // Format: can_interface_name (No # found)
    if (!firstHash)
    {
        strncpy(outConfig->interfaceName, rawConfig, sizeof(outConfig->interfaceName) - 1);
        outConfig->interfaceName[sizeof(outConfig->interfaceName) - 1] = '\0';
        return;
    }

    const char* secondHash = strchr(firstHash + 1, '#');

    // Format: invalid (only 1 # found). We fall back to treating it as just interface name.
    if (!secondHash)
    {
        strncpy(outConfig->interfaceName, rawConfig, sizeof(outConfig->interfaceName) - 1);
        outConfig->interfaceName[sizeof(outConfig->interfaceName) - 1] = '\0';
        return;
    }

    // Format: can_interface_name#usb_id#led_name
    size_t interfaceLen = firstHash - rawConfig;
    if (interfaceLen > sizeof(outConfig->interfaceName) - 1)
    {
        interfaceLen = sizeof(outConfig->interfaceName) - 1;
    }
    strncpy(outConfig->interfaceName, rawConfig, interfaceLen);
    outConfig->interfaceName[interfaceLen] = '\0';

    size_t usbLen = secondHash - (firstHash + 1);
    if (usbLen > sizeof(outConfig->usbId) - 1)
    {
        usbLen = sizeof(outConfig->usbId) - 1;
    }
    strncpy(outConfig->usbId, firstHash + 1, usbLen);
    outConfig->usbId[usbLen] = '\0';

    strncpy(outConfig->ledName, secondHash + 1, sizeof(outConfig->ledName) - 1);
    outConfig->ledName[sizeof(outConfig->ledName) - 1] = '\0';
}

class UsbWatchdog
{
public:
    UsbWatchdog(const char* canInterfaceName, const char* usbUniqName) : m_running(false)
    {
        strncpy(m_canInterfaceName, canInterfaceName ? canInterfaceName : "", sizeof(m_canInterfaceName) - 1);
        m_canInterfaceName[sizeof(m_canInterfaceName) - 1] = '\0';

        strncpy(m_usbUniqName, usbUniqName ? usbUniqName : "", sizeof(m_usbUniqName) - 1);
        m_usbUniqName[sizeof(m_usbUniqName) - 1] = '\0';
    }
    ~UsbWatchdog()
    {
        stop();
    }

    void start()
    {
        if (m_running) return;

        if (strlen(m_usbUniqName) == 0) return; // No USB ID to monitor

        m_running = true;

        // TODO: Add implementation for the watchdog thread
        // TODO: Add active interface restart logic when hardware is plugged/unplugged
    }

    void stop()
    {
        m_running = false;

        // TODO: Stop and join the watchdog thread
    }

private:
    char m_canInterfaceName[64];
    char m_usbUniqName[64];
    bool m_running;
};

// --- MainService Implementation ---

MainService::MainService(const char* configPath)
    : m_params(nullptr),
      m_sniffer(nullptr),
      m_systemWatchdog(nullptr),
      m_computerWatchdog(nullptr),
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
            // TODO: Watchdogs will be destroyed and recreated here when destroySniffer()/createSniffer() is called.
            // Need to ensure watchdogs cleanly shut down and release hardware resources.
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
    const char* sysNameRaw = m_params->get("car_system_can_name");
    const char* compNameRaw = m_params->get("car_computer_can_name");
    CanInterfaceConfig sysConfig, compConfig;
    int servicePort, clientPort;

    parseCanConfig(sysNameRaw ? sysNameRaw : "vcan0", &sysConfig);
    parseCanConfig(compNameRaw ? compNameRaw : "vcan1", &compConfig);

    // Construct interface#led for Sniffer
    snprintf(params.car_system_can_name, sizeof(params.car_system_can_name), "%s#%s", sysConfig.interfaceName, sysConfig.ledName);
    snprintf(params.car_computer_can_name, sizeof(params.car_computer_can_name), "%s#%s", compConfig.interfaceName, compConfig.ledName);

    servicePort = m_params->getInt("external_service_port", 9095);
    clientPort = m_params->getInt("external_client_port", 9096);

    params.external_service_port = (uint16_t)servicePort;
    params.external_client_port = (uint16_t)clientPort;

    printf("[MainService] Creating Sniffer with params:\n");
    printf("  System CAN config: %s (Watchdog USB: %s)\n", params.car_system_can_name, sysConfig.usbId);
    printf("  Computer CAN config: %s (Watchdog USB: %s)\n", params.car_computer_can_name, compConfig.usbId);
    printf("  External Service Port (Listen): %d\n", params.external_service_port);
    printf("  External Client Port (Send): %d\n", params.external_client_port);

    m_sniffer = new sniffer::Sniffer(params);
    m_sniffer->setSystemCallback(this);

    // Initialize watchdogs if needed
    // TODO: When Sniffer is decoupled, the watchdogs should be responsible for bringing up the interfaces and injecting them.
    if (strlen(sysConfig.usbId) > 0)
    {
        m_systemWatchdog = new UsbWatchdog(sysConfig.interfaceName, sysConfig.usbId);
        m_systemWatchdog->start();
    }

    if (strlen(compConfig.usbId) > 0)
    {
        m_computerWatchdog = new UsbWatchdog(compConfig.interfaceName, compConfig.usbId);
        m_computerWatchdog->start();
    }

    if (!m_sniffer->start())
    {
        fprintf(stderr, "[MainService] Failed to start Sniffer!\n");
        destroySniffer(); // This will clean up the watchdogs too
        // Wait a bit before retrying to avoid busy loop
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void MainService::destroySniffer()
{
    if (m_systemWatchdog)
    {
        m_systemWatchdog->stop();
        delete m_systemWatchdog;
        m_systemWatchdog = nullptr;
    }

    if (m_computerWatchdog)
    {
        m_computerWatchdog->stop();
        delete m_computerWatchdog;
        m_computerWatchdog = nullptr;
    }

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
