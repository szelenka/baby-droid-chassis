# Baby Droid Chassis - Makefile
# Convenience wrapper for PlatformIO commands

.PHONY: help build upload monitor clean release calibrate

# Default target
help:
	@echo "Baby Droid Chassis - Build Commands"
	@echo "===================================="
	@echo ""
	@echo "Release (Normal Operation):"
	@echo "  make build         - Build release firmware"
	@echo "  make upload        - Upload release firmware"
	@echo "  make monitor       - Open serial monitor"
	@echo "  make release       - Build + upload + monitor (release)"
	@echo ""
	@echo "Calibration Mode:"
	@echo "  make calibrate     - Build + upload + monitor (calibration)"
	@echo "  make cal-build     - Build calibration firmware only"
	@echo "  make cal-upload    - Upload calibration firmware only"
	@echo ""
	@echo "Utilities:"
	@echo "  make clean         - Clean build files"
	@echo "  make list-envs     - List all environments"
	@echo ""

# ============================================
# RELEASE COMMANDS (default environment)
# ============================================

build:
	@pio run -e release

upload:
	@pio run -e release --target upload

monitor:
	@pio device monitor

release: upload monitor

# ============================================
# CALIBRATION COMMANDS
# ============================================

cal-build:
	@pio run -e calibrate

cal-upload:
	@pio run -e calibrate --target upload

calibrate: cal-upload monitor

# ============================================
# UTILITIES
# ============================================

clean:
	pio run --target clean

list-envs:
	@echo "Available environments:"
	@echo "  - release (default)"
	@echo "  - calibrate"
