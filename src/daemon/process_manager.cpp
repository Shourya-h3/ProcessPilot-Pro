/*
 * ProcessPilot Pro - Linux Process Lifecycle & cgroups v2 Manager Implementation
 * File: src/daemon/process_manager.cpp
 */

#include "engine/process_manager.hpp"
#include "common/logger.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace process_pilot {

ProcessManager::ProcessManager() {
    init_cgroups();
}

ProcessManager::~ProcessManager() {
}

void ProcessManager::set_log_directory(const std::string& dir) {
    log_dir_ = dir;
    try {
        if (!fs::exists(log_dir_)) {
            fs::create_directories(log_dir_);
        }
    } catch (const std::exception& e) {
        LOG_WARN("Could not create log directory: " + std::string(e.what()));
    }
}

std::string ProcessManager::get_service_log_path(const std::string& service_name) const {
    return log_dir_ + "/" + service_name + ".log";
}

void ProcessManager::init_cgroups() {
#ifndef _WIN32
    if (fs::exists("/sys/fs/cgroup/cgroup.controllers")) {
        try {
            if (!fs::exists(cgroup_base_path_)) {
                fs::create_directories(cgroup_base_path_);
            }
            cgroups_available_ = true;
            LOG_INFO("cgroups v2 initialized at: " + cgroup_base_path_);
        } catch (const std::exception& e) {
            LOG_WARN("cgroups v2 available but could not create root slice: " + std::string(e.what()));
            cgroups_available_ = false;
        }
    } else {
        LOG_DEBUG("cgroups v2 not available or not mounted; resource isolation will run in standard POSIX mode");
        cgroups_available_ = false;
    }
#else
    cgroups_available_ = false;
#endif
}

std::vector<std::string> ProcessManager::tokenize_cmd(const std::string& cmd_str) {
    std::vector<std::string> tokens;
    std::istringstream iss(cmd_str);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

PilotStatus ProcessManager::spawn_service(const ServiceConfig& config, pid_t& out_pid) {
    std::lock_guard<std::mutex> lock(proc_mutex_);

    std::vector<std::string> tokens = tokenize_cmd(config.exec_start);
    if (tokens.empty()) {
        LOG_ERROR("Cannot spawn service '" + config.name + "': Empty ExecStart command");
        return PilotStatus::ERR_CONFIG_INVALID;
    }

#ifndef _WIN32
    std::string log_path = get_service_log_path(config.name);
    try {
        if (!fs::exists(log_dir_)) {
            fs::create_directories(log_dir_);
        }
    } catch (...) {}

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("fork() failed for service '" + config.name + "': " + std::string(strerror(errno)));
        return PilotStatus::ERR_PROCESS_FORK_FAILED;
    }

    if (pid == 0) {
        // --- Child Process ---
        setpgid(0, 0); // Put child in its own process group

        // Change working directory if specified
        if (!config.working_dir.empty()) {
            if (chdir(config.working_dir.c_str()) != 0) {
                // Ignore failure or fallback to /
            }
        }

        // Set environment variables
        for (const auto& env : config.environment) {
            setenv(env.first.c_str(), env.second.c_str(), 1);
        }

        // Set standard ProcessPilot runtime env variables
        setenv("PILOT_SERVICE_NAME", config.name.c_str(), 1);

        // Redirect stdout/stderr to log file
        int log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        // Prepare exec arguments
        std::vector<char*> argv;
        for (const auto& t : tokens) {
            argv.push_back(const_cast<char*>(t.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        // If execvp returns, an error occurred
        std::cerr << "execvp failed for " << argv[0] << ": " << strerror(errno) << std::endl;
        _exit(127);
    }

    // --- Parent Process ---
    out_pid = pid;
    LOG_INFO("Successfully spawned service '" + config.name + "' with PID " + std::to_string(pid));

    // Apply cgroups v2 limits if configured
    if (cgroups_available_) {
        apply_cgroup_limits(config.name, pid, config.resources);
    }

    return PilotStatus::SUCCESS;
#else
    // Simulated PID for non-Linux build environments
    out_pid = 1000 + (rand() % 9000);
    LOG_INFO("[SIM] Spawned mock service '" + config.name + "' with PID " + std::to_string(out_pid));
    return PilotStatus::SUCCESS;
#endif
}

PilotStatus ProcessManager::stop_service(const std::string& service_name, pid_t pid, uint32_t timeout_ms) {
    if (pid <= 0) return PilotStatus::ERR_PROCESS_NOT_FOUND;

    LOG_INFO("Stopping service '" + service_name + "' (PID " + std::to_string(pid) + ")...");

#ifndef _WIN32
    // Send SIGTERM to the process group
    kill(-pid, SIGTERM);
    kill(pid, SIGTERM);

    uint32_t elapsed = 0;
    const uint32_t interval = 50;

    while (elapsed < timeout_ms) {
        int status;
        pid_t res = waitpid(pid, &status, WNOHANG);
        if (res == pid || (res == -1 && errno == ECHILD)) {
            LOG_INFO("Service '" + service_name + "' (PID " + std::to_string(pid) + ") exited cleanly");
            return PilotStatus::SUCCESS;
        }

        if (kill(pid, 0) != 0 && errno == ESRCH) {
            return PilotStatus::SUCCESS;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        elapsed += interval;
    }

    LOG_WARN("Service '" + service_name + "' (PID " + std::to_string(pid) + ") did not stop gracefully; sending SIGKILL");
    kill(-pid, SIGKILL);
    kill(pid, SIGKILL);
#else
    LOG_INFO("[SIM] Service '" + service_name + "' stopped.");
#endif

    return PilotStatus::SUCCESS;
}

PilotStatus ProcessManager::kill_service(pid_t pid) {
    if (pid <= 0) return PilotStatus::ERR_PROCESS_NOT_FOUND;
#ifndef _WIN32
    kill(-pid, SIGKILL);
    kill(pid, SIGKILL);
#endif
    return PilotStatus::SUCCESS;
}

std::vector<ChildProcessExitInfo> ProcessManager::reap_zombies() {
    std::vector<ChildProcessExitInfo> exited;
#ifndef _WIN32
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        ChildProcessExitInfo info;
        info.pid = pid;
        if (WIFEXITED(status)) {
            info.exited_normally = true;
            info.exit_code = WEXITSTATUS(status);
            info.term_signal = 0;
        } else if (WIFSIGNALED(status)) {
            info.exited_normally = false;
            info.exit_code = -1;
            info.term_signal = WTERMSIG(status);
        } else {
            info.exited_normally = false;
            info.exit_code = -1;
            info.term_signal = 0;
        }
        exited.push_back(info);
    }
#endif
    return exited;
}

PilotStatus ProcessManager::apply_cgroup_limits(const std::string& service_name, pid_t pid, const ResourceLimits& limits) {
    if (!cgroups_available_ || pid <= 0) {
        return PilotStatus::SUCCESS;
    }

#ifndef _WIN32
    std::string svc_cgroup = cgroup_base_path_ + "/" + service_name;
    try {
        if (!fs::exists(svc_cgroup)) {
            fs::create_directories(svc_cgroup);
        }

        // Attach PID to cgroup.procs
        std::ofstream procs_file(svc_cgroup + "/cgroup.procs");
        if (procs_file.is_open()) {
            procs_file << pid << std::endl;
        }

        // Apply Memory limit
        if (limits.memory_max_bytes > 0) {
            std::ofstream mem_file(svc_cgroup + "/memory.max");
            if (mem_file.is_open()) {
                mem_file << limits.memory_max_bytes << std::endl;
            }
        }

        // Apply CPU quota (e.g. 50% => "50000 100000")
        if (limits.cpu_quota_pct > 0) {
            std::ofstream cpu_file(svc_cgroup + "/cpu.max");
            if (cpu_file.is_open()) {
                uint64_t quota = limits.cpu_quota_pct * 1000;
                cpu_file << quota << " 100000" << std::endl;
            }
        }

        // Apply Max PIDs
        if (limits.max_pids > 0) {
            std::ofstream pids_file(svc_cgroup + "/pids.max");
            if (pids_file.is_open()) {
                pids_file << limits.max_pids << std::endl;
            }
        }

        LOG_DEBUG("Applied cgroups v2 resource limits for '" + service_name + "' (PID " + std::to_string(pid) + ")");
    } catch (const std::exception& e) {
        LOG_WARN("Failed setting cgroups limits for '" + service_name + "': " + std::string(e.what()));
        return PilotStatus::ERR_CGROUP_SETUP_FAILED;
    }
#endif
    return PilotStatus::SUCCESS;
}

PilotStatus ProcessManager::sample_resource_usage(pid_t pid, uint64_t& out_rss_bytes, double& out_cpu_pct) {
    out_rss_bytes = 0;
    out_cpu_pct = 0.0;

    if (pid <= 0) return PilotStatus::ERR_PROCESS_NOT_FOUND;

#ifndef _WIN32
    // 1. Read RSS memory from /proc/<pid>/status
    std::string status_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_file(status_path);
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                std::istringstream iss(line);
                std::string key, unit;
                uint64_t val_kb;
                if (iss >> key >> val_kb) {
                    out_rss_bytes = val_kb * 1024;
                }
                break;
            }
        }
    } else {
        return PilotStatus::ERR_PROCESS_NOT_FOUND;
    }

    // 2. Read CPU ticks from /proc/<pid>/stat
    std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream stat_file(stat_path);
    if (stat_file.is_open()) {
        std::string line;
        if (std::getline(stat_file, line)) {
            std::istringstream iss(line);
            std::string token;
            int field_idx = 1;
            uint64_t utime = 0, stime = 0;

            while (iss >> token) {
                if (field_idx == 14) utime = std::stoull(token);
                else if (field_idx == 15) stime = std::stoull(token);
                field_idx++;
            }

            auto now = std::chrono::steady_clock::now();
            auto it = prev_cpu_samples_.find(pid);
            if (it != prev_cpu_samples_.end()) {
                double elapsed_sec = std::chrono::duration<double>(now - it->second.sample_time).count();
                if (elapsed_sec > 0.05) {
                    uint64_t delta_ticks = (utime - it->second.utime) + (stime - it->second.stime);
                    long ticks_per_sec = sysconf(_SC_CLK_TCK);
                    if (ticks_per_sec > 0) {
                        out_cpu_pct = (static_cast<double>(delta_ticks) / ticks_per_sec) / elapsed_sec * 100.0;
                    }
                }
            }

            prev_cpu_samples_[pid] = CpuSample{utime, stime, now};
        }
    }
#endif
    return PilotStatus::SUCCESS;
}

} // namespace process_pilot
