#include "communication/TcpCanbusCommunication.h"
#include <cstring>
#include <iostream>

namespace communication
{

TcpCanbusCommunication::TcpCanbusCommunication(base::ICommunicationListener& listener, const std::string& ip, uint16_t port)
    : TcpCommunication(*this, ip, port), // Pass *this as the listener to the base TcpCommunication
      m_targetListener(listener),
      m_commandListener(nullptr)
{
}

TcpCanbusCommunication::~TcpCanbusCommunication()
{
}

void TcpCanbusCommunication::setCommandListener(ICommandListener* commandListener)
{
    m_commandListener = commandListener;
}

void TcpCanbusCommunication::onDataReceived(const uint8_t* data, size_t length)
{
    // Process the incoming buffer directly, assuming fixed-size packets
    processBuffer(data, length);
}

void TcpCanbusCommunication::onError(int32_t errorCode)
{
    // Forward error to the target listener
    m_targetListener.onError(errorCode);
}

void TcpCanbusCommunication::processBuffer(const uint8_t* data, size_t length)
{
    size_t offset = 0;
    const uint8_t* ptr = nullptr;
    size_t remaining = length;

    while (remaining >= 4)
    {
        ptr = data + offset;

        // Check for V1 Protocol ("v1.00")
        if (strncmp((const char*)ptr, "v1.00", 4) == 0)
        {
            if (remaining >= sizeof(ExternalMessageV1))
            {
                const ExternalMessageV1* msg = (const ExternalMessageV1*)ptr;

                if (msg->data_size <= 1024)
                {
                    if (msg->command == CMD_CANBUS_DATA)
                    {
                        // Forward CAN data to the main listener
                        m_targetListener.onDataReceived(msg->data, msg->data_size);
                    }
                    else if (m_commandListener)
                    {
                        // Forward other commands to the command listener
                        m_commandListener->onCommandReceived(msg->command, msg->data, msg->data_size);
                    }
                    else
                    {
                        // TODO: Handle unhandled commands internally or log
                    }
                }
                else
                {
                    fprintf(stderr, "Dropping invalid packet! Received with magic key 'v1.00', but data size exceeds 1024 bytes limit. Possible protocol version mismatch.\n");
                }

                // Advance
                offset += sizeof(ExternalMessageV1);
                remaining -= sizeof(ExternalMessageV1);
            }
            else
            {
                // Incomplete V1 message. Drop remaining buffer as we assume no fragmentation.
                fprintf(stderr, "Incomplete V1 message. Dropping remaining buffer.\n");
                break;
            }
        }
        // Check for CANFD Protocol ("canf")
        else if (strncmp((const char*)ptr, "canf", 4) == 0)
        {
            if (remaining >= sizeof(ExternalCanfdMessage))
            {
                const ExternalCanfdMessage* msg = (const ExternalCanfdMessage*)ptr;

                // Forward raw CAN FD frame to the main listener
                // Note: The listener expects raw bytes, so we send the struct canfd_frame directly.
                m_targetListener.onDataReceived((const uint8_t*)&msg->frame, sizeof(struct canfd_frame));

                // Advance
                offset += sizeof(ExternalCanfdMessage);
                remaining -= sizeof(ExternalCanfdMessage);
            }
            else
            {
                // Incomplete CANFD message. Drop remaining buffer.
                fprintf(stderr, "Incomplete CANFD message. Dropping remaining buffer.\n");
                break;
            }
        }
        else
        {
            // Unknown magic key. Drop remaining buffer.
            fprintf(stderr, "Unknown magic key. Dropping remaining buffer.\n");
            break;
        }
    }

    if (remaining > 0 && remaining < 4)
    {
        fprintf(stderr, "Buffer too small for magic key (%zu bytes). Dropping.\n", remaining);
    }
}

} // namespace communication
