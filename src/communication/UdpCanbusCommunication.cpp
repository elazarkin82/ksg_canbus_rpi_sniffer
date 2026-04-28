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

void UdpCanbusCommunication::onStatusChanged(base::CommunicationStatus status)
{
    m_targetListener.onStatusChanged(status);
}

int32_t UdpCanbusCommunication::write(const uint8_t* data, size_t length)
{
#ifdef DEBUG_MSG
    char ip[64] = {0};
    uint16_t port = getRemotePort();
    const char* remoteIp = getRemoteIp();

    if (remoteIp && remoteIp[0] != '\0')
    {
        strncpy(ip, remoteIp, sizeof(ip)-1);
    }
    else
    {
        // Dynamic mode, get last sender
        getLastSenderInfo(ip, sizeof(ip), &port);
    }

    if (length >= 5 && strncmp((const char*)data, "v1.00", 5) == 0)
    {
        const ExternalMessageV1* msg = (const ExternalMessageV1*)data;
        fprintf(stderr, "[UDP TX] To: %s:%d, Command: 0x%X\n", ip, port, msg->command);
    }
    else
    {
        fprintf(stderr, "[UDP TX] To: %s:%d, Unknown/Raw Data, Size: %zu\n", ip, port, length);
    }
#endif

    return UdpCommunication::write(data, length);
}

void UdpCanbusCommunication::processPacket(const uint8_t* data, size_t length)
{
#ifdef DEBUG
    printf("[UdpCanbus] Received %zu bytes\n", length);
#endif

#ifdef DEBUG_MSG
    char ip[64] = {0};
    uint16_t port = 0;
    getLastSenderInfo(ip, sizeof(ip), &port);

    if (length >= 5 && strncmp((const char*)data, "v1.00", 5) == 0)
    {
        const ExternalMessageV1* msg = (const ExternalMessageV1*)data;
        fprintf(stderr, "[UDP RX] From: %s:%d, Command: 0x%X\n", ip, port, msg->command);
    }
    else
    {
        fprintf(stderr, "[UDP RX] From: %s:%d, Unknown/Raw Data, Size: %zu\n", ip, port, length);
    }
#endif

    if (length < 4)
    {
        fprintf(stderr, "UDP Packet too small for magic key (%zu bytes). Dropping.\n", length);
        return;
    }

    // Check for V1 Protocol ("v1.00")
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
                        m_targetListener.onDataReceived(msg->data, msg->data_size);
                    }
                    else if (m_commandListener)
                    {
                        m_commandListener->onCommandReceived(msg->command, msg->data, msg->data_size);
                    }
                }
                else
                {
                    fprintf(stderr, "Dropping invalid packet! Data size %u exceeds limit or buffer incomplete.\n", msg->data_size);
                }
            }
            else
            {
                fprintf(stderr, "Incomplete V1 header. Got %zu, expected at least %zu. Dropping packet.\n", length, headerSize);
            }
        }
        else
        {
            fprintf(stderr, "Buffer larger than max V1 message. Got %zu. Dropping packet.\n", length);
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
#ifdef DEBUG
        printf("Magic: %02X %02X %02X %02X\n", data[0], data[1], data[2], data[3]);
#endif
    }
}

} // namespace communication