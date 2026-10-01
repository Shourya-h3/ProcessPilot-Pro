# ProcessPilot Pro - Master Makefile

BUILD_DIR ?= build
DRIVER_DIR = src/driver

.PHONY: all userspace driver test clean install load_driver unload_driver demo help

all: userspace driver

userspace:
	@echo "=== Building ProcessPilot Pro Userspace Binaries ==="
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && $(MAKE) -j$(shell nproc 2>/dev/null || echo 2)
	@mkdir -p bin
	@cp -f $(BUILD_DIR)/bin/* bin/ 2>/dev/null || cp -f $(BUILD_DIR)/* bin/ 2>/dev/null || true
	@echo "Userspace build complete. Binaries available in ./bin/"

driver:
	@echo "=== Building ProcessPilot Pro Linux Kernel Driver ==="
	@if [ -d "/lib/modules/$(shell uname -r)/build" ]; then \
		$(MAKE) -C $(DRIVER_DIR); \
	else \
		echo "Kernel build tree not found. Skipping kernel module compilation."; \
	fi

load_driver:
	@echo "=== Loading ProcessPilot Pro Kernel Driver ==="
	@sudo insmod $(DRIVER_DIR)/pilot_driver.ko 2>/dev/null || echo "Module already loaded or insmod failed"
	@sudo chmod 666 /dev/process_pilot 2>/dev/null || true
	@echo "Driver /dev/process_pilot ready"

unload_driver:
	@echo "=== Unloading ProcessPilot Pro Kernel Driver ==="
	@sudo rmmod pilot_driver 2>/dev/null || echo "Module not loaded"

test: userspace
	@echo "=== Running Automated Unit Test Suite ==="
	@cd $(BUILD_DIR) && ctest --output-on-failure || (./bin/test_dag && ./bin/test_parser && ./bin/test_healer)

demo: userspace
	@bash scripts/demo_self_healing.sh

clean:
	@echo "=== Cleaning Build Artifacts ==="
	@rm -rf $(BUILD_DIR) bin logs /tmp/process_pilot.sock
	@$(MAKE) -C $(DRIVER_DIR) clean 2>/dev/null || true
	@echo "Clean completed."

help:
	@echo "ProcessPilot Pro Build System"
	@echo "  make all           - Build userspace binaries & kernel driver"
	@echo "  make userspace     - Build daemon, CLI, mock services, and tests"
	@echo "  make driver        - Build pilot_driver.ko Linux kernel module"
	@echo "  make load_driver   - Load kernel driver and configure /dev/process_pilot"
	@echo "  make unload_driver - Unload kernel driver"
	@echo "  make test          - Run test suite"
	@echo "  make demo          - Run automated self-healing & chaos injection demo"
	@echo "  make clean         - Remove build artifacts"
