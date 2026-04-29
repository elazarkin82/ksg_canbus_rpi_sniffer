#include "core/MainService.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <dirent.h>

namespace core
{

// Default configuration
static const char* DEFAULT_PARAMS =
    "car_system_can_name=vcan0\n"
    "car_computer_can_name=vcan1\n"
    "external_service_port=9095\n"
    "external_client_port=9096\n";

struct CanInterfaceConfig
{
    char interfaceName[64];
    char usbId[64];
    char ledName[64];
};

static inline void parseCanConfig(const char* rawConfig, CanInterfaceConfig* outConfig)
{
    const char* firstHash;
    const char* secondHash;
    size_t interfaceLen;
    size_t usbLen;

    outConfig->interfaceName[0] = '\0';
    outConfig->usbId[0] = '\0';
    outConfig->ledName[0] = '\0';

    if (!rawConfig || strlen(rawConfig) == 0)
    {
        return;
    }

    firstHash = strchr(rawConfig, '#');

    // Format: can_interface_name (No # found)
    if (!firstHash)
    {
        strncpy(outConfig->interfaceName, rawConfig, sizeof(outConfig->interfaceName) - 1);
        outConfig->interfaceName[sizeof(outConfig->interfaceName) - 1] = '\0';
        return;
    }

    secondHash = strchr(firstHash + 1, '#');

    // Format: invalid (only 1 # found). We fall back to treating it as just interface name.
    if (!secondHash)
    {
        strncpy(outConfig->interfaceName, rawConfig, sizeof(outConfig->interfaceName) - 1);
        outConfig->interfaceName[sizeof(outConfig->interfaceName) - 1] = '\0';
        return;
    }

    // Format: can_interface_name#usb_id#led_name
    interfaceLen = firstHash - rawConfig;
    if (interfaceLen > sizeof(outConfig->interfaceName) - 1)
    {
        interfaceLen = sizeof(outConfig->interfaceName) - 1;
    }
    strncpy(outConfig->interfaceName, rawConfig, interfaceLen);
    outConfig->interfaceName[interfaceLen] = '\0';

    usbLen = secondHash - (firstHash + 1);
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
        if (strlen(m_usbUniqName) == 0) return;

        m_running = true;
        m_thread = std::thread(&UsbWatchdog::watchdogLoop, this);
    }

    void stop()
    {
        if (!m_running) return;

        m_running = false;
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

private:
    bool findTtyAcm(char* outTty, size_t outSize)
    {
        char basePath[256];
        DIR* dir;
        struct dirent* entry;
        bool found = false;

        snprintf(basePath, sizeof(basePath), "/sys/bus/usb/devices/%s", m_usbUniqName);
        dir = opendir(basePath);
        if (!dir) return false;

        while ((entry = readdir(dir)) != NULL)
        {
            // Look for interface directories like "1-1.2:1.0"
            if (entry->d_type == DT_DIR && strchr(entry->d_name, ':'))
            {
                char ttyPath[512];
                DIR* ttyDir;
                struct dirent* ttyEntry;

                snprintf(ttyPath, sizeof(ttyPath), "%s/%s/tty", basePath, entry->d_name);
                ttyDir = opendir(ttyPath);
                if (ttyDir)
                {
                    while ((ttyEntry = readdir(ttyDir)) != NULL)
                    {
                        if (strncmp(ttyEntry->d_name, "ttyACM", 6) == 0)
                        {
                            strncpy(outTty, ttyEntry->d_name, outSize - 1);
                            outTty[outSize - 1] = '\0';
                            found = true;
                            break;
                        }
                    }
                    closedir(ttyDir);
                }
            }
            if (found) break;
        }
        closedir(dir);
        return found;
    }

    void watchdogLoop()
    {
        int i;

        while (m_running)
        {
            char usbPath[256];
            char netPath[256];
            bool usbPresent;
            bool netPresent;

            snprintf(usbPath, sizeof(usbPath), "/sys/bus/usb/devices/%s", m_usbUniqName);
            snprintf(netPath, sizeof(netPath), "/sys/class/net/%s", m_canInterfaceName);

            usbPresent = (access(usbPath, F_OK) == 0);
            netPresent = (access(netPath, F_OK) == 0);

            if (usbPresent && !netPresent)
            {
                char ttyName[64];
                if (findTtyAcm(ttyName, sizeof(ttyName)))
                {
                    char cmd[256];
                    
                    // slcand -o -c -s6 /dev/ttyACMx canx
                    snprintf(cmd, sizeof(cmd), "slcand -o -c -s6 /dev/%s %s &", ttyName, m_canInterfaceName);
                    system(cmd);

                    // Wait 100ms for interface to be created
                    usleep(100000);

                    // ip link set up canx
                    snprintf(cmd, sizeof(cmd), "ip link set %s up", m_canInterfaceName);
                    system(cmd);
                }
            }

            // Sleep 2 seconds (interruptible)
            for (i = 0; i < 20 && m_running; ++i)
            {
                usleep(100000);
            }
        }
    }

    char m_canInterfaceName[64];
    char m_usbUniqName[64];
    std::atomic<bool> m_running;
    std::thread m_thread;
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
            char* buffer = new char[len + 1];
            memcpy(buffer, data, len);
            buffer[len] = '\0';

#ifdef DEBUG
            printf("[MainService] Received new parameters:\n%s\n", buffer);
#endif
            m_params->update(buffer);
            delete[] buffer;
            m_restartRequested = true;
        }
    }
}

void MainService::createSniffer()
{
    sniffer::SnifferParams params;
    const char* sysNameRaw;
    const char* compNameRaw;
    CanInterfaceConfig sysConfig, compConfig;
    int servicePort, clientPort;

    if (m_sniffer) return;

#ifdef DEBUG
    printf("[MainService] Starting Sniffer...\n");
#endif

    sysNameRaw = m_params->get("car_system_can_name");
    compNameRaw = m_params->get("car_computer_can_name");

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

    // Initialize watchdogs
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
        destroySniffer();
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
