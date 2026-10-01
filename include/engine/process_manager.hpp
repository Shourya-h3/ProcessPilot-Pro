/*
 * ProcessPilot Pro - Linux Process Lifecycle & cgroups v2 Manager
 * File: include/engine/process_manager.hpp
 */

#ifndef PROCESS_PILOT_PROCESS_MANAGER_HPP
#define PROCESS_PILOT_PROCESS_MANAGER_HPP

#include "common/pilot_types.hpp"
#include "common/error_codes.hpp"
#include <string>
#include <map>
#include <memory>
#include <mutex>

namespace process_pilot {

struct ChildProcessExitInfo {
    pid_t pid;
    int exit_code;
    int term_signal;
    bool exited_normally;
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    void set_log_directory(const std::string& dir);

    /* Spawn a configured service */
    PilotStatus spawn_service(const ServiceConfig& config, pid_t& out_pid);

    /* Terminate a service (graceful SIGTERM -> wait timeout -> SIGKILL) */
    PilotStatus stop_service(const std::string& service_name, pid_t pid, uint32_t timeout_ms = 3000);

    /* Force kill a service (SIGKILL) */
    PilotStatus kill_service(pid_t pid);

    /* Non-blocking harvest of exited child processes */
    std::vector<ChildProcessExitInfo> reap_zombies();

    /* Apply cgroups v2 limits to PID */
    PilotStatus apply_cgroup_limits(const std::string& service_name, pid_t pid, const ResourceLimits& limits);

    /* Sample resource utilization for a PID (reads /proc/<pid>/stat and /proc/<pid>/status) */
    PilotStatus sample_resource_usage(pid_t pid, uint64_t& out_rss_bytes, double& out_cpu_pct);

    /* Get stdout/stderr log path for service */
    std::string get_service_log_path(const std::string& service_name) const;

private:
    std::string log_dir_ = "./logs";
    std::string cgroup_base_path_ = "/sys/fs/cgroup/processpilot";
    bool cgroups_available_ = false;
    std::mutex proc_mutex_;

    /* Previous CPU sample tracking for computing CPU % */
    struct CpuSample {
        uint64_t utime = 0;
        uint64_t stime = 0;
        std::chrono::steady_clock::time_point sample_time;
    };
    std::map<pid_t, CpuSample> prev_cpu_samples_;

    void init_cgroups();
    std::vector<std::string> tokenize_cmd(const std::string& cmd_str);
};

} // namespace process_pilot

#endif // PROCESS_PILOT_PROCESS_MANAGER_HPP
