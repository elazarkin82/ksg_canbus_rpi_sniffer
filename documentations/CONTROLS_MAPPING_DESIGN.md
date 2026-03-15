# Virtual Cockpit - Controls Mapping Design

## 1. Objective
The goal of this phase is to establish the foundation for active vehicle control (Virtual Cockpit) by defining a formal mapping between reverse-engineered CAN bus signals and specific vehicle control functions: **Steering**, **Throttle**, and **Brake**.

This mapping will serve as the bridge between passive analysis and active injection, allowing the system to transition from normal driving mode to joystick/keyboard control.

## 2. Challenges & Considerations

### 2.1 Complex Message Structures (Multiplexing & Checksums)
In modern vehicles, critical control commands (like steering angle) are rarely transmitted as plain text. They usually incorporate:
*   **Rolling Counters:** A sequence number that increments with each message.
*   **Checksums (CRC):** A cryptographic hash of the data to ensure integrity.
If we inject a steering command without properly updating the counter and calculating the correct CRC, the ECU will reject the message and likely throw a fault code (DTC).

### 2.2 Value Normalization
The input from our virtual controllers (Joystick/Keyboard) will be normalized values:
*   Throttle: `0.0` to `1.0` (0% to 100%)
*   Steering: `-1.0` (Full Left) to `1.0` (Full Right)

We need a mechanism to map these normalized inputs to the raw values expected by the vehicle (e.g., `-1.0` steering might map to raw hex `0xF448` representing `-3000` degrees).

### 2.3 Injection Rate
Control messages must be injected at a specific, consistent frequency (e.g., exactly every 10ms or 20ms). Deviating from this timing can cause the target controller to disengage or enter a fail-safe state.

## 3. Proposed Implementation Plan

### 3.1 Data Model (`ProfileManager`)
A new section will be added to the `profile.json` to store the control mappings.

```json
"mappings": {
    "steering": {
        "can_id": "0x401",
        "pid": null,
        "signal_name": "Steering Angle",
        "min_raw_value": -300,
        "max_raw_value": 300,
        "requires_checksum": false
    },
    "throttle": { ... },
    "brake": { ... }
}
```

### 3.2 GUI: "Control Mappings" Interface
We will create a dedicated interface (likely a new tab or a section within the Rules Manager) featuring three distinct blocks: **Steering**, **Throttle**, and **Brake**.

For each control, the user will be able to:
1.  **Select a Signal:** Choose from a dropdown list populated by all currently defined Decoders (from the Reverse Engineering process).
2.  **Define Min/Max Boundaries:** Input the raw minimum and maximum values that correspond to full input (e.g., max left vs. max right).

### 3.3 UX Integration (Reverse Engineering Panel)
To streamline the workflow, we will add an option to the right-click context menu in the `ReverseEngineeringPanel`:
**"Set as Control Input..." -> "Set as Steering / Throttle / Brake"**
Clicking this will automatically link the selected, decoded signal to the corresponding vehicle control function in the Mappings tab.

### 3.4 The Path Forward (Future Injection Logic)
Once the mappings are defined, the system will be able to execute the "Take Control" sequence:
1.  **Block (DROP):** Create an automatic filter rule in the Sniffer to drop the original CAN ID coming from the vehicle's actual ECU/Sensors.
2.  **Translate:** Take normalized input from the PC keyboard/joystick and interpolate it into the expected raw value using the defined Min/Max boundaries.
3.  **Encode (Reverse Decode):** Pack the raw value back into the specific bytes and bits defined by the original signal's formula.
4.  **Inject:** Transmit the newly constructed frame to the target bus at the required frequency.
