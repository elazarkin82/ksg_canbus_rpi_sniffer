#ifndef MAIN_SERVICE_H
#define MAIN_SERVICE_H

#include "sniffer/Sniffer.h"
#include "utils/Params.hpp"
#include <atomic>

namespace core
{

class UsbWatchdog; // Forward declaration

class MainService : public sniffer::ISystemCallback
{
public:
    MainService(const char* configPath);
    virtual ~MainService();

    int run();
    void stop();

    // --- ISystemCallback Implementation ---
    virtual void onSystemCommand(uint32_t cmd, const uint8_t* data, size_t len) override;

private:
    char m_configPath[256];
    utils::Params* m_params;
    sniffer::Sniffer* m_sniffer;

    UsbWatchdog* m_systemWatchdog;
    UsbWatchdog* m_computerWatchdog;

    std::atomic<bool> m_running;
    std::atomic<bool> m_restartRequested;

    void loadParams();
    void createSniffer();
    void destroySniffer();
};

} // namespace core

#endif // MAIN_SERVICE_H