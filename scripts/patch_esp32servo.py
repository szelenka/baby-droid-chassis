"""
Pre-build script: patch ESP32Servo library to recognize ESP32C6's 6-channel LEDC limit.

The upstream ESP32Servo library (v3.0.9) only checks for ESP32C3 when setting NUM_PWM=6.
The ESP32C6 also has 6 LEDC channels but falls through to the default of 16, causing
channel allocation failures at runtime.
"""
Import("env")
import os

def patch_esp32pwm(source, target, env):
    libdeps = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "ESP32Servo", "src", "ESP32PWM.h")
    if not os.path.exists(libdeps):
        return

    with open(libdeps, "r") as f:
        content = f.read()

    old = "#if defined(CONFIG_IDF_TARGET_ESP32C3)"
    new = "#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)"

    if old in content and new not in content:
        content = content.replace(old, new)
        with open(libdeps, "w") as f:
            f.write(content)
        print("Patched ESP32PWM.h: added ESP32C6 to 6-channel LEDC targets")

env.AddPreAction("buildprog", patch_esp32pwm)
