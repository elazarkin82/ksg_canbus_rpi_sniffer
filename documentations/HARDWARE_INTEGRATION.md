# Hardware Integration & Resilience

## 1. Dynamic CAN Interface Management

The system is designed to handle unstable hardware connections gracefully, particularly USB-based CAN adapters (e.g., Canable2), while maintaining full backward compatibility with simple virtual interfaces (e.g., `vcan0`).

### 1.1 Interface Definition (`sniffer.prop`)
The configuration file supports defining CAN interfaces with optional hardware mapping using the `#` delimiter (chosen to avoid conflicts with USB path colons like `usb-0:1.1:1.0`):

**Format:**
`can_interface_name#optional_usb_uniq_name#optional_led_name`

*   `can_interface_name`: The virtual or physical interface name (e.g., `vcan0`, `can0`).
*   `optional_usb_uniq_name` (Optional): A unique identifier for the USB device (e.g., `usb-0:1.1:1.0`). If present, the system employs a hardware Watchdog.
*   `optional_led_name` (Optional): The system LED name (e.g., `ACT`, `PWR` on a Raspberry Pi) to use for visual status indication.

**Backward Compatibility:**
Old formats are fully supported and are implicitly translated:
*   `vcan0` is treated as `vcan0##` (No USB Watchdog, No LED).
*   `can0#usb-0:1.1:1.0` is treated as `can0#usb-0:1.1:1.0#` (USB Watchdog active, No LED).

### 1.2 Component Responsibilities & Locations
*   **`src/core/MainService.cpp`:**
    *   **Configuration Parsing:** Responsible for parsing the `#` delimited strings from the properties file into a structured format.
    *   **Watchdog Thread (`UsbDeviceWatchdog`):** A dedicated thread running inside `MainService`. It periodically scans the OS for devices matching `usb_uniq_name`.
    *   **Interface Lifecycle:** When the Watchdog detects a connection, it brings the interface up, creates the `ObdCanbusCommunication` object, and injects it into the `Sniffer`. On disconnect, it destroys the object and notifies the `Sniffer`.
*   **`src/sniffer/Sniffer.cpp`:**
    *   **Decoupled Operation:** The Sniffer runs continuously regardless of interface status. It exposes setters (e.g., `setSystemInterface()`) allowing `MainService` to hot-swap communication channels.
    *   **Status Reporting:** Sends periodic `CMD_CANBUS_DATA` (Keep-Alive) messages. The payload of this message includes a structural representation of the current connection status of both interfaces, allowing the External Server GUI to update accordingly.

## 2. LED Status Indication

To provide immediate visual feedback that the service is running and to indicate the health of the hardware, the system uses the `led_utils` module.

### 2.1 Component Responsibilities & Locations
*   **`include/utils/led_utils.h` & `src/utils/led_utils.c` (New):** C-style implementation performing low-level file I/O on `/sys/class/leds/<led_name>/`. Exposes functions for setting specific timer triggers (ON/OFF durations).
*   **`src/core/MainService.cpp`:** Orchestrates the LEDs based on the Watchdog status and overall service health.

### 2.2 LED Patterns
If an `optional_led_name` is provided, the system enforces the following heartbeat and error patterns via the Linux `timer` trigger:

1.  **Healthy / Normal Operation:**
    *   *Pattern:* 1000ms ON / 1000ms OFF.
    *   *Meaning:* The `MainService` is running, the `Sniffer` is active, and the defined CAN interface is connected and functioning.
2.  **Error Type 1 (Interface Disconnected):**
    *   *Pattern:* Short ON (e.g., 200ms) / Long OFF (1000ms).
    *   *Meaning:* The service is running, but the Watchdog detected that the specific USB CAN device is physically disconnected or unavailable.
3.  **Error Type 2 (System Fault / Initialization Error):**
    *   *Pattern:* Short OFF (e.g., 200ms) / Long ON (1000ms).
    *   *Meaning:* The service is running, but encountered a severe issue (e.g., failure to bring up the network interface even when physically connected).