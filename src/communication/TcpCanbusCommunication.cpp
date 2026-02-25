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
    const ExternalCanbusMessageHeader* header = nullptr;
    const size_t packetSize = sizeof(ExternalCanbusMessageHeader);

    while (offset + packetSize <= length)
    {
        header = (const ExternalCanbusMessageHeader*)(data + offset);

        // Validate Magic Key
        if (strncmp(header->magic_key, "KSG1", 4) == 0)
        {
            // Validate data size
            if (header->data_size <= 1024)
            {
                if (header->command == CMD_CANBUS_DATA)
                {
                    // Forward CAN data to the main listener
                    m_targetListener.onDataReceived(header->data, header->data_size);
                }
                else if (m_commandListener)
                {
                    // Forward other commands to the command listener
                    m_commandListener->onCommandReceived(header->command, header->data, header->data_size);
                }
                else
                {
                    // TODO: Handle unhandled commands internally or log
                }
            }
        }
        else
        {
            // Magic key mismatch. For robustness, we could scan byte-by-byte for the next key.
            // For simplicity now, we just advance by one byte.
            offset++;
            continue;
        }

        // Advance to the next packet
        offset += packetSize;
    }

    // Note: Any remaining bytes (less than a full packet) are discarded,
    // based on the assumption of no fragmentation.
}

} // namespace communication
