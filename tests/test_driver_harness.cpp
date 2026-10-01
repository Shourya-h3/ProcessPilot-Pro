/*
 * ProcessPilot Pro - Linux Kernel Driver Test Harness
 * File: tests/test_driver_harness.cpp
 */

#include "driver/pilot_ioctl.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cassert>
#include <thread>
#include <chrono>

int main() {
    std::cout << "================ Linux Driver Test Harness (/dev/process_pilot) ================" << std::endl;

    int fd = open(PILOT_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        std::cerr << "[SKIP] Driver node " << PILOT_DEVICE_PATH << " is not accessible ("
                  << strerror(errno) << "). Run 'sudo make load_driver' to insert module.\n";
        return 0;
    }

    std::cout << "[+] Opened " << PILOT_DEVICE_PATH << " (FD: " << fd << ")\n";

    pid_t my_pid = getpid();

    // 1. Register Watchdog
    struct pilot_watchdog_reg reg;
    memset(&reg, 0, sizeof(reg));
    reg.pid = my_pid;
    reg.timeout_ms = 800;
    reg.max_misses = 3;
    strncpy(reg.service_name, "test_harness_svc", sizeof(reg.service_name) - 1);

    if (ioctl(fd, PILOT_IOCTL_REGISTER_WATCHDOG, &reg) < 0) {
        std::cerr << "[-] PILOT_IOCTL_REGISTER_WATCHDOG failed: " << strerror(errno) << "\n";
        close(fd);
        return 1;
    }
    std::cout << "[+] Registered Watchdog for PID " << my_pid << " with timeout: " << reg.timeout_ms << " ms\n";

    // 2. Send 3 periodic heartbeats
    for (int i = 1; i <= 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        struct pilot_heartbeat_ping ping;
        memset(&ping, 0, sizeof(ping));
        ping.pid = my_pid;
        ping.sequence_num = i;

        if (ioctl(fd, PILOT_IOCTL_PING_HEARTBEAT, &ping) < 0) {
            std::cerr << "[-] Heartbeat ping #" << i << " failed: " << strerror(errno) << "\n";
        } else {
            std::cout << "[+] Sent Heartbeat ping #" << i << "\n";
        }
    }

    // 3. Query Driver Stats
    struct pilot_driver_stats stats;
    if (ioctl(fd, PILOT_IOCTL_GET_STATS, &stats) < 0) {
        std::cerr << "[-] PILOT_IOCTL_GET_STATS failed: " << strerror(errno) << "\n";
    } else {
        std::cout << "[+] Driver Stats:\n"
                  << "    Version: " << stats.driver_version_major << "." << stats.driver_version_minor << "\n"
                  << "    Active Watchdogs: " << stats.active_watchdogs_count << "\n"
                  << "    Heartbeats Received: " << stats.total_heartbeats_received << "\n"
                  << "    Registered Watchdogs: " << stats.total_watchdogs_registered << "\n";
    }

    // 4. Unregister Watchdog
    if (ioctl(fd, PILOT_IOCTL_UNREGISTER_WATCHDOG, &my_pid) < 0) {
        std::cerr << "[-] PILOT_IOCTL_UNREGISTER_WATCHDOG failed: " << strerror(errno) << "\n";
    } else {
        std::cout << "[+] Unregistered Watchdog for PID " << my_pid << "\n";
    }

    close(fd);
    std::cout << "[PASS] Driver IOCTL Test Harness completed successfully.\n";
    return 0;
}
