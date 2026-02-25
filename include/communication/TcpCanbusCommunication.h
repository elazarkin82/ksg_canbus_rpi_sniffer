#ifndef TCP_CANBUS_COMMUNICATION_H
#define TCP_CANBUS_COMMUNICATION_H

#include "communication/TcpCommunication.h"
#include <string.h>
#include <linux/can.h>
#include <linux/can/raw.h>

namespace communication
{

// --- Protocol Structures ---

/**
 * @brief Defines a fixed-size protocol message for communication (V1).
 *
 * This structure is designed for simplicity and stability over efficiency.
 * It uses a large, fixed-size payload buffer (1024 bytes) to ensure that
 * each message sent over the network has a consistent size.
 *
 * TODO: Future Optimization
 * In a production environment with high-frequency data, a more dynamic
 * two-stage read (header then payload) might be more efficient.
 */
#pragma pack(push, 1)
struct ExternalMessageV1
{
    char magic_key[4];          // "v1.00"
    uint32_t command;           // Command ID
    char pad[128];              // Reserved for future use
    uint32_t data_size;         // Size of the following data payload
    uint8_t data[1024];         // Fixed size payload buffer
};

/**
 * @brief Defines a lightweight protocol message for CAN FD frames.
 *
 * This structure is optimized for high-frequency CAN FD traffic.
 * It contains only the magic key and the raw CAN FD frame.
 */
struct ExternalCanfdMessage
{
    char magic_key[4];          // "canf"
    struct canfd_frame frame;   // Raw CAN FD frame (72 bytes)
};
#pragma pack(pop)

// Command IDs (for V1 protocol)
static const uint32_t CMD_CANBUS_DATA = 0x1001;      // Normal traffic (Sniffer)
static const uint32_t CMD_CANBUS_TO_SYSTEM = 0x1002; // Inject to System CAN
static const uint32_t CMD_CANBUS_TO_CAR = 0x1003;    // Inject to Car CAN

// Callback for non-CAN commands
class ICommandListener
{
public:
    virtual ~ICommandListener() {}
    virtual void onCommandReceived(uint32_t command, const uint8_t* data, size_t length) = 0;
};

class TcpCanbusCommunication : public TcpCommunication, public base::ICommunicationListener
{
public:
    /**
     * @param listener The listener for CANBUS_DATA events (the "normal" traffic).
     * @param ip The server IP address.
     * @param port The server port.
     */
    TcpCanbusCommunication(base::ICommunicationListener& listener, const std::string& ip, uint16_t port);

    virtual ~TcpCanbusCommunication();

    /**
     * Sets the listener for non-CAN commands (e.g. configuration, injection).
     */
    void setCommandListener(ICommandListener* commandListener);

protected:
    // --- ICommunicationListener Implementation (Internal) ---
    // This class listens to its own base TcpCommunication to parse the protocol.
    virtual void onDataReceived(const uint8_t* data, size_t length) override;
    virtual void onError(int32_t errorCode) override;

private:
    void processBuffer(const uint8_t* data, size_t length);

    base::ICommunicationListener& m_targetListener; // The external listener for CAN data
    ICommandListener* m_commandListener;            // Optional listener for other commands
};

} // namespace communication

#endif // TCP_CANBUS_COMMUNICATION_H
