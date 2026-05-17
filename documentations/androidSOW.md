# Comprehensive Statement of Work (SOW) - Android External Server

## 1. Project Overview
The Android External Server is a professional mobile application designed to act as a remote control and diagnostic hub for the `RpiCanbusSniffer`. It provides real-time CAN bus monitoring, high-performance data recording, and active signal injection. The application must maintain 100% data compatibility with the existing Python backend while leveraging mobile-native capabilities like touch gestures, gyroscopic sensors, and system-level sharing.

---

## 2. Technical Architecture & Constraints

### 2.1 Native Core Engine (C++/NDK)
*   **Implementation:** C-style C++ for deterministic performance.
*   **Networking:** Persistent UDP/TCP client utilizing native sockets to handle `ExternalMessageV1` packets.
*   **Throughput:** Must support sustained traffic of 2000+ messages per second.
*   **Concurrency:** Multi-threaded architecture:
    *   **Net-Thread:** Handles high-speed packet reception and heartbeat logic.
    *   **Disk-Thread:** Dedicated to synchronous CSV writing during recording to avoid UI blocking.
*   **JNI Bridge:** Optimized data marshaling to minimize overhead when passing CAN frames to the Java layer.

### 2.2 Application Framework
*   **Background Resilience:** A `Foreground Service` will be implemented to ensure the UDP connection and recording remain active when the user switches apps or the screen is locked.
*   **Connectivity Management:** Auto-reconnect with exponential backoff and a "Keep-Alive" (Heartbeat) mechanism.
*   **Data Models:** Shared JSON schemas for Decoders and Vehicle Profiles between Android and Python.

---

## 3. Functional Requirements - Screen by Screen

### 3.1 Main Screen (Host Activity)
*   **Navigation:** A `Navigation Drawer` (Side Menu) providing instant access to all modules.
*   **Toolbar (Header):**
    *   **Global Connection LED:** A visual indicator (Red: Disconnected, Yellow: Handshake, Green: Connected).
    *   **Heartbeat Display:** Real-time Round Trip Time (RTT) in milliseconds.
    *   **Panic Button:** A prominent, high-priority Red button to instantly send a "Clear All Filters" command to the RPi.
*   **Content Area:** Fragment-based container for switching between functional modules.

### 3.2 CAN Sniffer (Live Monitor)
*   **Live Traffic Table:** High-speed scrolling list with columns: Timestamp, ID (Hex), DLC, Data (Hex), Direction.
*   **Advanced Display Logic:**
    *   **ID Grouping:** Option to show only the most recent frame for each CAN ID, with a frequency counter.
    *   **Change Tracking:** Individual bytes in the Data column highlight (e.g., flash blue/red) when their value changes.
*   **Quick Filters:** Instant search by ID or hex content.
*   **Recording FAB:** A Floating Action Button to toggle the recording state.

### 3.3 Records Management & Library (Crucial Module)
*   **The Recording Workflow:**
    1.  User starts recording via the Sniffer FAB.
    2.  **Naming Protocol:** Upon clicking "Stop", a mandatory Modal Dialog appears.
    3.  The dialog prompts for a custom filename (Default: `CAN_LOG_YYYY_MM_DD_HHMM.csv`).
    4.  The file is saved only after the name is confirmed.
*   **Record Library Screen:**
    *   **File List:** Displays all saved CSV files with metadata: Date/Time, File Size, and Duration.
    *   **Multi-Select Mode:** Support for long-press selection of multiple files.
    *   **Sharing Interface:**
        *   Integrated "Share" action triggering the **Android Native Share Sheet**.
        *   Seamless sharing to: **WhatsApp** (as document), **Telegram**, **Gmail** (as attachment), and Cloud providers.
        *   Uses `FileProvider` to ensure secure temporary access for external apps.

### 3.4 Virtual Cockpit (Active Control)
*   **Sensory Controls:**
    *   **Gyro-Steering:** Injects CAN frames for steering angle based on device tilt.
    *   **Pedal Sliders:** Smooth vertical touch sliders for Throttle and Braking.
*   **UI Controls:** Large toggle buttons for Lights, Wipers, Indicators, and Horn.
*   **Live Gauges:** Graphical Speedometer and Tachometer decoded from incoming traffic.

### 3.5 Decoder & Rules Manager
*   **Decoder Editor:** GUI to define signal mappings (Start Bit, Length, Factor, Offset, Endianness).
*   **Advanced Data Types:** Support for BCD, ASCII, and Bit-fields.
*   **Integrity Checks:** Implementation of **CRC/Checksum validation** and **Rolling Counter** tracking to match Python logic.
*   **Rules Engine:** Interface to manage `PASS`, `DROP`, and `MODIFY` rules.
*   **Bulk Sync:** A "Sync to RPi" button to upload the current filter set.

### 3.6 OBD2 Scanner (Diagnostic Module)
*   **PID Management:** Interface to select standard OBD2 PIDs (e.g., Engine Temp, Fuel Level, RPM).
*   **Query Engine:** Periodic transmission of OBD2 query frames and parsing of standardized responses.
*   **Visual Dashboard:** Real-time display of ECU reported values in a dedicated diagnostic grid.

### 3.7 Status Window (Diagnostics)
*   **Live Console:** Scrolling log of system events, socket status, and internal errors.
*   **Metrics Dashboard:** Displays RX/TX message rates, Packet loss %, RPi Uptime, and CPU Temp.

### 3.8 Settings (Split Configuration)
*   **Local Settings (App):** 
    *   UI Theme (Dark/Light).
    *   Keep Screen On (WakeLock).
    *   Haptic Feedback Intensity.
    *   Steering Sensitivity/Dead-zone calibration.
*   **Remote Settings (RPi Node):**
    *   Target IP and UDP/TCP Port.
    *   CAN Interface selection (`can0`, `vcan0`).
    *   Bus Bitrate configuration (250k, 500k, etc.).
    *   **Get/Set Mechanism:** 
        *   **Fetch (GET):** Ability to query the RPi for its current active configuration to ensure the UI is in sync.
        *   **Apply (SET):** Push new configurations to the RPi with immediate feedback on success/failure.

---

## 4. Non-Functional & Safety Requirements
*   **Latency:** End-to-end latency from RPi to Android UI should be <50ms.
*   **Safety Default:** On app crash or disconnect, the RPi must return to a transparent "Passthrough" state.
*   **Compatibility:** All CSV files generated must be openable by the Python `recorder.py` and Excel/Google Sheets.

## 5. Deliverables
1.  Source Code (Android Studio Project).
2.  Compiled APK.
3.  Unit Tests for CSV integrity and JNI data marshaling.
4.  Documentation for `FileProvider` and sharing setup.
