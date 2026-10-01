#!/usr/bin/env bash
# ProcessPilot Pro - Integration Test Suite

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "=========================================================="
echo "          Running ProcessPilot Pro Integration Tests      "
echo "=========================================================="

# Clean previous socket
rm -f /tmp/process_pilot_test.sock
pkill -9 -f "pilot_mock_svc" 2>/dev/null || true
pkill -9 -f "processpilotd" 2>/dev/null || true

# 1. Run Unit Tests First
echo "[TEST 1] Running DAG Unit Tests..."
./bin/test_dag

echo "[TEST 2] Running Parser Unit Tests..."
./bin/test_parser

echo "[TEST 3] Running Self-Healer Unit Tests..."
./bin/test_healer

# 2. Test Daemon IPC & Lifecycle
echo "[TEST 4] Testing Daemon Startup and IPC..."
./bin/processpilotd -c ./services.d -s /tmp/process_pilot_test.sock > /dev/null 2>&1 &
TEST_PID=$!
sleep 2

# Test IPC status query
./bin/pilotctl -s /tmp/process_pilot_test.sock status

# Test IPC tree query
./bin/pilotctl -s /tmp/process_pilot_test.sock tree

# Test start/stop command
./bin/pilotctl -s /tmp/process_pilot_test.sock stop frontend
sleep 1
./bin/pilotctl -s /tmp/process_pilot_test.sock start frontend
sleep 1

# Kill daemon
kill -TERM $TEST_PID
wait $TEST_PID 2>/dev/null || true
rm -f /tmp/process_pilot_test.sock

echo "=========================================================="
echo "          All Integration Tests Passed!                   "
echo "=========================================================="
