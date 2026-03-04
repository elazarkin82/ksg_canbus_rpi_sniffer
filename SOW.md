# Statement of Work (SOW) - RpiCanbusSniffer System

## 1. Introduction & Motivation
The automotive industry relies heavily on the CAN bus protocol for communication between vehicle components. Researchers, security experts, and developers often need a robust tool to monitor, analyze, and intervene in this traffic in real-time. Existing solutions are often proprietary, expensive, or lack the flexibility required for advanced reverse engineering and active control.

**Goal:** To develop a high-performance, open-source CAN bus sniffing and manipulation system based on Raspberry Pi (or similar embedded Linux platforms), capable of acting as a transparent bridge with advanced filtering and injection capabilities.

## 2. System Architecture

### 2.1 RpiCanbusSniffer (Core)
The core component runs on the embedded device. It connects to two CAN interfaces:
1.  **Car System Bus:** Connected to the vehicle's peripheral systems (sensors, dashboard).
2.  **Car Computer Bus:** Connected to the ECU (Electronic Control Unit).

**Development Principles:**
*   **Low-Level C++:** The core logic is written in "C-style" C++ to ensure deterministic performance and minimal overhead.
*   **Memory Management:** Static memory allocation is preferred over dynamic allocation to prevent fragmentation and ensure stability.
*   **MISRA Compliance:** The code aims to adhere to **MISRA C++** guidelines. Full compliance is a future roadmap item to ensure safety-critical reliability.

### 2.2 External Server (Control Station)
A PC-based application (Python) that connects to the Sniffer via UDP. It serves as the user interface for:
*   Real-time monitoring.
*   Reverse engineering (decoding signals).
*   Defining filtering and injection rules.
*   Virtual cockpit control.

### 2.3 Emulators
To facilitate development without physical vehicle access, the system includes:
*   **Car System Simulator:** Simulates vehicle physics (RPM, Speed) and sensors.
*   **ECU Simulator:** Simulates the car computer's control logic (Cruise Control).

## 3. Functional Requirements

### 3.1 Sniffer Capabilities
*   **Passthrough:** Transparently forward messages between interfaces by default.
*   **Filtering:** Block (DROP) specific messages based on ID or content.
*   **Modification:** Modify (MODIFY) message content on-the-fly (Bitwise operations).
*   **Injection:** Inject new messages into either bus.
*   **Logging:** Mirror traffic to the External Server via UDP.
*   **Configuration:** Load settings from `sniffer.prop` and support dynamic reconfiguration.

### 3.2 External Server Capabilities
*   **Dashboard:** Connection status and basic controls.
*   **Reverse Engineering:** Traffic recording, filtering, and flexible decoders (Integer, Float, Enum, etc.).
*   **Rules Manager:** GUI for defining complex logic rules.
*   **Virtual Cockpit:** Visual representation of vehicle state (Steering wheel, Pedals) and keyboard control.

## 4. Use Cases (Flows)

### 4.1 Passive Research
1.  User connects Sniffer to vehicle.
2.  External Server records traffic.
3.  User identifies the RPM signal ID.
4.  User configures a decoder to visualize RPM graph.

### 4.2 Active Intervention
1.  User identifies the Speed signal ID.
2.  User creates a `MODIFY` rule to overwrite the speed value.
3.  Vehicle dashboard displays the manipulated speed.

### 4.3 Remote Control
1.  User identifies Steering Control ID.
2.  User creates a `DROP` rule to block original ECU steering commands.
3.  User uses the Virtual Cockpit (Keyboard) to inject steering commands.
4.  Vehicle wheels turn according to user input.

## 5. Modules Description
*   **Communication Layer:** `Tcp/UdpCanbusCommunication` classes handling raw socket operations.
*   **Filter Engine:** A highly optimized, static-memory engine for processing `CanFilterRule` logic in $O(1)$ time.
*   **SDK (`sniffer_client`):** A C++ shared library exposing a clean API for external applications (Python, Android, etc.) to interact with the Sniffer.
*   **GUI:** Python modules (`tkinter`) for visualization and control.

## 6. Non-Functional Requirements
*   **Performance:** Ultra-low latency processing to prevent bus errors or vehicle fault codes.
*   **Reliability:** Watchdog mechanism to reset the system to a safe "Passthrough" state if the control link is lost.
*   **Portability:** Standard C++17 codebase, compatible with Linux SocketCAN.

## 7. QA & Testing Strategy
*   **CI/CD:** Automated test suite execution during the release process.
*   **Test Implementation:** Unlike the Core, tests are written in **Modern C++** (and Python) utilizing high-level abstractions for readability and rapid development.
*   **Coverage:**
    *   **Unit Tests:** For individual components (Filter Engine, Protocol).
    *   **Integration Tests:** Using Emulators to verify full system flow.
    *   **System Tests:** End-to-end verification with the External Server.
