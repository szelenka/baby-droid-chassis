#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Servo Pin Definitions (Seeed Studio XIAO ESP32C6)
// Using D10, D9, D8 for direct servo control
#define SERVO_LEFT_PIN      D10  // GPIO18 (D10) - Left wheel continuous rotation servo signal
#define SERVO_RIGHT_PIN     D9   // GPIO20 (D9) - Right wheel continuous rotation servo signal
#define SERVO_DOME_PIN      D8   // GPIO19 (D8) - Dome rotation continuous rotation servo signal

// Button Mapping (from controller)
#define BUTTON_1_BIT        0   // Spin dome left
#define BUTTON_2_BIT        1   // Spin dome right
#define BUTTON_3_BIT        2   // Turn right
#define BUTTON_4_BIT        3   // Turn left
#define BUTTON_5_BIT        4   // Play sound
#define BUTTON_6_BIT        5   // Drive backwards
#define BUTTON_7_BIT        6   // Drive forwards
#define BUTTON_8_BIT        7   // Auxiliary

// Servo Configuration for continuous rotation servos
// These servos use a custom range of 800-2200µs (not standard 1000-2000µs)
// Center/stop should be around 1500µs (midpoint of 800-2200)
#define SERVO_MIN_US        800   // Minimum pulse width in microseconds
#define SERVO_MAX_US        2200  // Maximum pulse width in microseconds

// Individual servo stop positions (calibrated for each servo)
// Adjust these values to find the exact stop point for each servo
#define SERVO_LEFT_STOP_US   1455  // Left wheel servo stop point (calibrated)
#define SERVO_RIGHT_STOP_US  1455  // Right wheel servo stop point (calibrated)
#define SERVO_DOME_STOP_US   1465  // Dome servo stop point (1440 actual, +15µs compensation)

// Speed configuration for continuous rotation servos (microseconds offset from stop position)
#define DRIVE_SPEED_US      400   // Full speed forward/backward in microseconds offset from stop
#define TURN_SPEED_US       350   // Speed when turning in place in microseconds offset from stop
#define DOME_SPEED_US       150   // Dome rotation speed in microseconds offset from stop

// Timing Constants
#define COMMAND_TIMEOUT           500   // Time before stopping motors if no command received (ms)

// Battery Monitoring
// Wire the switched 2S LiPo rail to D0 / GPIO0 through a resistor divider:
//   switched battery+ -> 47k -> D0 / GPIO0
//   D0 / GPIO0 -> 22k -> GND
// Optional: add 100nF from D0 / GPIO0 to GND for extra filtering.
#define BATTERY_MONITOR_ENABLED           1
#define BATTERY_SENSE_GPIO                0       // GPIO0 / D0 header pin (ADC-capable)
#define BATTERY_DIVIDER_TOP_OHMS          47000UL // Resistor from switched battery+ to sense pin
#define BATTERY_DIVIDER_BOTTOM_OHMS       22000UL // Resistor from sense pin to ground
#define BATTERY_SAMPLE_INTERVAL_MS        1000    // Poll battery voltage once per second
#define BATTERY_SAMPLE_COUNT              8       // Average several ADC samples to reduce noise
#define BATTERY_LOW_MV                    7000    // Warn that the 2S pack should be recharged soon
#define BATTERY_LOW_CLEAR_MV              7200    // Clear low-battery warning after recovery
#define BATTERY_CRITICAL_MV               6800    // Stop drive before the pack is over-discharged
#define BATTERY_CRITICAL_CLEAR_MV         7000    // Clear critical lockout after recovery
#define BATTERY_STATUS_PRINT_INTERVAL_MS  30000   // Repeat low/critical battery status over serial

// Use the XIAO ESP32C6 user LED as the low-battery indicator.
// If your board's LED behaves inverted, flip BATTERY_STATUS_LED_ACTIVE_LOW.
#define BATTERY_STATUS_LED_PIN            LED_BUILTIN
#define BATTERY_STATUS_LED_ACTIVE_LOW     0
#define BATTERY_LOW_BLINK_MS              500
#define BATTERY_CRITICAL_BLINK_MS         150

// Calibration mode - uncomment to enable manual servo calibration
// #define CALIBRATION_MODE

// ESP-NOW Configuration
// This will be the receiver - MAC address should match TARGET_MAC_ADDRESS in controller
// You can get this device's MAC address from Serial output on first run
#define WIFI_CHANNEL        1

// Allowed Controller MAC Address (optional security feature)
// Set to controller's MAC address to only accept commands from that device
// Comment out to accept commands from any ESP-NOW transmitter
#define ALLOWED_CONTROLLER_MAC {0x0C, 0x8B, 0x95, 0x94, 0xF1, 0x00}

#endif // CONFIG_H
