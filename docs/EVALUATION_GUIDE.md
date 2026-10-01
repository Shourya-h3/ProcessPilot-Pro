# ProcessPilot Pro: Capstone Evaluation & Presentation Guide

This guide is designed for the **5–10 minute evaluation interview** with your technical evaluator/trainer.

---

## ⏱️ Suggested 8-Minute Evaluation Schedule

| Time | Segment | Focus |
| :--- | :--- | :--- |
| **0:00 - 1:30** | **Problem Statement & Core Architecture** | Explain service orchestration challenges, single points of failure, and why integrating a Linux Device Driver provides out-of-band resilience. |
| **1:30 - 3:00** | **Kernel Driver Design (`pilot_driver`)** | Present `/dev/process_pilot`, `struct timer_list` watchdogs, ring buffer concurrency, and IOCTL contracts. |
| **3:00 - 6:00** | **Live Demonstration** | Run `make demo` to demonstrate DAG parallel startup, live telemetry, `kill -9` crash recovery, and circuit breaker trip. |
| **6:00 - 8:00** | **Q&A and Code Walkthrough** | Answer evaluator questions regarding cgroups v2, Kahn's algorithm, and POSIX signal isolation. |

---

## 🎯 1. Elevator Pitch
> *"ProcessPilot Pro is a high-performance Linux service supervisor written strictly in C++17 and C. Unlike traditional supervisors that run purely in userspace, ProcessPilot Pro integrates a dedicated Linux Character Device Driver (`/dev/process_pilot`). This provides hardware-like kernel watchdogs that can detect deadlocks even if userspace freezes, paired with a DAG dependency resolver and a self-healing runtime featuring jittered exponential backoffs and circuit breakers."*

---

## 🎬 2. Step-by-Step Live Demo Walkthrough

### Step 1: Clean Build & Unit Tests (30 seconds)
```bash
make clean
make all
make test
```
**Talking Point:** *"Notice that our entire build is written in pure C++17 and C with zero third-party dependencies, compiling both userspace daemons and the kernel module."*

---

### Step 2: Launch Daemon & Inspect DAG Graph (1 minute)
```bash
# Start daemon in background
./bin/processpilotd -c ./services.d &

# View DAG execution tree
./bin/pilotctl tree
```
**Talking Point:** *"ProcessPilot parses our `.pilot` unit files, builds a Directed Acyclic Graph, detects any circular dependencies using Kahn's algorithm, and computes parallel execution tiers to boot independent services concurrently."*

---

### Step 3: Inspect Live Telemetry & Driver Stats (1 minute)
```bash
# View live services
./bin/pilotctl status

# Query Linux Kernel Module stats
./bin/pilotctl driver-stats
```
**Talking Point:** *"The status table reports live CPU percentage, resident memory (RSS), uptime, and restart history sampled directly from `/proc` and cgroups v2. We can also query the kernel driver to verify registered watchdogs and heartbeat counters."*

---

### Step 4: Chaos Injection & Self-Healing Verification (1.5 minutes)
```bash
# 1. Kill the database service violently
pkill -9 -f "pilot_mock_svc database"

# 2. Check status immediately
./bin/pilotctl status
```
**Talking Point:** *"The supervisor instantly reaped the zombie process via `waitpid(WNOHANG)`, identified the unexpected termination, calculated an exponential backoff with randomized jitter, and automatically respawned the database service."*

```bash
# 3. Simulate a continuous crash loop (triggering circuit breaker)
for i in {1..5}; do pkill -9 -f "pilot_mock_svc database"; sleep 0.3; done

# 4. Check status
./bin/pilotctl status
```
**Talking Point:** *"Because the database crashed 5 times within 60 seconds, our Circuit Breaker tripped, entering `CIRCUIT_BROKEN` state. This prevents fork-bomb CPU exhaustion and cascading failures."*

```bash
# 5. Reset circuit and restart cleanly
./bin/pilotctl reset-circuit database
./bin/pilotctl restart database
./bin/pilotctl status
```

---

## 💡 3. Key Evaluator Questions & Answers

### Q1: Why implement a custom Linux Device Driver instead of doing everything in userspace?
**Answer:** Userspace watchdogs fail when the supervisor itself experiences priority inversion, scheduler starvation, or a thread deadlock. The `pilot_driver.ko` module runs in kernel space (`timer_list` in softirq context), guaranteeing out-of-band monitoring and reliable hardware/kernel crash telemetry.

### Q2: How does the DAG resolve dependencies and detect cycles?
**Answer:** We implemented Kahn's topological sort algorithm. We compute in-degrees for all service nodes. Nodes with in-degree 0 are queued into Tier 0. As nodes finish, we decrement dependent in-degrees. If the final resolved node count is less than the total node count, DFS traces the exact cycle path (e.g. `A -> B -> C -> A`) and rejects the configuration before spawning.

### Q3: How do you prevent zombie processes and stray child processes?
**Answer:** 
1. Every child process is assigned its own process group via `setpgid(0, 0)`. When terminating, signals are dispatched to `-pid` to kill the entire group.
2. The supervisor runs a non-blocking `waitpid(-1, &status, WNOHANG)` loop to harvest dead processes and decode termination codes without blocking the event loop.

### Q4: How is resource throttling enforced?
**Answer:** When available, ProcessPilot connects to the Linux **cgroups v2** unified hierarchy (`/sys/fs/cgroup/processpilot/<service>`), writing hard ceilings to `memory.max`, `cpu.max`, and `pids.max`.
