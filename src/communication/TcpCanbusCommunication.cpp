#include "communication/TcpCanbusCommunication.h"
#include <string.h>
#include <stdio.h>

namespace communication
{

TcpCanbusCommunication::TcpCanbusCommunication(base::ICommunicationListener& listener, const char* ip, uint16_t port)
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
#ifdef DEBUG
    printf("[TcpCanbus] Received %zu bytes\n", length);
#endif

    if (length < 4)
    {
        fprintf(stderr, "Buffer too small for magic key (%zu bytes). Dropping.\n", length);
        return;
    }

    // Check for V1 Protocol ("v1.00")
    // Note: Magic key is now 8 bytes, but we check first 5 to be safe
    if (strncmp((const char*)data, "v1.00", 5) == 0)
    {
        if (length <= sizeof(ExternalMessageV1))
        {
            const size_t headerSize = calculateExternalMessageV1Size(0);

            if (length >= headerSize)
            {
                const ExternalMessageV1* msg = (const ExternalMessageV1*)data;

                if (msg->data_size <= sizeof(ExternalMessageV1::data) && length >= calculateExternalMessageV1Size(msg->data_size))
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
                    fprintf(stderr, "Dropping invalid packet! Data size %u exceeds limit or buffer incomplete.\n", msg->data_size);
                }
            }
            else
            {
                fprintf(stderr, "Incomplete V1 header. Got %zu, expected at least %zu. Dropping buffer.\n", length, headerSize);
            }
        }
        else
        {
            fprintf(stderr, "Buffer larger than max V1 message. Got %zu. Dropping buffer.\n", length);
        }
    }
    // Check for CANFD Protocol ("canf")
    else if (strncmp((const char*)data, "canf", 4) == 0)
    {
        if (length >= sizeof(ExternalCanfdMessage))
        {
            const ExternalCanfdMessage* msg = (const ExternalCanfdMessage*)data;

            // Forward raw CAN FD frame to the main listener
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
#ifdef DEBUG
        printf("Magic: %02X %02X %02X %02X\n", data[0], data[1], data[2], data[3]);
#endif
    }
}

} // namespace communication
