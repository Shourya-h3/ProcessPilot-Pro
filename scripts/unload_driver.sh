#!/usr/bin/env bash
# ProcessPilot Pro - Kernel Module Unload Script

set -e

echo "=== Unloading ProcessPilot Pro Kernel Driver ==="

if lsmod | grep -q "pilot_driver"; then
    echo "Removing pilot_driver kernel module..."
    sudo rmmod pilot_driver
    echo "pilot_driver unloaded successfully."
else
    echo "pilot_driver is not loaded."
fi
