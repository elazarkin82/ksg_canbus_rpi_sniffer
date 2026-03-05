# Future Ideas & Improvements

## 1. Safe Configuration Apply (Anti-Lockout Mechanism)
**Problem:** Updating network parameters (IP/Port) remotely carries a risk of losing connectivity if the new settings are incorrect or unreachable, effectively "bricking" the remote control capability.

**Proposed Solution: "Try & Confirm" Logic**
Implement a mechanism similar to OS screen resolution changes.

1.  **Temporary Application:**
    *   The External Server sends new parameters with a "Trial" flag (or a specific command).
    *   The Sniffer applies these settings temporarily (in memory or a temp file) and restarts.
    *   The Sniffer starts a countdown timer (e.g., 30 seconds).

2.  **Confirmation Handshake:**
    *   The External Server attempts to reconnect to the Sniffer using the new settings.
    *   If successful, it sends a `CMD_CONFIRM_PARAMS` command.
    *   Upon receiving confirmation, the Sniffer saves the settings permanently to `sniffer.prop` and stops the timer.

3.  **Automatic Revert:**
    *   If the timer expires without confirmation (meaning the Server couldn't reconnect), the Sniffer automatically reverts to the previous known-good configuration and restarts.
    *   This restores connectivity without physical access to the device.

**Implementation Requirements:**
*   **Protocol:** Add `CMD_CONFIRM_PARAMS` (0x1008). Update `CMD_SET_PARAMS` to support a timeout/trial mode.
*   **MainService:** Implement state machine for `Normal` vs `Trial` modes, backup/restore logic for `sniffer.prop`, and a revert timer.
*   **External Server:** Implement the logic to send params -> reconnect -> send confirmation.
