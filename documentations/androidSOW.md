# Detailed Statement of Work (SOW) - Android External Server

## 1. Project Mission & Objectives
The Android External Server is a professional-grade mobile station designed to interface with the `RpiCanbusSniffer`. It provides high-performance CAN bus monitoring, reverse engineering tools, and active intervention capabilities. 

**Key Objectives:**
*   **Performance:** Sustained processing of 2000+ CAN messages per second via NDK.
*   **Compatibility:** 100% parity with Python External Server data structures (CSV logs, JSON profiles).
*   **Safety:** Real-time "Panic" mechanism to restore vehicle passthrough mode.
*   **Portability:** Mobile-first UX utilizing touch gestures and gyroscopic sensors for vehicle control.

---

## 2. Technical Architecture Specification

### 2.1 Native Core Engine (C-Style C++ / NDK)
*   **Logic:** Implementation of the low-level communication protocol in C++ to ensure deterministic latency.
*   **Networking:** Persistent UDP/TCP Client managing `ExternalMessageV1` packets.
*   **Data Flow:** 
    *   **RX Thread:** High-speed listener for incoming CAN frames.
    *   **Command Thread:** Serialization of outgoing control commands and filter rules.
    *   **Logging Thread:** Synchronous file I/O for CSV recording to prevent main-thread jank.
*   **JNI Interface:** Optimized data marshaling using direct byte buffers or primitive arrays to minimize garbage collection overhead.

### 2.2 Application Framework (Android / Java)
*   **Lifecycle Management:** A `Foreground Service` maintains the network heartbeat even when the screen is off or the user switches apps.
*   **Notification Integration:** Sticky notification showing real-time status (Connected/Recording) and a "Stop" action button.
*   **Threading Model:**
    *   **UI Thread:** Strictly for rendering and user interaction.
    *   **Service Thread:** Handles the JNI bridge and logic coordination.
    *   **Worker Threads:** Used for JSON profile parsing and local storage operations.

---

## 3. Functional Requirements - Detailed Module Specification

### 3.1 Connection & System Status (Primary Entry)
*   **User Interface:**
    *   **Connection Pane:** Input fields for Sniffer IP, Remote Port, and Local Port with persistent storage (SharedPreferences).
    *   **Real-Time Metrics:** Live display of RX/TX Bitrate (msgs/sec), Packet Loss %, and RTT (Round Trip Time).
    *   **Status Console:** A scrollable text area (Terminal-style) displaying low-level system events (e.g., "Socket Bound", "JNI Library Loaded", "Handshake Successful").
*   **Panic Mechanism:** A persistent RED button in the Toolbar. Sending `CMD_SET_FILTERS` with an empty rule set to restore transparency.

### 3.2 CAN Sniffer (The Reverse Engineering Hub)
*   **The Grouped Table:**
    *   **Grouping Logic:** By default, messages are grouped by `CAN_ID` (and `PID` if applicable). Each row represents a unique signal source.
    *   **Columns:** ID, Protocol, PID, Direction, Counter (total messages for this ID), Interval (ms since last message), Data (Hex), Decoded Value.
    *   **Visual Feedback:** Byte highlighting (colors) for changed values within the data stream.
*   **Interaction Model:**
    *   **Long-Press (Context Menu):** Replicates Python's right-click. Menu items:
        *   **Protocol Configuration:** Select Manual/OBD2/UDS/J1939. Toggle "Has PID" and set "PID Index".
        *   **Common Presets:** Apply standard formulas (e.g., Engine RPM, Speed).
        *   **Control Mapping:** Map the ID to Virtual Cockpit inputs (Steering, Throttle, etc.).
    *   **Double-Click:** Opens the contextual **Decoder Editor Dialog** for the selected ID/PID.
*   **Recording:** Floating Action Button (FAB) toggles real-time CSV logging.

### 3.3 Records Management & Library
*   **The Recording Workflow:**
    1.  User starts recording from the Sniffer.
    2.  On Stop: A mandatory dialog forces the user to provide a filename.
    3.  File format: `CSV` with headers `timestamp, time_ms, direction, can_id, data`.
*   **Records Library:**
    *   Management of all local `.csv` files.
    *   **Sharing Sheet:** Native Android sharing to WhatsApp, Telegram, Email, or Cloud (Google Drive).

### 3.4 Decoder Editor (Contextual Dialog)
*   **Access Point:** Triggered from the Sniffer table.
*   **Sections:**
    1.  **Header:** Displays current CAN ID and PID.
    2.  **Signal Params:** Input fields for Name, Start Bit, Length, Factor, Offset, and Endianness.
    3.  **Reverse Engineering Tool:** 
        *   Dropdown list of presets (RPM, Temp, etc.).
        *   Manual Formula field (supporting standard math: `x[0]*256 + x[1]`).
    4.  **Live Traffic Preview:** A mini-table at the bottom showing the last 10-20 raw messages for *this specific ID* to assist in real-time formula verification.

### 3.5 OBD2 Scanner (Active Diagnostic Tool)
*   **Polling Logic:**
    *   User selects PIDs from a predefined list.
    *   "Start Polling" button triggers periodic `CMD_CANBUS_TO_CAR` requests.
    *   User-defined interval (e.g., 100ms, 1000ms).
*   **Display:** A grid view showing the status and decoded value of each active PID.

### 3.6 Virtual Cockpit (Control Module)
*   **Sensors:** Gyro-Steering (tilt phone to steer). Vertical sliders for Throttle/Brake.
*   **Mapping Logic:** Converts sensor values into CAN frames using the "Reverse Formula" defined in the profile.

---

## 4. Connectivity Engine Details

### 4.1 Connection Lifecycle & Logic
The connectivity engine mimics the Python `UdpClient` and the `sniffer_client` library.

*   **Default Configuration:**
    *   **Sniffer IP:** `192.168.4.1` (Default for RPi Access Point).
    *   **Sniffer Port:** `9095`.
    *   **Local Port:** `9096`.
*   **Handshake & Heartbeat:**
    *   Upon "Connect", the app starts a background thread that sends `CMD_KEEPALIVE` (0x1001) every 500ms.
    *   **Handshake:** Connection is considered "Pending" until the first response from the Sniffer is received.
    *   **Timeout:** If no response is received for 3000ms, the UI switches to "Disconnected/Reconnecting".
*   **Metadata Integration:**
    *   The Sniffer includes system metadata in its Keep-Alive response payload.
    *   **Metadata Fields:** CPU Temperature, System Load, Uptime, Memory usage.
    *   **UI Display:** Decoded metadata is printed to the **Status Console** in the `ConnectionFragment` in real-time.

### 4.2 Native Implementation
*   **Sniffer Client Library:** The Android NDK builds the `sniffer_client.cpp` as a shared library.
*   **Thread Safety:** Native mutexes protect the RX queue and the status buffers.
*   **Data Alignment:** Uses `#pragma pack(push, 1)` for all shared structures between Java, C++, and the RPi.

---

## 5. Non-Functional Requirements
*   **Latency:** End-to-end (Sensor to Bus) must be <30ms.
*   **Safety:** Watchdog timer on the RPi resets filters if the Android Heartbeat is lost for >1000ms.
*   **Power:** Background service must optimize CPU usage when no traffic is being recorded.
