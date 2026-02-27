#ifndef OBD_PIDS_H
#define OBD_PIDS_H

#include <stdint.h>

// --- OBD2 Standard PIDs (Service 01) ---
// See: https://en.wikipedia.org/wiki/OBD-II_PIDs

// Engine RPM (2 bytes)
// Formula: ((A * 256) + B) / 4
#define PID_RPM             0x0C

// Vehicle Speed (1 byte)
// Formula: A (km/h)
#define PID_SPEED           0x0D

// Throttle Position (1 byte)
// Formula: (A * 100) / 255 (%)
#define PID_THROTTLE_POS    0x11

// Engine Coolant Temperature (1 byte)
// Formula: A - 40 (Celsius)
#define PID_COOLANT_TEMP    0x05

// Fuel Level Input (1 byte)
// Formula: (A * 100) / 255 (%)
#define PID_FUEL_LEVEL      0x2F

// --- Proprietary PIDs (Custom for Simulation) ---
// Using unused range or custom service

// Brake Pressure (1 byte)
// Formula: A (Bar)
#define PID_BRAKE_PRESSURE  0xB0

// Steering Angle (2 bytes, signed)
// Formula: ((A * 256) + B) - 32768 (Degrees, +Right, -Left)
#define PID_STEERING_ANGLE  0xB1

// Steering Torque (1 byte)
// Formula: A (Nm)
#define PID_STEERING_TORQUE 0xB2

// Door Status (1 byte, Bitmask)
// Bit 0: Driver, Bit 1: Passenger, Bit 2: RearLeft, Bit 3: RearRight, Bit 4: Trunk, Bit 5: Hood
#define PID_DOOR_STATUS     0xD0

// Lights Status (1 byte, Bitmask)
// Bit 0: Headlights, Bit 1: HighBeams, Bit 2: LeftTurn, Bit 3: RightTurn, Bit 4: BrakeLights
#define PID_LIGHTS_STATUS   0xD1

// Odometer (4 bytes)
// Formula: ((A*2^24) + (B*2^16) + (C*2^8) + D) / 10 (km)
#define PID_ODOMETER        0xD2

// --- CAN IDs ---
#define CAN_ID_REQUEST      0x7DF // Standard OBD2 Request
#define CAN_ID_RESPONSE     0x7E8 // Standard ECU Response (Engine)

#endif // OBD_PIDS_H
