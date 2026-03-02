#include "sniffer/Sniffer.h"
#include "communication/CanbusProtocol.h"
#include "canbus_communication/ObdCanbusCommunication.h"
#include "communication/UdpCanbusCommunication.h"

#include <stdio.h>
#include <string.h>
#include <chrono>

namespace sniffer
{

Sniffer::Sniffer(const SnifferParams& params)
    : m_systemListener(*this, CanListener::SOURCE_CAR_SYSTEM),
      m_computerListener(*this, CanListener::SOURCE_CAR_COMPUTER),
      m_externalListener(*this, CanListener::SOURCE_EXTERNAL),
      m_running(false),
      m_interventionMode(false),
      m_lastExternalMsgTime(std::chrono::steady_clock::now())
{
    // Initialize CAN interfaces
    m_carSystemCan = new canbus_communication::ObdCanbusCommunication(m_systemListener, params.car_system_can_name);
    m_carComputerCan = new canbus_communication::ObdCanbusCommunication(m_computerListener, params.car_computer_can_name);

    // Initialize External Service (UDP)
    // We bind to the specified port and listen for commands
    m_externalService = new communication::UdpCanbusCommunication(m_externalListener, "0.0.0.0", 0, params.external_service_port);
    m_externalService->setCommandListener(this);
}

Sniffer::~Sniffer()
{
    stop();

    if (m_carSystemCan)
    {
        delete m_carSystemCan;
        m_carSystemCan = nullptr;
    }

    if (m_carComputerCan)
    {
        delete m_carComputerCan;
        m_carComputerCan = nullptr;
    }

    if (m_externalService)
    {
        delete m_externalService;
        m_externalService = nullptr;
    }
}

bool Sniffer::start()
{
    if (m_running)
    {
        return true;
    }

    m_running = true;

    // Start CAN interfaces
    if (!m_carSystemCan->start())
    {
        fprintf(stderr, "Failed to start Car System CAN\n");
        return false;
    }

    if (!m_carComputerCan->start())
    {
        fprintf(stderr, "Failed to start Car Computer CAN\n");
        return false;
    }

    // Start External Service
    if (!m_externalService->start())
    {
        fprintf(stderr, "Failed to start External Service\n");
        return false;
    }

    // Start Watchdog Thread
    m_watchdogThread = std::thread(&Sniffer::watchdogLoop, this);

    return true;
}

void Sniffer::stop()
{
    if (!m_running)
    {
        return;
    }

    m_running = false;

    if (m_watchdogThread.joinable())
    {
        m_watchdogThread.join();
    }

    if (m_carSystemCan) m_carSystemCan->stop();
    if (m_carComputerCan) m_carComputerCan->stop();
    if (m_externalService) m_externalService->stop();
}

void Sniffer::CanListener::onDataReceived(const uint8_t* data, size_t length)
{
    m_parent.handleCanData(m_source, data, length);
}

void Sniffer::CanListener::onError(int32_t errorCode)
{
    // Log error or handle it
    (void)errorCode;
}

void Sniffer::handleCanData(CanListener::Source source, const uint8_t* data, size_t length)
{
    if (!m_running) return;

    // Default behavior: Forward data to the other side
    // If intervention mode is active, we might block or modify
    // For now, we just forward unless blocked (logic to be added)

    bool shouldForward = true;

    // Check intervention rules here (if any)
    // ...

    if (shouldForward)
    {
        if (source == CanListener::SOURCE_CAR_SYSTEM)
        {
            m_carComputerCan->send(data, length);
        }
        else if (source == CanListener::SOURCE_CAR_COMPUTER)
        {
            m_carSystemCan->send(data, length);
        }
        else if (source == CanListener::SOURCE_EXTERNAL)
        {
            // Data from external service (not commands)
            // Maybe forward to both? Or handle as injection?
            // For now, ignore or log
        }
    }

    // Also forward to external service if connected (optional, or on demand)
    // m_externalService->send(data, length);
}

void Sniffer::onCommandReceived(uint32_t command, const uint8_t* data, size_t length)
{
    if (!m_running) return;

    // Update last message time
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastExternalMsgTime = std::chrono::steady_clock::now();
        if (!m_interventionMode)
        {
            m_interventionMode = true;
            printf("Intervention Mode Activated\n");
        }
    }

    // Handle commands
    switch (command)
    {
        case communication::CMD_CANBUS_TO_SYSTEM:
            m_carSystemCan->send(data, length);
            break;

        case communication::CMD_CANBUS_TO_CAR:
            // Assuming "Car" means the car computer side in this context?
            // Or maybe "Car System"? The names are a bit ambiguous.
            // Let's assume TO_CAR means to the car computer.
            m_carComputerCan->send(data, length);
            break;

        case communication::CMD_CANBUS_DATA:
            // Maybe just a keep-alive or data injection
            break;

        default:
            break;
    }
}

void Sniffer::watchdogLoop()
{
    while (m_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        bool resetNeeded = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_interventionMode)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastExternalMsgTime).count();
                if (elapsed > 1000) // 1 second timeout
                {
                    resetNeeded = true;
                }
            }
        }

        if (resetNeeded)
        {
            resetToDefault();
        }
    }
}

void Sniffer::resetToDefault()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_interventionMode)
    {
        m_interventionMode = false;
        printf("Timeout: Resetting to Default Mode\n");
        // Clear any filters or blocking rules here
    }
}

} // namespace sniffer
