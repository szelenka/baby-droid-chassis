#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

// Servo objects
Servo servoLeft;
Servo servoRight;
Servo servoDome;

// Command tracking
unsigned long lastCommandTime = 0;
uint8_t currentButtonMask = 0;

// Mutex for thread-safe access to shared variables
SemaphoreHandle_t dataMutex = NULL;

// Track if servos are currently stopped (to avoid sending stop repeatedly)
bool leftStopped = true;
bool rightStopped = true;
bool domeStopped = true;

#if BATTERY_MONITOR_ENABLED
    // The config stores the battery sense input as the raw ESP32-C6 GPIO number.
    // Use the raw-GPIO ADC helpers directly so the code matches the wiring table.
extern "C" uint32_t __analogReadMilliVolts(uint8_t pin);
extern "C" void __analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation);

enum BatteryState {
    BATTERY_OK = 0,
    BATTERY_LOW,
    BATTERY_CRITICAL
};

BatteryState batteryState = BATTERY_OK;
uint32_t filteredBatteryMilliVolts = 0;
unsigned long lastBatterySampleTime = 0;
unsigned long lastBatteryStatusPrintTime = 0;
#endif

void stopAllMotors() {
    servoLeft.writeMicroseconds(SERVO_LEFT_STOP_US);
    servoRight.writeMicroseconds(SERVO_RIGHT_STOP_US);
    servoDome.writeMicroseconds(SERVO_DOME_STOP_US);
    leftStopped = true;
    rightStopped = true;
    domeStopped = true;
}

#if BATTERY_MONITOR_ENABLED
const char *batteryStateName(BatteryState state) {
    switch (state) {
        case BATTERY_LOW:
            return "LOW";
        case BATTERY_CRITICAL:
            return "CRITICAL";
        case BATTERY_OK:
        default:
            return "OK";
    }
}

void setBatteryStatusLed(bool on) {
#if BATTERY_STATUS_LED_ACTIVE_LOW
    digitalWrite(BATTERY_STATUS_LED_PIN, on ? LOW : HIGH);
#else
    digitalWrite(BATTERY_STATUS_LED_PIN, on ? HIGH : LOW);
#endif
}

uint32_t scaleBatterySenseToPackMilliVolts(uint32_t senseMilliVolts) {
    const uint32_t dividerTotal = BATTERY_DIVIDER_TOP_OHMS + BATTERY_DIVIDER_BOTTOM_OHMS;
    return (senseMilliVolts * dividerTotal + (BATTERY_DIVIDER_BOTTOM_OHMS / 2)) / BATTERY_DIVIDER_BOTTOM_OHMS;
}

uint32_t readBatteryPackMilliVolts() {
    uint32_t totalSenseMilliVolts = 0;

    for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; ++i) {
        totalSenseMilliVolts += __analogReadMilliVolts(BATTERY_SENSE_GPIO);
        delayMicroseconds(200);
    }

    const uint32_t averageSenseMilliVolts = totalSenseMilliVolts / BATTERY_SAMPLE_COUNT;
    return scaleBatterySenseToPackMilliVolts(averageSenseMilliVolts);
}

BatteryState classifyBatteryState(uint32_t batteryMilliVolts, BatteryState currentState) {
    switch (currentState) {
        case BATTERY_CRITICAL:
            if (batteryMilliVolts <= BATTERY_CRITICAL_CLEAR_MV) {
                return BATTERY_CRITICAL;
            }
            return (batteryMilliVolts <= BATTERY_LOW_MV) ? BATTERY_LOW : BATTERY_OK;

        case BATTERY_LOW:
            if (batteryMilliVolts <= BATTERY_CRITICAL_MV) {
                return BATTERY_CRITICAL;
            }
            return (batteryMilliVolts < BATTERY_LOW_CLEAR_MV) ? BATTERY_LOW : BATTERY_OK;

        case BATTERY_OK:
        default:
            if (batteryMilliVolts <= BATTERY_CRITICAL_MV) {
                return BATTERY_CRITICAL;
            }
            if (batteryMilliVolts <= BATTERY_LOW_MV) {
                return BATTERY_LOW;
            }
            return BATTERY_OK;
    }
}

void printBatteryStatus(const char *prefix) {
    Serial.printf(
        "%s %lu.%03luV (%s)\n",
        prefix,
        filteredBatteryMilliVolts / 1000UL,
        filteredBatteryMilliVolts % 1000UL,
        batteryStateName(batteryState)
    );
}

void initBatteryMonitor() {
    pinMode(BATTERY_STATUS_LED_PIN, OUTPUT);
    setBatteryStatusLed(false);

    analogReadResolution(12);
    __analogSetPinAttenuation(BATTERY_SENSE_GPIO, ADC_11db);

    filteredBatteryMilliVolts = readBatteryPackMilliVolts();
    batteryState = classifyBatteryState(filteredBatteryMilliVolts, BATTERY_OK);
    printBatteryStatus("Battery monitor initialized:");

    if (batteryState == BATTERY_LOW) {
        Serial.println("Battery warning: recharge soon.");
    } else if (batteryState == BATTERY_CRITICAL) {
        Serial.println("Battery critical: drive outputs inhibited until voltage recovers.");
    }
}

void updateBatteryMonitor() {
    const unsigned long now = millis();
    if (now - lastBatterySampleTime < BATTERY_SAMPLE_INTERVAL_MS) {
        return;
    }

    lastBatterySampleTime = now;

    const uint32_t measuredBatteryMilliVolts = readBatteryPackMilliVolts();
    if (filteredBatteryMilliVolts == 0) {
        filteredBatteryMilliVolts = measuredBatteryMilliVolts;
    } else {
        // Low-pass filter the pack reading to avoid reacting to single noisy samples.
        filteredBatteryMilliVolts = ((filteredBatteryMilliVolts * 3UL) + measuredBatteryMilliVolts) / 4UL;
    }

    const BatteryState newState = classifyBatteryState(filteredBatteryMilliVolts, batteryState);
    if (newState != batteryState) {
        batteryState = newState;
        printBatteryStatus("Battery state changed:");

        if (batteryState == BATTERY_LOW) {
            Serial.println("Battery warning: finish the current run and recharge soon.");
        } else if (batteryState == BATTERY_CRITICAL) {
            Serial.println("Battery critical: stopping drive outputs to protect the pack.");
        } else {
            Serial.println("Battery recovered to the normal operating range.");
        }

        lastBatteryStatusPrintTime = now;
    } else if (batteryState != BATTERY_OK && now - lastBatteryStatusPrintTime >= BATTERY_STATUS_PRINT_INTERVAL_MS) {
        printBatteryStatus("Battery status:");
        lastBatteryStatusPrintTime = now;
    }
}

void updateBatteryStatusLed() {
    bool ledOn = false;

    if (batteryState == BATTERY_LOW) {
        ledOn = ((millis() / BATTERY_LOW_BLINK_MS) % 2U) == 0U;
    } else if (batteryState == BATTERY_CRITICAL) {
        ledOn = ((millis() / BATTERY_CRITICAL_BLINK_MS) % 2U) == 0U;
    }

    setBatteryStatusLed(ledOn);
}
#endif

// Function to print MAC address on startup
void printMACAddress() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    Serial.print("ESP32 MAC Address: {0x");
    for (int i = 0; i < 6; i++) {
        Serial.print(mac[i], HEX);
        if (i < 5) Serial.print(", 0x");
    }
    Serial.println("}");
    Serial.println("Use this MAC address in the controller's TARGET_MAC_ADDRESS");
}

// Callback when data is received
void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
#ifdef ALLOWED_CONTROLLER_MAC
    // Verify sender MAC address if security is enabled
    uint8_t allowedMAC[] = ALLOWED_CONTROLLER_MAC;
    bool macMatch = true;

    for (int i = 0; i < 6; i++) {
        if (recv_info->src_addr[i] != allowedMAC[i]) {
            macMatch = false;
            break;
        }
    }

    if (!macMatch) {
        Serial.print("Rejected packet from unauthorized MAC: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", recv_info->src_addr[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
        return;  // Ignore packets from unauthorized senders
    }
#endif

    if (len == 1) {
        // Thread-safe update of shared variables
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            currentButtonMask = incomingData[0];
            lastCommandTime = millis();
            xSemaphoreGive(dataMutex);
        }

        Serial.print("Received button mask: 0b");
        Serial.print(incomingData[0], BIN);
        Serial.print(" (0x");
        Serial.print(incomingData[0], HEX);
        Serial.println(")");
    }
}

void initESPNow() {
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Small delay to allow WiFi to initialize
    delay(100);

    // Print MAC address for setup
    printMACAddress();

    // Set WiFi channel
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Register receive callback
    esp_now_register_recv_cb(onDataReceived);

    Serial.println("ESP-NOW initialized successfully");
}

void initServos() {
    // Attach servos one at a time with delays to avoid conflicts
    // Use custom range 800-2200µs as determined by servo tester
    Serial.printf("Attaching servo to pin %d (left) with range 800-2200us...\n", SERVO_LEFT_PIN);
    servoLeft.attach(SERVO_LEFT_PIN, SERVO_MIN_US, SERVO_MAX_US);
    delay(100);

    Serial.printf("Attaching servo to pin %d (right) with range 800-2200us...\n", SERVO_RIGHT_PIN);
    servoRight.attach(SERVO_RIGHT_PIN, SERVO_MIN_US, SERVO_MAX_US);
    delay(100);

    Serial.printf("Attaching servo to pin %d (dome) with range 800-2200us...\n", SERVO_DOME_PIN);
    servoDome.attach(SERVO_DOME_PIN, SERVO_MIN_US, SERVO_MAX_US);
    delay(100);

    // Initialize all servos to their calibrated stop positions
    Serial.println("Setting servos to stop position...");
    servoLeft.writeMicroseconds(SERVO_LEFT_STOP_US);
    delay(50);
    servoRight.writeMicroseconds(SERVO_RIGHT_STOP_US);
    delay(50);
    servoDome.writeMicroseconds(SERVO_DOME_STOP_US);
    delay(50);

    // Verify what's being sent
    Serial.print("Left servo readMicroseconds: ");
    Serial.print(servoLeft.readMicroseconds());
    Serial.println("µs");

    Serial.print("Right servo readMicroseconds: ");
    Serial.print(servoRight.readMicroseconds());
    Serial.println("µs");

    Serial.print("Dome servo readMicroseconds: ");
    Serial.print(servoDome.readMicroseconds());
    Serial.println("µs");

    Serial.println("\n*** If servo spins continuously, try manually adjusting SERVO_DOME_STOP_US in config.h ***");

    Serial.println("Servos initialized");

    // Verify PWM is actually working
    Serial.print("Left servo attached: ");
    Serial.println(servoLeft.attached() ? "YES" : "NO");
    Serial.print("Right servo attached: ");
    Serial.println(servoRight.attached() ? "YES" : "NO");
    Serial.print("Dome servo attached: ");
    Serial.println(servoDome.attached() ? "YES" : "NO");
}

void updateMotors() {
#if BATTERY_MONITOR_ENABLED
    if (batteryState == BATTERY_CRITICAL) {
        stopAllMotors();
        return;
    }
#endif

    // Thread-safe read of shared variables
    uint8_t buttonMask;
    unsigned long cmdTime;
    
    if (xSemaphoreTake(dataMutex, 0) == pdTRUE) {
        buttonMask = currentButtonMask;
        cmdTime = lastCommandTime;
        xSemaphoreGive(dataMutex);
    } else {
        // Could not get mutex, skip this update cycle
        return;
    }

    // Check for command timeout
    if (millis() - cmdTime > COMMAND_TIMEOUT) {
        // Stop all motors if no recent command
        stopAllMotors();
        return;
    }

    // If no buttons pressed, stop everything and continue to update
    // (don't return early - we need to process dome separately below)
    if (buttonMask == 0) {
        stopAllMotors();
        return; 
    }

    // Extract button states from bitmask (using local copy)
    bool domeLeft = (buttonMask >> BUTTON_1_BIT) & 1;
    bool domeRight = (buttonMask >> BUTTON_2_BIT) & 1;
    bool turnRight = (buttonMask >> BUTTON_3_BIT) & 1;
    bool turnLeft = (buttonMask >> BUTTON_4_BIT) & 1;
    bool driveBack = (buttonMask >> BUTTON_6_BIT) & 1;
    bool driveFwd = (buttonMask >> BUTTON_7_BIT) & 1;

    // Handle dome rotation (independent of drive)
    if (domeLeft && !domeRight) {
        servoDome.writeMicroseconds(SERVO_DOME_STOP_US - DOME_SPEED_US);  // Rotate left
        domeStopped = false;
    } else if (domeRight && !domeLeft) {
        servoDome.writeMicroseconds(SERVO_DOME_STOP_US + DOME_SPEED_US);  // Rotate right
        domeStopped = false;
    } else if (!domeStopped) {
        // Only send stop command once
        servoDome.writeMicroseconds(SERVO_DOME_STOP_US);
        domeStopped = true;
    }

    // Calculate drive speeds for differential drive (as microseconds offset from stop)
    int leftSpeedUs = 0;
    int rightSpeedUs = 0;

    // Forward/Backward drive
    if (driveFwd && !driveBack) {
        leftSpeedUs = DRIVE_SPEED_US;   // Forward = positive microseconds offset
        rightSpeedUs = DRIVE_SPEED_US;
    } else if (driveBack && !driveFwd) {
        leftSpeedUs = -DRIVE_SPEED_US;  // Backward = negative microseconds offset
        rightSpeedUs = -DRIVE_SPEED_US;
    }

    // Turn in place or adjust for turns while driving
    if (turnLeft && !turnRight) {
        if (leftSpeedUs == 0 && rightSpeedUs == 0) {
            // Turn in place - left wheel backwards, right wheel forwards
            leftSpeedUs = -TURN_SPEED_US;
            rightSpeedUs = TURN_SPEED_US;
        } else {
            // Adjust speeds for turning while driving - reduce inner wheel more for sharper turn
            leftSpeedUs = leftSpeedUs / 4;
            rightSpeedUs = rightSpeedUs;
        }
    } else if (turnRight && !turnLeft) {
        if (leftSpeedUs == 0 && rightSpeedUs == 0) {
            // Turn in place - left wheel forwards, right wheel backwards
            leftSpeedUs = TURN_SPEED_US;
            rightSpeedUs = -TURN_SPEED_US;
        } else {
            // Adjust speeds for turning while driving - reduce inner wheel more for sharper turn
            leftSpeedUs = leftSpeedUs;
            rightSpeedUs = rightSpeedUs / 4;
        }
    }

    // Apply speeds to servos using calibrated stop points
    // Speeds are already in microseconds, just apply offset from calibrated stop
    // Positive speed = forward (higher µs), negative = backward (lower µs)
    // Left servo is reversed relative to right servo for forward motion
    int leftUs = SERVO_LEFT_STOP_US + leftSpeedUs;   // Normal: add for forward
    int rightUs = SERVO_RIGHT_STOP_US - rightSpeedUs; // Reversed: subtract for forward

    // Constrain to valid range
    leftUs = constrain(leftUs, SERVO_MIN_US, SERVO_MAX_US);
    rightUs = constrain(rightUs, SERVO_MIN_US, SERVO_MAX_US);

    // Only send signals when needed (moving or initial stop)
    if (leftUs == SERVO_LEFT_STOP_US) {
        if (!leftStopped) {
            servoLeft.writeMicroseconds(leftUs);
            leftStopped = true;
        }
    } else {
        servoLeft.writeMicroseconds(leftUs);
        leftStopped = false;
    }

    if (rightUs == SERVO_RIGHT_STOP_US) {
        if (!rightStopped) {
            servoRight.writeMicroseconds(rightUs);
            rightStopped = true;
        }
    } else {
        servoRight.writeMicroseconds(rightUs);
        rightStopped = false;
    }

}

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 Baby Droid Chassis Starting...");

    // Create mutex for thread-safe access to shared variables
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println("ERROR: Failed to create mutex!");
        while(1); // Halt if mutex creation fails
    }

#if BATTERY_MONITOR_ENABLED
    initBatteryMonitor();
#endif

    // Initialize servos
    initServos();

    // Initialize ESP-NOW
    initESPNow();

    Serial.println("Chassis ready!");
}

void loop() {
#if BATTERY_MONITOR_ENABLED
    updateBatteryMonitor();
    updateBatteryStatusLed();
#endif

    // Update motor speeds based on current button state
    updateMotors();

    // Small delay to prevent overwhelming the system
    // Increased to 50ms to reduce servo update frequency and minimize twitching
    delay(50);
}
