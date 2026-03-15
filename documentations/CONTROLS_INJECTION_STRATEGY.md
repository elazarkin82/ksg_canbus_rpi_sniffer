# Virtual Cockpit - Injection Strategy & Challenges

## 1. Overview
This document outlines the primary challenges encountered when transitioning from passive CAN bus monitoring (Reverse Engineering) to active vehicle control (Virtual Cockpit/Injection), and details the specific strategies our RpiCanbusSniffer architecture employs to overcome them.

## 2. Challenge 1: Checksums (CRC) and Rolling Counters (E2E Protection)

### The Problem
Modern automotive Electronic Control Units (ECUs) implement End-to-End (E2E) protection on critical messages (like steering or braking). This typically involves:
*   **Rolling Counter:** A specific sequence (e.g., 0-15) that must increment with each consecutive message.
*   **Checksum/CRC:** A cryptographic hash calculated over the payload bytes.
If an injected message contains an invalid counter or incorrect CRC, the target ECU will reject it as corrupted or malicious data, often triggering a Diagnostic Trouble Code (DTC) or entering a fail-safe state.

### Industry Standard Solutions
1.  **Replay Attacks:** Capturing a valid sequence of messages and replaying them exactly. (Limitation: Inflexible, cannot dynamically adjust steering angle).
2.  **Algorithm Reversing:** Reverse engineering the specific CRC polynomial and initial seed used by the manufacturer to calculate valid CRCs on-the-fly.

### Our Project's Proposed Solution: Assisted MiTM & Hardware CRC
Since our Sniffer operates as a transparent hardware bridge (Man-in-the-Middle):
1.  **Phase 1 (Basic/Older Vehicles):** We utilize direct `MODIFY` rules. The Sniffer intercepts the original message (with its valid counter), overwrites only the data bytes (e.g., steering angle), and forwards it. If no CRC is present, this works flawlessly.
2.  **Phase 2 (Advanced Vehicles):** We will introduce a `MODIFY_WITH_CRC` rule in the C++ Filter Engine. The user will select a known automotive CRC standard (e.g., CRC8-SAE J1850). The C++ engine will intercept the frame, inject the new joystick data, increment/maintain the counter, instantly recalculate the hardware-accelerated CRC, and transmit the valid frame.

## 3. Challenge 2: Value Normalization & Reverse Decoding

### The Problem
The Virtual Cockpit inputs are normalized (e.g., Joystick Steering: `-1.0` to `1.0`). We must translate these physical, normalized inputs back into the raw, multiplexed hexadecimal bytes the vehicle expects, reversing the formulas discovered during the Reverse Engineering phase.

### Industry Standard Solutions
*   **DBC Files:** Utilizing proprietary database files that define scaling, offset, minimum, and maximum values for every signal on the bus to automatically translate physical values to raw frames.

### Our Project's Proposed Solution: Dynamic Interpolation & Packing
We implement a dynamic reverse-encoding engine based on user-defined profiles:
1.  **Mapping Profile:** The user defines the `Raw Min` (e.g., `0x0000` for full left) and `Raw Max` (e.g., `0xFFFF` for full right) for a specific signal in the External Server UI.
2.  **Interpolation:** The Python backend takes the normalized joystick input (`-1.0` to `1.0`) and performs linear interpolation to calculate the required raw integer.
3.  **Byte Packing:** Using the previously defined Decoder configuration (`start_byte`, `length`, `type`), the system uses `struct.pack` (or bitwise shifting) to insert the raw integer directly into the correct position within the 8-byte (or 64-byte CAN-FD) payload array before injection.

## 4. Challenge 3: Injection Frequency & Bus Arbitration

### The Problem
Vehicle ECUs expect control messages at highly deterministic intervals (e.g., exactly every 10ms). 
*   **Too fast/Collision:** Injecting our messages alongside the original ECU creates bus collisions (Error Frames) and confuses the receiver.
*   **Too slow:** The receiver starves for data and disengages the system.

### Industry Standard Solutions
1.  **Node Spoofing:** Physically disconnecting the original transmitting ECU to prevent collisions, forcing the injection tool to take over the entire timing responsibility.
2.  **Overpowering:** Attempting to transmit a spoofed message micro-seconds after the original message to overwrite the receiver's buffer (highly unreliable).

### Our Project's Proposed Solution: The Bridge Advantage
Our architecture was specifically designed to solve this problem elegantly without requiring perfect timing logic on the External Server:
1.  **Natural Heartbeat:** Because the Sniffer physically sits *between* the sender and receiver, we do not need to generate our own clock or injection loops.
2.  **On-the-fly Modification:** When the user initiates "Take Control", the Python server simply updates a rule in the C++ engine (e.g., "Change bytes 0-1 of ID 0x401 to value X").
3.  **Perfect Timing:** The C++ engine waits for the *original* ECU to send its natural 10ms heartbeat. It catches the frame, modifies the bytes in $O(1)$ time, and forwards it to the target bus. 
4.  **Result:** The receiving ECU continues to get messages at the exact original frequency, with perfect intervals, and zero bus collisions, but containing our injected control data.
