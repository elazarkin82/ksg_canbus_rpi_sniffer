#include "communication/UdpCanbusCommunication.h"
#include <string.h>
#include <stdio.h>

namespace communication
{

UdpCanbusCommunication::UdpCanbusCommunication(base::ICommunicationListener& listener, const char* ip, uint16_t remotePort, uint16_t localPort)
    : UdpCommunication(*this, ip, remotePort, localPort),
      m_targetListener(listener),
      m_commandListener(nullptr)
{
}

UdpCanbusCommunication::~UdpCanbusCommunication()
{
}

void UdpCanbusCommunication::setCommandListener(ICommandListener* commandListener)
{
    m_commandListener = commandListener;
}

void UdpCanbusCommunication::onDataReceived(const uint8_t* data, size_t length)
{
    // UDP preserves message boundaries, so each callback is a full packet
    processPacket(data, length);
}

void UdpCanbusCommunication::onError(int32_t errorCode)
{
    m_targetListener.onError(errorCode);
}

void UdpCanbusCommunication::processPacket(const uint8_t* data, size_t length)
{
    if (length < 4)
    {
        fprintf(stderr, "UDP Packet too small for magic key (%zu bytes). Dropping.\n", length);
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
                    m_targetListener.onDataReceived(msg->data, msg->data_size);
                }
                else if (m_commandListener)
                {
                    m_commandListener->onCommandReceived(msg->command, msg->data, msg->data_size);
                }
            }
            else
            {
                fprintf(stderr, "Dropping invalid packet! Received with magic key 'v1.00', but data size exceeds 1024 bytes limit. Possible protocol version mismatch.\n");
            }
        }
        else
        {
            fprintf(stderr, "Incomplete V1 message. Dropping packet.\n");
        }
    }
    // Check for CANFD Protocol ("canf")
    else if (strncmp((const char*)data, "canf", 4) == 0)
    {
        if (length >= sizeof(ExternalCanfdMessage))
        {
            const ExternalCanfdMessage* msg = (const ExternalCanfdMessage*)data;
            m_targetListener.onDataReceived((const uint8_t*)&msg->frame, sizeof(struct canfd_frame));
        }
        else
        {
            fprintf(stderr, "Incomplete CANFD message. Dropping packet.\n");
        }
    }
    else
    {
        fprintf(stderr, "Unknown magic key. Dropping packet.\n");
    }
}

} // namespace communication
