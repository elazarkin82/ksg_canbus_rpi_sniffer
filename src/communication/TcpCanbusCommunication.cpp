#include "communication/TcpCanbusCommunication.h"
#include <string.h>
#include <stdio.h>

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
    if (length < 4)
    {
        fprintf(stderr, "Buffer too small for magic key (%zu bytes). Dropping.\n", length);
        return;
    }

    // Check for V1 Protocol ("v1.00")
    if (strncmp((const char*)data, "v1.00", 4) == 0)
    {
        if (length >= sizeof(ExternalMessageV1))
        {
            const ExternalMessageV1* msg = (const ExternalMessageV1*)data;

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
        }
        else
        {
            fprintf(stderr, "Incomplete V1 message. Dropping buffer.\n");
        }
    }
    // Check for CANFD Protocol ("canf")
    else if (strncmp((const char*)data, "canf", 4) == 0)
    {
        if (length >= sizeof(ExternalCanfdMessage))
        {
            const ExternalCanfdMessage* msg = (const ExternalCanfdMessage*)data;

            // Forward raw CAN FD frame to the main listener
            // Note: The listener expects raw bytes, so we send the struct canfd_frame directly.
            m_targetListener.onDataReceived((const uint8_t*)&msg->frame, sizeof(struct canfd_frame));
        }
        else
        {
            fprintf(stderr, "Incomplete CANFD message. Dropping buffer.\n");
        }
    }
    else
    {
        fprintf(stderr, "Unknown magic key. Dropping buffer.\n");
    }
}

} // namespace communication
