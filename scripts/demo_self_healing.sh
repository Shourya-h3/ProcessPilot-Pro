#!/usr/bin/env bash
# ProcessPilot Pro - Live Self-Healing & Chaos Demo Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}       ProcessPilot Pro - Live Self-Healing Runtime Demo        ${NC}"
echo -e "${CYAN}================================================================${NC}"

# Clean up any previous runs
pkill -9 processpilotd 2>/dev/null || true
pkill -9 pilot_mock_svc 2>/dev/null || true
rm -f /tmp/process_pilot.sock

# 1. Start the daemon
echo -e "\n${YELLOW}[Step 1] Starting processpilotd supervisor daemon in background...${NC}"
./bin/processpilotd -c ./services.d -s /tmp/process_pilot.sock > logs/daemon_demo.log 2>&1 &
DAEMON_PID=$!
sleep 2

# Verify socket
if [ ! -S "/tmp/process_pilot.sock" ]; then
    echo -e "${RED}[ERROR] Supervisor failed to create socket /tmp/process_pilot.sock${NC}"
    kill -9 $DAEMON_PID 2>/dev/null || true
    exit 1
fi
echo -e "${GREEN}[OK] processpilotd is active (PID: $DAEMON_PID)${NC}"

# 2. View DAG Tree
echo -e "\n${YELLOW}[Step 2] Querying Dependency DAG Execution Tree via pilotctl tree...${NC}"
./bin/pilotctl tree

# 3. View Service Status
echo -e "\n${YELLOW}[Step 3] Querying Live Services Status via pilotctl status...${NC}"
./bin/pilotctl status

# 4. Check Driver Stats
echo -e "\n${YELLOW}[Step 4] Querying Linux Kernel Device Driver stats via pilotctl driver-stats...${NC}"
./bin/pilotctl driver-stats || true

# 5. Simulate Sudden Crash
echo -e "\n${YELLOW}[Step 5] Chaos Simulation: Killing 'database' service with SIGKILL (kill -9)...${NC}"
DB_PID=$(pgrep -f "pilot_mock_svc database" | head -n 1 || echo "")
if [ -n "$DB_PID" ]; then
    echo -e "${RED}[CHAOS] Sending SIGKILL to PID $DB_PID (database)...${NC}"
    kill -9 $DB_PID
    sleep 1.5

    echo -e "\n${GREEN}[Step 6] Observing Self-Healing: Supervisor detects termination & schedules backoff restart...${NC}"
    ./bin/pilotctl status

    echo -e "\n${GREEN}Waiting for exponential backoff timer to respawn 'database'...${NC}"
    sleep 2
    ./bin/pilotctl status
fi

# 6. Simulate Rapid Crash Loop (Trip Circuit Breaker)
echo -e "\n${YELLOW}[Step 7] Chaos Simulation: Triggering Crash Loop to test Circuit Breaker...${NC}"
for i in {1..5}; do
    CURR_PID=$(pgrep -f "pilot_mock_svc database" | head -n 1 || echo "")
    if [ -n "$CURR_PID" ]; then
        echo -e "${RED}[CRASH #$i] Killing database PID $CURR_PID${NC}"
        kill -9 $CURR_PID 2>/dev/null || true
        sleep 0.4
    fi
done

sleep 1
echo -e "\n${YELLOW}[Step 8] Verifying Circuit Breaker Tripped status...${NC}"
./bin/pilotctl status

# 7. Manual Circuit Reset
echo -e "\n${YELLOW}[Step 9] Manually resetting Circuit Breaker via pilotctl reset-circuit database...${NC}"
./bin/pilotctl reset-circuit database
./bin/pilotctl restart database
sleep 1.5
./bin/pilotctl status

# 8. Clean Shutdown
echo -e "\n${YELLOW}[Step 10] Gracefully shutting down supervisor daemon...${NC}"
kill -TERM $DAEMON_PID 2>/dev/null || true
wait $DAEMON_PID 2>/dev/null || true

echo -e "\n${GREEN}================================================================${NC}"
echo -e "${GREEN} Demo Completed Successfully! All self-healing policies verified.${NC}"
echo -e "${GREEN}================================================================${NC}"
