// Servo calibration tool - finds exact stop point
// Edit TEST_PIN and APPROX_STOP_US below for each servo you want to calibrate

#include <Arduino.h>
#include <ESP32Servo.h>

// ===== EDIT THESE VALUES FOR EACH SERVO =====
#define TEST_PIN D10            // Change to D10, D9, or D8 on XIAO ESP32C6
#define APPROX_STOP_US 1400     // Approximate stop point from config.h
// =============================================

#define RANGE 100               // Test ±100µs from approximate stop
#define STEP 10                  // Increment by 5µs

Servo testServo;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=== Servo Calibration Tool ===");
    Serial.print("Testing pin: ");
    Serial.println(TEST_PIN);
    Serial.print("Approximate stop: ");
    Serial.print(APPROX_STOP_US);
    Serial.println("µs");
    Serial.print("Testing range: ");
    Serial.print(APPROX_STOP_US - RANGE);
    Serial.print("µs to ");
    Serial.print(APPROX_STOP_US + RANGE);
    Serial.println("µs");
    Serial.print("Step size: ");
    Serial.print(STEP);
    Serial.println("µs\n");

    testServo.attach(TEST_PIN, 800, 2200);
    delay(100);

    Serial.println("Watch the servo and note which value stops it completely.\n");
    delay(2000);
}

void loop() {
    int startUs = APPROX_STOP_US - RANGE;
    int endUs = APPROX_STOP_US + RANGE;

    Serial.println("Starting calibration sweep...\n");

    for (int us = startUs; us <= endUs; us += STEP) {
        Serial.print("Testing: ");
        Serial.print(us);
        Serial.println("µs");

        testServo.writeMicroseconds(us);
        delay(3000);  // Hold for 3 seconds
    }

    Serial.println("\n=== Calibration complete ===");
    Serial.println("Which value stopped the servo?");
    Serial.println("Update the corresponding SERVO_X_STOP_US in config.h");
    Serial.println("\nHolding at approximate stop point...\n");

    testServo.writeMicroseconds(APPROX_STOP_US);
    delay(10000);
}
