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
 * It uses a large, fixed-size payload buffer (64000 bytes) to ensure that
 * each message sent over the network has a consistent size.
 */
#pragma pack(push, 1)
struct ExternalMessageV1
{
    char magic_key[8];          // "v1.00" (8 bytes)
    uint32_t command;           // Command ID
    char pad[128];              // Reserved for future use
    uint32_t data_size;         // Size of the following data payload
    uint8_t data[64000];        // Fixed size payload buffer (64000 bytes)
};

static inline size_t calculateExternalMessageV1Size(uint32_t dataSize)
{
    return sizeof(ExternalMessageV1) - sizeof(ExternalMessageV1::data) + dataSize;
}

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

/**
 * @brief Defines a filtering rule for the Sniffer service.
 *
 * This structure is used to configure the internal filtering engine.
 * Multiple rules can be sent in a single UDP packet (CMD_SET_FILTERS).
 * The engine processes rules for a specific CAN ID in the order they appear.
 *
 * Usage for External Server:
 * 1. Create an array of CanFilterRule structures.
 * 2. Fill each rule with the desired logic:
 *    - To BLOCK a specific ID:
 *      can_id = 0x123; action_type = 1 (DROP); data_mask = 0 (match all);
 *    - To MODIFY a specific byte (e.g., byte 2 to 0xFF):
 *      can_id = 0x123; action_type = 2 (MODIFY);
 *      modification_data[2] = 0xFF; modification_mask[2] = 0xFF;
 *    - To FILTER by content (e.g., only if byte 0 is 0x01):
 *      can_id = 0x123; data_index = 0; data_value = 0x01; data_mask = 0xFF;
 * 3. Send the array as the payload of a CMD_SET_FILTERS command.
 *    The Sniffer will replace its entire rule set with the new one.
 */
struct CanFilterRule
{
    uint32_t can_id;            // The CAN ID to match (0-0x7FF for standard frames)

    // --- Condition (Optional) ---
    // If data_mask is 0, the condition is ignored (matches all frames with this ID).
    // If data_mask is non-zero, the rule applies only if:
    // (frame_data[data_index] & data_mask) == data_value
    uint8_t data_index;         // Byte index to check (0-7)
    uint8_t data_value;         // Expected value after masking
    uint8_t data_mask;          // Mask for the check (0xFF = exact match, 0x00 = ignore)

    // --- Action ---
    // 0 = PASS (Default): Forward the frame as is.
    // 1 = DROP: Block the frame (do not forward).
    // 2 = MODIFY: Modify the frame data before forwarding.
    uint8_t action_type;

    // --- Target Direction ---
    // 0 = BOTH (Default): Apply rule to both directions.
    // 1 = TO_SYSTEM: Apply rule only to frames going TO the car system (from computer).
    // 2 = TO_CAR: Apply rule only to frames going TO the car computer (from system).
    uint8_t target;

    // For MODIFY action:
    // new_data = (old_data & ~modification_mask) | (modification_data & modification_mask)
    // Bits set in modification_mask will be replaced by bits from modification_data.
    uint8_t modification_data[8];
    uint8_t modification_mask[8];
};
#pragma pack(pop)

// Command IDs (for V1 protocol)
static const uint32_t CMD_CANBUS_DATA = 0x1001;      // Normal traffic (Sniffer)
static const uint32_t CMD_CANBUS_TO_SYSTEM = 0x1002; // Inject to System CAN
static const uint32_t CMD_CANBUS_TO_CAR = 0x1003;    // Inject to Car CAN
static const uint32_t CMD_EXTERNAL_SERVICE_LOGGING_ON = 0x1004;
static const uint32_t CMD_EXTERNAL_SERVICE_LOGGING_OFF = 0x1005;
static const uint32_t CMD_SET_FILTERS = 0x1006;      // Set/Replace all filter rules

// Filter Target Constants
static const uint8_t FILTER_TARGET_BOTH = 0;
static const uint8_t FILTER_TARGET_TO_SYSTEM = 1;
static const uint8_t FILTER_TARGET_TO_CAR = 2;

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
