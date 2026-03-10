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
    if (can_id == 0x200 && len >= 1) // Speed
    {
        m_currentSpeed = (double)data[0];
    }
    else if (can_id == 0x100 && len >= 2) // RPM
    {
        uint16_t val = (data[0] << 8) | data[1];
        m_currentRpm = (double)val / 4.0;
    }

    // --- OBD2 Request Handling (0x7DF - Broadcast, 0x7E0 - ECU Request) ---
    if (can_id == 0x7DF || can_id == 0x7E0)
    {
        // Check for Service 01 (Show Current Data)
        // Data format: [Len, Mode, PID, ...]
        if (len >= 3 && data[1] == 0x01)
        {
            uint8_t pid = data[2];
            struct can_frame response;
            memset(&response, 0, sizeof(response));
            response.can_id = 0x7E8; // ECU Response ID
            response.can_dlc = 8;

            // Standard Response Header: [Len, Mode+0x40, PID, A, B, C, D, Unused]
            response.data[1] = 0x41; // Mode 01 Response
            response.data[2] = pid;

            bool handled = false;

            switch (pid)
            {
                case 0x0C: // Engine RPM (2 bytes: (A*256+B)/4)
                {
                    int val = (int)(m_currentRpm * 4);
                    response.data[3] = (val >> 8) & 0xFF;
                    response.data[4] = val & 0xFF;
                    response.data[0] = 4; // Length (Mode, PID, A, B)
                    handled = true;
                    break;
                }
                case 0x0D: // Vehicle Speed (1 byte: A)
                {
                    response.data[3] = (uint8_t)m_currentSpeed;
                    response.data[0] = 3; // Length (Mode, PID, A)
                    handled = true;
                    break;
                }
                case 0x05: // Coolant Temp (1 byte: A-40)
                {
                    // Simulate warming up: 20C to 90C
                    static double temp = 20.0;
                    if (temp < 90.0) temp += 0.05;
                    response.data[3] = (uint8_t)(temp + 40);
                    response.data[0] = 3;
                    handled = true;
                    break;
                }
                case 0x11: // Throttle Position (1 byte: A*100/255)
                {
                    // Simulate based on speed/rpm or random
                    uint8_t throttle = (uint8_t)((m_currentRpm / 8000.0) * 255);
                    response.data[3] = throttle;
                    response.data[0] = 3;
                    handled = true;
                    break;
                }
                case 0x04: // Calculated Engine Load (1 byte: A*100/255)
                {
                    // Simulate load
                    uint8_t load = (uint8_t)((m_currentSpeed / 200.0) * 200 + 20);
                    response.data[3] = load;
                    response.data[0] = 3;
                    handled = true;
                    break;
                }
                case 0x2F: // Fuel Level (1 byte: A*100/255)
                {
                    static double fuel = 255.0;
                    if (fuel > 0) fuel -= 0.001; // Slowly decreasing
                    response.data[3] = (uint8_t)fuel;
                    response.data[0] = 3;
                    handled = true;
                    break;
                }
                case 0x10: // MAF Air Flow (2 bytes: (A*256+B)/100 g/s)
                {
                    // Rough estimation based on RPM
                    double maf = (m_currentRpm / 60.0) * 2.0; // g/s
                    int val = (int)(maf * 100);
                    response.data[3] = (val >> 8) & 0xFF;
                    response.data[4] = val & 0xFF;
                    response.data[0] = 4;
                    handled = true;
                    break;
                }
                case 0x0E: // Timing Advance (1 byte: A/2 - 64)
                {
                    // Random fluctuation around 10-20 degrees
                    double advance = 15.0 + ((rand() % 100) / 10.0 - 5.0);
                    uint8_t val = (uint8_t)((advance + 64) * 2);
                    response.data[3] = val;
                    response.data[0] = 3;
                    handled = true;
                    break;
                }
                case 0x0B: // Intake MAP (1 byte: A kPa)
                {
                    // 30-100 kPa
                    uint8_t map = 30 + (uint8_t)((m_currentRpm / 8000.0) * 70);
                    response.data[3] = map;
                    response.data[0] = 3;
                    handled = true;
                    break;
                }
                case 0x0A: // Fuel Pressure (1 byte: A*3 kPa)
                {
                    // Constant ~300kPa -> 100
                    response.data[3] = 100 + (rand() % 5);
                    response.data[0] = 3;
                    handled = true;
                    break;
                }
                case 0x1F: // Run Time (2 bytes: A*256+B seconds)
                {
                    static time_t start_time = time(NULL);
                    int seconds = (int)difftime(time(NULL), start_time);
                    response.data[3] = (seconds >> 8) & 0xFF;
                    response.data[4] = seconds & 0xFF;
                    response.data[0] = 4;
                    handled = true;
                    break;
                }
            }

            if (handled)
            {
                // Pad unused bytes with 0x55 or 0x00 (standard usually 0x00 or 0xAA/0x55)
                // We already memset to 0.
                m_canBus->send((uint8_t*)&response, sizeof(response));
            }
        }
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
