# External Server Specification for RpiCanbusSniffer

## 1. Overview
The External Server is a Python-based control station designed to interact with the `RpiCanbusSniffer` running on a Raspberry Pi. It provides a graphical user interface (GUI) for monitoring CAN bus traffic, performing reverse engineering tasks, and actively controlling vehicle systems via CAN injection and filtering rules.

## 2. Architecture
*   **Language:** Python 3 (using `tkinter` or `PyQt` for GUI).
*   **Communication:** UDP over Ethernet/WiFi.
*   **Backend:** A C++ shared library (`libudp_client.so`) handles the low-level protocol serialization/deserialization to ensure performance and compatibility with the Sniffer's C++ codebase.
*   **Data Storage:** JSON files for saving/loading vehicle profiles (decoding rules and control mappings).

## 3. Protocol Specification
The communication relies on the `ExternalMessageV1` structure defined in `CanbusProtocol.h`.

### 3.1 Message Structure
*   **Header:** Magic Key (8 bytes), Command ID (4 bytes), Padding, Data Size.
*   **Payload:** Up to 64KB of data.

### 3.2 Commands
*   `CMD_CANBUS_DATA (0x1001)`: Keep-alive / Data injection.
*   `CMD_CANBUS_TO_SYSTEM (0x1002)`: Inject frame to Car System bus.
*   `CMD_CANBUS_TO_CAR (0x1003)`: Inject frame to Car Computer (ECU) bus.
*   `CMD_EXTERNAL_SERVICE_LOGGING_ON (0x1004)`: Enable traffic mirroring to UDP.
*   `CMD_EXTERNAL_SERVICE_LOGGING_OFF (0x1005)`: Disable traffic mirroring.
*   `CMD_SET_FILTERS (0x1006)`: Bulk update of all filtering/modification rules.

### 3.3 Filter Rules (`CanFilterRule`)
*   **Match:** CAN ID, Data Index, Data Value, Data Mask.
*   **Action:** PASS, DROP, MODIFY.
*   **Target:** TO_SYSTEM, TO_CAR, BOTH.
*   **Modification:** Mask and Data for bitwise replacement.

## 4. GUI Modules

### 4.1 Dashboard & Connection
*   **Connection Control:** Connect/Disconnect buttons.
*   **Status Indicators:** Connection state, Keep-alive heartbeat.
*   **Panic Button:** Immediately clears all filters and stops injection (Reset to Default).
*   **Settings Menu:**
    *   Remote IP/Port (RPi).
    *   Local IP/Port.
    *   CAN Interface names configuration.

### 4.2 Reverse Engineering Suite
*   **Recorder:** Start/Stop recording of live CAN traffic.
*   **Traffic Table:**
    *   Columns: Timestamp, Direction, ID, DLC, Data (Hex), Decoded Value.
    *   Grouping: Option to group by ID (show last message only).
*   **Filtering:**
    *   Filter by ID (single or range).
    *   Filter by Data content (byte index/value).
*   **Decoder Engine:**
    *   Select a message ID to configure decoding.
    *   **Data Types:**
        *   Integer (signed/unsigned, big/little endian)
        *   Boolean (bit specific)
        *   Percent (scaled integer)
        *   Bit Fields
        *   Enum (map values to text)
        *   Scaled Values (Raw * Factor + Offset)
        *   Fixed Point / Float
        *   BCD / ASCII
        *   Counters / CRC
    *   **Custom Formula:** Python-like expression support (e.g., `(d[0] * 256 + d[1]) / 4`).
    *   **Preview:** Live preview of decoded value.

### 4.3 Rules Manager
*   **Rule Table:** List of currently active rules.
*   **Rule Editor:**
    *   Action: DROP / MODIFY / INJECT.
    *   Direction: System / Computer.
    *   Condition: ID, Byte Index, Value.
    *   Modification: Target Byte, New Value.
*   **Bulk Operations:** "Apply Rules" (sends `CMD_SET_FILTERS`), "Clear All".

### 4.4 Virtual Cockpit (Control Screen)
*   **Visuals:**
    *   **Steering Wheel:** Rotates based on value.
    *   **Pedals:** Progress bars for Throttle and Brake.
    *   **Switches:** Toggle buttons for Wipers, Lights, etc.
*   **Mapping:**
    *   Link each visual control to a specific CAN ID and decoding rule defined in the Reverse Engineering suite.
*   **Keyboard Control:**
    *   `Left/Right Arrow`: Steer left/right.
    *   `Shift + Arrow`: Fast steering.
    *   `Up Arrow`: Increase Throttle.
    *   `Down Arrow`: Brake.
    *   `W`: Toggle Wipers.
*   **Logic:** Generates CAN frames based on input and sends them via `INJECT` or updates `MODIFY` rules.

## 5. Data Persistence
*   **Profiles:** Save all settings to a JSON file.
    *   Decoders (ID -> Name, Type, Formula).
    *   Control Mappings (Control -> ID, Signal Name).
    *   Connection Settings.
*   **Load:** Load a previously saved profile to instantly configure the workspace for a specific vehicle.

## 6. Code Structure (Python)
*   `main.py`: Entry point, GUI initialization.
*   `backend/`:
    *   `udp_client.py`: Wrapper around `libudp_client.so`.
    *   `protocol.py`: Python definitions of C structures (`ctypes`).
*   `gui/`:
    *   `dashboard.py`: Main window and connection bar.
    *   `reverse_engineering.py`: Traffic table and decoder dialogs.
    *   `rules_manager.py`: Filter configuration.
    *   `cockpit.py`: Visual controls and keyboard handling.
*   `logic/`:
    *   `decoder.py`: Implementation of data type parsing and formulas.
    *   `profile_manager.py`: JSON save/load logic.
