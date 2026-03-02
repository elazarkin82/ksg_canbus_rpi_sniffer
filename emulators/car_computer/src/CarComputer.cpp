#include "CarComputer.h"
#include "base/ObdPids.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <linux/can.h>
#include <linux/can/raw.h>

CarComputer::CarComputer(const char* interfaceName)
    : m_canBus(nullptr),
      m_running(false),
      m_currentSpeed(0),
      m_currentRpm(0),
      m_targetSpeed(0),
      m_steeringAngle(0),
      m_cruiseControlActive(true)
{
    m_canBus = new canbus_communication::ObdCanbusCommunication(*this, interfaceName);
    srand(time(NULL));
}

CarComputer::~CarComputer()
{
    stop();
    if (m_canBus)
    {
        delete m_canBus;
        m_canBus = nullptr;
    }
}

bool CarComputer::start()
{
    if (m_running) return true;

    if (!m_canBus->start())
    {
        fprintf(stderr, "Failed to start CAN bus\n");
        return false;
    }

    m_running = true;
    m_controlThread = std::thread(&CarComputer::controlLoop, this);
    return true;
}

void CarComputer::stop()
{
    if (!m_running) return;

    m_running = false;
    if (m_controlThread.joinable())
    {
        m_controlThread.join();
    }
    m_canBus->stop();
}

void CarComputer::onDataReceived(const uint8_t* data, size_t length)
{
    if (length < sizeof(struct can_frame)) return;

    struct can_frame* frame = (struct can_frame*)data;
    processCanFrame(frame->can_id, frame->data, frame->can_dlc);
}

void CarComputer::onError(int32_t errorCode)
{
    (void)errorCode;
}

void CarComputer::processCanFrame(uint32_t can_id, const uint8_t* data, uint8_t len)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Process incoming data from Car System (e.g. Speed, RPM)
    // Assuming IDs based on Car System Emulator
    if (can_id == 0x200 && len >= 1) // Speed
    {
        m_currentSpeed = (double)data[0];
    }
    else if (can_id == 0x100 && len >= 2) // RPM
    {
        uint16_t val = (data[0] << 8) | data[1];
        m_currentRpm = (double)val / 4.0;
    }
}

void CarComputer::controlLoop()
{
    int counter = 0;
    struct can_frame frame;
    double throttle = 0;
    double brake = 0;
    int16_t angle = 0;

    while (m_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 10ms loop
        counter++;

        // --- Control Logic ---
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Random Driving Logic (Change target speed occasionally)
            if (counter % 500 == 0) // Every 5 seconds
            {
                m_targetSpeed = (rand() % 100) + 20; // 20 - 120 km/h
                printf("[Comp] New Target Speed: %.1f km/h\n", m_targetSpeed);
            }

            // Random Steering
            if (counter % 10 == 0) // Every 100ms
            {
                m_steeringAngle += ((rand() % 10) - 5) * 0.1; // Small random change
                if (m_steeringAngle > 30) m_steeringAngle = 30;
                if (m_steeringAngle < -30) m_steeringAngle = -30;
            }

            // Cruise Control (Throttle/Brake)
            if (m_cruiseControlActive)
            {
                double error = m_targetSpeed - m_currentSpeed;
                if (error > 0)
                {
                    throttle = error * 2.0; // Simple P controller
                    if (throttle > 100) throttle = 100;
                    brake = 0;
                }
                else
                {
                    throttle = 0;
                    brake = -error * 2.0;
                    if (brake > 100) brake = 100;
                }
            }

            // Send Commands
            if (counter % 5 == 0) // 50ms
            {
                // Throttle/Brake (ID 0x400)
                memset(&frame, 0, sizeof(frame));
                frame.can_id = 0x400;
                frame.can_dlc = 2;
                frame.data[0] = (uint8_t)throttle;
                frame.data[1] = (uint8_t)brake;
                m_canBus->send((uint8_t*)&frame, sizeof(frame));

                // Steering (ID 0x401)
                memset(&frame, 0, sizeof(frame));
                frame.can_id = 0x401;
                frame.can_dlc = 2;
                angle = (int16_t)(m_steeringAngle * 10); // Scale x10
                frame.data[0] = (angle >> 8) & 0xFF;
                frame.data[1] = angle & 0xFF;
                m_canBus->send((uint8_t*)&frame, sizeof(frame));
            }
        }
    }
}
