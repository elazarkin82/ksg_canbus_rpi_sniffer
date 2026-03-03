#ifndef SNIFFER_CLIENT_H
#define SNIFFER_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "communication/CanbusProtocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Lifecycle ---

/**
 * Creates a new Sniffer Client instance.
 * @param ip The IP address of the Sniffer (RPi).
 * @param remote_port The port the Sniffer is listening on.
 * @param local_port The local port to bind to (for receiving logs).
 * @param keep_alive_interval_ms Interval for automatic keep-alive messages (0 to disable).
 * @return A handle to the client instance, or NULL on failure.
 */
void* client_create(const char* ip, uint16_t remote_port, uint16_t local_port, uint32_t keep_alive_interval_ms);

/**
 * Starts the client threads (Keep-Alive and RX).
 * @param handle The client handle.
 * @return 0 on success, <0 on error.
 */
int client_start(void* handle);

/**
 * Stops the client threads and sends a reset command.
 * @param handle The client handle.
 */
void client_stop(void* handle);

/**
 * Destroys the client instance and frees memory.
 * @param handle The client handle.
 */
void client_destroy(void* handle);

// --- Commands ---

/**
 * Enables or disables logging on the Sniffer.
 */
void client_send_log_enable(void* handle, bool enable);

/**
 * Injects a CAN frame.
 * @param target 1=TO_SYSTEM, 2=TO_CAR.
 */
void client_send_injection(void* handle, uint8_t target, uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * Sets the filter rules.
 * @param rules Array of CanFilterRule structures.
 * @param count Number of rules.
 */
void client_set_filters(void* handle, const communication::CanFilterRule* rules, size_t count);

/**
 * Sends a raw command.
 */
void client_send_raw_command(void* handle, uint32_t command_id, const uint8_t* payload, size_t len);

// --- Receiving ---

/**
 * Reads a message from the receive queue.
 * @param out_msg Pointer to a buffer to store the message (ExternalCanfdMessage).
 * @param timeout_ms Timeout in milliseconds.
 * @return 1 if message received, 0 on timeout, <0 on error.
 */
int client_read_message(void* handle, communication::ExternalCanfdMessage* out_msg, int timeout_ms);

// --- Helpers ---

/**
 * Helper to create a filter rule structure.
 */
communication::CanFilterRule client_create_rule(uint32_t id, uint8_t action, uint8_t target,
                                                uint8_t data_index, uint8_t data_value, uint8_t data_mask);

#ifdef __cplusplus
}
#endif

#endif // SNIFFER_CLIENT_H
