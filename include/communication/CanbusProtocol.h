#ifndef CANBUS_PROTOCOL_H
#define CANBUS_PROTOCOL_H

#include <stdint.h>
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
static const uint32_t CMD_EXTERNAL_SERVICE_LOGGING_ON = 0x1004;
static const uint32_t CMD_EXTERNAL_SERVICE_LOGGING_OFF = 0x1005;

/**
 * @brief Interface for handling non-standard CAN commands.
 *
 * This listener is used to receive control commands, configuration updates,
 * and injection requests (e.g., sending a specific CAN frame to the car or system).
 * Unlike the main ICommunicationListener which handles the high-frequency stream
 * of sniffed CAN data, this interface is intended for lower-frequency management tasks.
 */
class ICommandListener
{
public:
    virtual ~ICommandListener() {}

    /**
     * Called when a command message is received.
     * @param command The command ID (e.g., CMD_CANBUS_TO_SYSTEM).
     * @param data Pointer to the command payload.
     * @param length Size of the payload.
     */
    virtual void onCommandReceived(uint32_t command, const uint8_t* data, size_t length) = 0;
};

} // namespace communication

#endif // CANBUS_PROTOCOL_H
