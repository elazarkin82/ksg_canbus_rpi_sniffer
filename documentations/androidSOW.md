# Comprehensive Statement of Work (SOW) - Android External Server

## 1. Project Overview
The Android External Server is a professional mobile application designed to act as a remote control and diagnostic hub for the `RpiCanbusSniffer`. It provides real-time CAN bus monitoring, high-performance data recording, and active signal injection. The application maintains 100% data compatibility with the existing Python backend while leveraging mobile-native capabilities like touch gestures, gyroscopic sensors, and system-level sharing.

---

## 2. Technical Architecture & Constraints

### 2.1 Native Core Engine (C++/NDK)
*   **Implementation:** C-style C++ for deterministic performance.
*   **Networking:** Persistent UDP/TCP client utilizing native sockets to handle `ExternalMessageV1` packets.
*   **Throughput:** Must support sustained traffic of 2000+ messages per second.
*   **JNI Bridge:** Optimized data marshaling to minimize overhead when passing CAN frames to the Java layer.

### 2.2 Application Framework
*   **Background Resilience:** A `Foreground Service` ensures the UDP connection and recording remain active when the app is in the background.
*   **Connectivity Management:** Auto-reconnect with exponential backoff and a "Keep-Alive" (Heartbeat) mechanism.

---

## 3. Functional Requirements - Screen by Screen

### 3.1 Connection & Status (Primary Screen)
*   **Purpose:** The main entry point for establishing communication and monitoring system health.
*   **Connection Controls:** 
    *   Fields for Sniffer IP, Remote Port, and Local Port.
    *   Connect/Disconnect toggle button.
*   **Status Indicators:** Real-time Round Trip Time (RTT) and Connection State (LED).
*   **Status Console:** A scrollable text area displaying system-level logs (similar to Python's `SnifferStatusWindow`).
*   **Panic Button:** High-priority Red button to instantly send a "Clear All Filters" command to the RPi.

### 3.2 CAN Sniffer (Live Monitor)
*   **Live Traffic Table:** Columns: Timestamp, ID (Hex), Protocol, PID, DLC, Data (Hex), Direction.
*   **Navigation to Decoder:** Clicking/Double-clicking a message entry opens the **Decoder Editor** specifically for that CAN ID.
*   **Display Logic:**
    *   **ID Grouping:** Collapses traffic by ID, showing the most recent frame.
    *   **Change Tracking:** Individual bytes highlight when their value changes.
*   **Recording FAB:** A Floating Action Button to toggle the recording state.

### 3.3 Records Management & Library
*   **Naming Protocol:** Upon stopping a recording, a mandatory Modal Dialog appears for custom filename entry (Default: `CAN_LOG_YYYY_MM_DD_HHMM.csv`).
*   **Sharing Interface:** Integration with Android Native Share Sheet (WhatsApp, Telegram, Email) via `FileProvider`.

### 3.4 Decoder Editor (Contextual Dialog)
*   **Access:** Opened only from the CAN Sniffer.
*   **Protocol Settings:**
    *   Protocol selection (Manual, OBD2, UDS, ISO-TP, J1939).
    *   **Has PID Toggle:** Enable/Disable PID tracking for the selected ID.
    *   **PID Index:** Specify which byte (0-7) contains the PID.
*   **Signal Mapping:** Define Start Bit, Length, Factor, Offset, and Endianness.
*   **Integrity Checks:** CRC/Checksum validation and Rolling Counter tracking.

### 3.5 OBD2 Scanner (Active Diagnostics)
*   **Selection:** Checkbox list of standard OBD2 PIDs.
*   **Polling Control:** 
    *   **Start/Stop Polling:** Button to begin periodic queries for the selected items.
    *   **Interval Setting:** Configurable polling frequency (in ms/seconds).
*   **Visual Grid:** Real-time display of ECU reported values for active PIDs.

### 3.6 Virtual Cockpit (Active Control)
*   **Sensory Controls:** Gyro-Steering (tilt) and vertical touch sliders for Throttle/Brake.
*   **UI Controls:** Toggle buttons for Lights, Wipers, and Indicators.

### 3.7 Rules Manager
*   **Interface:** Create and manage `PASS`, `DROP`, and `MODIFY` rules.
*   **Sync:** Upload current filter set to the RPi.

### 3.8 Remote Settings (RPi Internal)
*   **Mechanism:** Dedicated screen for RPi internal parameters.
*   **GET:** Query the RPi for its current active hardware configuration (Interface, Bitrate, etc.).
*   **SET:** Apply and save new hardware configurations to the RPi.

### 3.9 Local Settings (App Side)
*   **UI/UX:** Theme selection, WakeLock toggle, and Sensor calibration.

---

## 4. Non-Functional & Safety Requirements
*   **Latency:** End-to-end latency <50ms.
*   **Safety Default:** On app disconnect, the RPi must return to "Passthrough" state.
*   **Compatibility:** All files (CSV/JSON) must be interchangeable with the Python version.
