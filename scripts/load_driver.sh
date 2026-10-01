#!/usr/bin/env bash
# ProcessPilot Pro - Kernel Module Load Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRIVER_DIR="$(cd "${SCRIPT_DIR}/../src/driver" && pwd)"

echo "=== Loading ProcessPilot Pro Kernel Driver ==="

if [ ! -f "${DRIVER_DIR}/pilot_driver.ko" ]; then
    echo "Compiling pilot_driver.ko..."
    make -C "${DRIVER_DIR}"
fi

# Insert module
if lsmod | grep -q "pilot_driver"; then
    echo "pilot_driver is already loaded."
else
    echo "Inserting pilot_driver.ko into kernel..."
    sudo insmod "${DRIVER_DIR}/pilot_driver.ko"
fi

# Ensure permissions on device node
if [ -e "/dev/process_pilot" ]; then
    sudo chmod 666 /dev/process_pilot
    echo "Device node /dev/process_pilot is ready (Permissions: 0666)."
fi

# Display sysfs status
if [ -f "/sys/class/process_pilot_class/stats" ]; then
    echo "Sysfs status:"
    cat /sys/class/process_pilot_class/stats
fi
