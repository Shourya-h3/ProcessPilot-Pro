/*
 * ProcessPilot Pro - Multi-Modal Health Checker & Driver Watchdog Client
 * File: include/engine/health_checker.hpp
 */

#ifndef PROCESS_PILOT_HEALTH_CHECKER_HPP
#define PROCESS_PILOT_HEALTH_CHECKER_HPP

#include "common/pilot_types.hpp"
#include "common/error_codes.hpp"
#include "driver/pilot_ioctl.h"

#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>

namespace process_pilot {

class HealthChecker {
public:
    HealthChecker();
    ~HealthChecker();

    /* Initialize connection to /dev/process_pilot character device */
    PilotStatus init_driver();

    /* Close connection to /dev/process_pilot */
    void close_driver();

    /* Register a process watchdog in the Linux kernel driver */
    PilotStatus register_kernel_watchdog(pid_t pid, const std::string& service_name, uint32_t timeout_ms, uint32_t max_misses = 3);

    /* Send heartbeat ping to kernel driver */
    PilotStatus ping_kernel_watchdog(pid_t pid);

    /* Unregister process watchdog from kernel driver */
    PilotStatus unregister_kernel_watchdog(pid_t pid);

    /* Query driver telemetry stats */
    PilotStatus get_driver_stats(struct pilot_driver_stats& out_stats);

    /* Inject fault via kernel driver */
    PilotStatus inject_driver_fault(pid_t pid, uint32_t fault_type, uint32_t delay_ms = 0);

    /* Execute single health probe for a service */
    bool execute_probe(const HealthCheckConfig& config, pid_t pid, std::string& out_reason);

    /* Check if kernel driver is loaded and functional */
    bool is_driver_available() const { return driver_fd_ >= 0; }

private:
    int driver_fd_ = -1;
    std::mutex driver_mutex_;
    std::atomic<uint32_t> heartbeat_seq_{0};

    bool check_tcp_port(const std::string& endpoint, uint32_t timeout_ms);
    bool check_http_endpoint(const std::string& endpoint, uint32_t timeout_ms);
    bool check_exec_command(const std::string& command, uint32_t timeout_ms);
};

} // namespace process_pilot

#endif // PROCESS_PILOT_HEALTH_CHECKER_HPP
