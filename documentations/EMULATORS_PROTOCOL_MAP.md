# Emulators Protocol Map

## 1. Overview
This document serves as the "DBC" (Data Base CAN) equivalent for our custom emulation environment (`CarSystemEmulator` and `CarComputerEmulator`). It documents every CAN ID currently simulated by the system, what physical property it represents, the byte structure, and the mathematical formula required to decode the raw hex bytes back into physical values.

## 2. Proprietary CAN Bus (Internal Communication)
These messages simulate the fast, internal communication between the vehicle's peripheral sensors/systems (`CarSystemEmulator`) and the Electronic Control Unit (`CarComputerEmulator`). They are broadcast continuously without requiring a request.

| CAN ID | Signal Name | Sender | Length (DLC) | Byte Index | Formula (Python/Decoder) | Unit | Description |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **`0x100`** | Engine RPM | Car System | 2 | `[0], [1]` | `(x[0]*256 + x[1]) / 4.0` | RPM | High-resolution engine speed. Standard 2-byte Big-Endian format. |
| **`0x200`** | Vehicle Speed | Car System | 1 | `[0]` | `x[0]` | km/h | Basic vehicle speed. Direct 1-to-1 mapping. |
| **`0x400`** | Throttle/Brake Cmd | Car Computer | 2 | `[0]` (Throttle)<br>`[1]` (Brake) | `x[0]` (Throttle)<br>`x[1]` (Brake) | % (0-100) | Control commands sent from the ECU (Cruise Control) to the actuators. |
| **`0x401`** | Steering Angle Cmd | Car Computer | 2 | `[0], [1]` | `signed_int16(x[0], x[1]) / 10.0` | Degrees | Steering command from ECU (e.g., Lane Keep Assist). Scaled by 10 (e.g., 300 = 30.0 deg). |

## 3. Standard OBD2 Diagnostics (ISO 15765-4 / SAE J1979)
These messages simulate standard diagnostic responses. The `CarComputerEmulator` (ECU) only sends these messages when explicitly requested by an external scanner (or our Sniffer) using the broadcast ID `0x7DF` or physical ID `0x7E0`. 

**Response ID:** `0x7E8` (Engine ECU Response)
**Standard Frame Format:** `[PCI Length] [Mode + 0x40] [PID] [Data A] [Data B] [Data C] [Data D]`
*(Note: In the formulas below, `x[0]` refers to the first actual data byte, which is `Data A` at physical frame index 3).*

| PID (Hex) | Signal Name | Data Bytes | Decoder Formula (x = Data Payload) | Unit | Note |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`0x04`** | Calculated Engine Load | 1 | `x[0] * 100.0 / 255.0` | % | Derived from speed in emulator. |
| **`0x05`** | Engine Coolant Temp | 1 | `x[0] - 40` | °C | Simulates engine warming up over time. |
| **`0x0A`** | Fuel Pressure | 1 | `x[0] * 3` | kPa | Constant simulation (~300kPa). |
| **`0x0B`** | Intake Manifold MAP | 1 | `x[0]` | kPa | Derived from RPM. |
| **`0x0C`** | Engine RPM | 2 | `(x[0]*256 + x[1]) / 4.0` | RPM | Same base value as `0x100`. |
| **`0x0D`** | Vehicle Speed | 1 | `x[0]` | km/h | Same base value as `0x200`. |
| **`0x0E`** | Timing Advance | 1 | `x[0] / 2.0 - 64.0` | ° (Deg) | Random fluctuation around 10-20°. |
| **`0x10`** | MAF Air Flow Rate | 2 | `(x[0]*256 + x[1]) / 100.0` | g/s | Derived from RPM. |
| **`0x11`** | Throttle Position | 1 | `x[0] * 100.0 / 255.0` | % | Derived from RPM/Speed. |
| **`0x1F`** | Run Time Since Start | 2 | `x[0]*256 + x[1]` | Seconds | Uptime of the emulator process. |
| **`0x2F`** | Fuel Tank Level Input | 1 | `x[0] * 100.0 / 255.0` | % | Slowly decreases over time. |

## 4. Usage in External Server
To apply these decodings in the `External Server`:
1. For **Proprietary CAN** (`0x100`, `0x200`), use the `Start Byte = 0` and the formulas listed above.
2. For **OBD2** (`0x7E8`), configure the Protocol as `OBD2`. The system will automatically set `PID Index = 2`. The payload data starts at physical byte index 3, so the Decoder engine will internally slice the frame from byte 3, making `x[0]` align perfectly with the formulas listed above.
