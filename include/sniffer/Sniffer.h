#ifndef SNIFFER_H
#define SNIFFER_H

#include "base/CommunicationObj.hpp"
#include "communication/CanbusProtocol.h"
#include "canbus_communication/ObdCanbusCommunication.h"
#include "communication/UdpCanbusCommunication.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace sniffer
{

struct SnifferParams
{
    char car_system_can_name[16];
    char car_computer_can_name[16];
    uint16_t external_service_port;
    uint16_t external_client_port; // New parameter for target port
};

class ISystemCallback
{
public:
    virtual ~ISystemCallback() {}
    virtual void onSystemCommand(uint32_t cmd, const uint8_t* data, size_t len) = 0;
};

class Sniffer : public communication::ICommandListener
{
public:
    enum Source { SOURCE_CAR_SYSTEM, SOURCE_CAR_COMPUTER, SOURCE_EXTERNAL };

    Sniffer(const SnifferParams& params);
    virtual ~Sniffer();

    bool start();
    void stop();

    void setSystemCallback(ISystemCallback* callback);

    // --- ICommandListener Implementation (from External Service) ---
    virtual void onCommandReceived(uint32_t command, const uint8_t* data, size_t length) override;

private:
    // Internal listener to distinguish between sources
    class CanListener : public base::ICommunicationListener
    {
    public:
        CanListener(Sniffer& parent, Source source)
            : m_parent(parent), m_source(source) {}

        virtual void onDataReceived(const uint8_t* data, size_t length) override;
        virtual void onError(int32_t errorCode) override;

    private:
        Sniffer& m_parent;
        Source m_source;
    };

    void handleCanData(Source source, const uint8_t* data, size_t length);
    void watchdogLoop();
    void resetToDefault();

    // Communication objects
    canbus_communication::ObdCanbusCommunication* m_carSystemCan;
    canbus_communication::ObdCanbusCommunication* m_carComputerCan;
    communication::UdpCanbusCommunication* m_externalService;

    // Listeners
    CanListener m_systemListener;
    CanListener m_computerListener;
    CanListener m_externalListener;

    // State
    std::atomic<bool> m_running;
    std::atomic<bool> m_externalServiceLogging;
    std::chrono::steady_clock::time_point m_lastExternalMsgTime;

    ISystemCallback* m_systemCallback;

    std::thread m_watchdogThread;
    std::mutex m_mutex; // Protects shared state
};

} // namespace sniffer

#endif // SNIFFER_H
