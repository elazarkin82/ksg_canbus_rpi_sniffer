#ifndef CAR_COMPUTER_H
#define CAR_COMPUTER_H

#include "base/CommunicationObj.hpp"
#include "canbus_communication/ObdCanbusCommunication.h"
#include <atomic>
#include <thread>
#include <mutex>

class CarComputer : public base::ICommunicationListener
{
public:
    CarComputer(const char* interfaceName);
    virtual ~CarComputer();

    bool start();
    void stop();

    // --- ICommunicationListener Implementation ---
    virtual void onDataReceived(const uint8_t* data, size_t length) override;
    virtual void onError(int32_t errorCode) override;
    virtual void onStatusChanged(base::CommunicationStatus status) override {}

private:
    void controlLoop();
    void processCanFrame(uint32_t can_id, const uint8_t* data, uint8_t len);

    canbus_communication::ObdCanbusCommunication* m_canBus;

    std::atomic<bool> m_running;
    std::thread m_controlThread;
    std::mutex m_mutex;

    // State
    double m_currentSpeed;
    double m_currentRpm;
    double m_targetSpeed;
    double m_steeringAngle;

    // Control Logic
    bool m_cruiseControlActive;
};

#endif // CAR_COMPUTER_H