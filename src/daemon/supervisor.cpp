/*
 * ProcessPilot Pro - Central Supervisor Daemon Engine Implementation
 * File: src/daemon/supervisor.cpp
 */

#include "daemon/supervisor.hpp"
#include "common/logger.hpp"

#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <algorithm>

namespace process_pilot {

Supervisor::Supervisor(const std::string& config_dir, const std::string& socket_path)
    : config_dir_(config_dir),
      socket_path_(socket_path),
      ipc_server_(socket_path) {
}

Supervisor::~Supervisor() {
    stop();
}

PilotStatus Supervisor::init() {
    LOG_INFO("Initializing ProcessPilot Pro Supervisor Daemon...");

    std::vector<ServiceConfig> configs;
    PilotStatus status = ConfigParser::parse_directory(config_dir_, configs);
    if (status != PilotStatus::SUCCESS && configs.empty()) {
        LOG_WARN("No valid service configurations found in " + config_dir_);
    }

    status = dag_resolver_.build_graph(configs);
    if (status != PilotStatus::SUCCESS) {
        LOG_ERROR("Failed to resolve dependency graph: " + status_to_string(status));
        return status;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    services_.clear();
    for (const auto& cfg : configs) {
        ManagedService svc;
        svc.config = cfg;
        svc.stats.state = ServiceState::INACTIVE;
        services_[cfg.name] = svc;
    }

    LOG_INFO("Successfully loaded " + std::to_string(services_.size()) + " service units into DAG engine");
    return PilotStatus::SUCCESS;
}

PilotStatus Supervisor::start() {
    if (running_) {
        return PilotStatus::SUCCESS;
    }

    running_ = true;

    // Start IPC Server
    ipc_server_.start([this](const IpcRequest& req) {
        return this->handle_ipc_request(req);
    });

    // Compute parallel execution tiers and spawn services
    std::vector<std::vector<std::string>> tiers;
    if (dag_resolver_.get_execution_tiers(tiers) == PilotStatus::SUCCESS) {
        for (size_t lvl = 0; lvl < tiers.size(); ++lvl) {
            LOG_INFO("Spawning Execution Tier " + std::to_string(lvl) + " (" + std::to_string(tiers[lvl].size()) + " services)...");
            for (const auto& svc_name : tiers[lvl]) {
                auto it = services_.find(svc_name);
                if (it != services_.end() && it->second.config.auto_start) {
                    start_service(svc_name);
                }
            }
        }
    }

    // Launch background supervision thread
    supervisor_thread_ = std::thread(&Supervisor::supervision_loop, this);
    LOG_INFO("ProcessPilot Pro Supervisor Daemon is running");
    return PilotStatus::SUCCESS;
}

void Supervisor::stop() {
    if (!running_) return;

    LOG_INFO("Stopping ProcessPilot Pro Supervisor Daemon...");
    running_ = false;

    ipc_server_.stop();

    if (supervisor_thread_.joinable()) {
        supervisor_thread_.join();
    }

    // Stop all child services
    std::lock_guard<std::mutex> lock(state_mutex_);
    for (auto& pair : services_) {
        if (pair.second.stats.pid > 0) {
            health_checker_.unregister_kernel_watchdog(pair.second.stats.pid);
            process_mgr_.stop_service(pair.first, pair.second.stats.pid);
            pair.second.stats.pid = 0;
            pair.second.stats.state = ServiceState::STOPPED;
        }
    }

    LOG_INFO("Supervisor Daemon stopped cleanly.");
}

PilotStatus Supervisor::start_service_recursive(const std::string& name) {
    auto it = services_.find(name);
    if (it == services_.end()) return PilotStatus::ERR_PROCESS_NOT_FOUND;

    // Ensure all required dependencies are healthy or starting
    for (const auto& dep : it->second.config.dependencies) {
        std::string dep_clean = dep;
        if (dep_clean.size() > 6 && dep_clean.substr(dep_clean.size() - 6) == ".pilot") {
            dep_clean = dep_clean.substr(0, dep_clean.size() - 6);
        }

        auto dep_it = services_.find(dep_clean);
        if (dep_it == services_.end()) {
            LOG_ERROR("Cannot start '" + name + "': Missing dependency '" + dep_clean + "'");
            return PilotStatus::ERR_DAG_UNKNOWN_DEP;
        }

        if (dep_it->second.stats.state != ServiceState::HEALTHY &&
            dep_it->second.stats.state != ServiceState::STARTING) {
            LOG_INFO("Starting prerequisite dependency '" + dep_clean + "' for '" + name + "'...");
            PilotStatus dep_status = start_service_recursive(dep_clean);
            if (dep_status != PilotStatus::SUCCESS) {
                return dep_status;
            }
        }
    }

    // Spawn process
    pid_t pid = 0;
    PilotStatus status = process_mgr_.spawn_service(it->second.config, pid);
    if (status == PilotStatus::SUCCESS) {
        it->second.stats.pid = pid;
        it->second.stats.state = ServiceState::HEALTHY;
        it->second.stats.start_time = std::chrono::system_clock::now();
        it->second.last_health_check = std::chrono::steady_clock::now();
        it->second.consecutive_probe_failures = 0;

        // Register in Kernel Watchdog driver if configured
        if (it->second.config.health_check.type == HealthCheckType::KERNEL_WATCHDOG) {
            health_checker_.register_kernel_watchdog(
                pid,
                name,
                it->second.config.health_check.kernel_watchdog_timeout_ms,
                it->second.config.health_check.max_retries
            );
        }

        self_healer_.record_healthy(name);
    }
    return status;
}

PilotStatus Supervisor::start_service(const std::string& name) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return start_service_recursive(name);
}

PilotStatus Supervisor::stop_service(const std::string& name) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto it = services_.find(name);
    if (it == services_.end()) return PilotStatus::ERR_PROCESS_NOT_FOUND;

    if (it->second.stats.pid > 0) {
        health_checker_.unregister_kernel_watchdog(it->second.stats.pid);
        process_mgr_.stop_service(name, it->second.stats.pid);
        it->second.stats.pid = 0;
    }
    it->second.stats.state = ServiceState::STOPPED;
    return PilotStatus::SUCCESS;
}

PilotStatus Supervisor::restart_service(const std::string& name) {
    stop_service(name);
    self_healer_.reset_circuit_breaker(name);
    return start_service(name);
}

PilotStatus Supervisor::reload_configs() {
    LOG_INFO("Reloading service configurations from " + config_dir_ + "...");
    std::vector<ServiceConfig> configs;
    PilotStatus status = ConfigParser::parse_directory(config_dir_, configs);
    if (status != PilotStatus::SUCCESS) return status;

    status = dag_resolver_.build_graph(configs);
    if (status != PilotStatus::SUCCESS) return status;

    std::lock_guard<std::mutex> lock(state_mutex_);
    for (const auto& cfg : configs) {
        if (services_.find(cfg.name) != services_.end()) {
            services_[cfg.name].config = cfg;
        } else {
            ManagedService svc;
            svc.config = cfg;
            svc.stats.state = ServiceState::INACTIVE;
            services_[cfg.name] = svc;
        }
    }
    LOG_INFO("Service configurations reloaded successfully");
    return PilotStatus::SUCCESS;
}

void Supervisor::supervision_loop() {
    while (running_) {
        handle_reaped_processes();
        handle_health_probes();
        handle_backoff_restarts();
        sample_system_metrics();

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void Supervisor::handle_reaped_processes() {
    auto dead_procs = process_mgr_.reap_zombies();

    std::lock_guard<std::mutex> lock(state_mutex_);
    for (const auto& info : dead_procs) {
        for (auto& pair : services_) {
            if (pair.second.stats.pid == info.pid) {
                std::string name = pair.first;
                LOG_WARN("Service '" + name + "' (PID " + std::to_string(info.pid) + ") terminated. Exit code: " +
                         std::to_string(info.exit_code) + ", signal: " + std::to_string(info.term_signal));

                health_checker_.unregister_kernel_watchdog(info.pid);
                pair.second.stats.pid = 0;
                pair.second.stats.last_crash_time = std::chrono::system_clock::now();
                pair.second.stats.restart_count++;

                self_healer_.record_crash(name, pair.second.config.healing);

                if (self_healer_.is_circuit_broken(name)) {
                    pair.second.stats.state = ServiceState::CIRCUIT_BROKEN;
                    pair.second.stats.last_error = "Circuit breaker tripped: Excessive crashes";
                } else {
                    uint32_t delay_ms = 0;
                    std::string reason;
                    if (self_healer_.should_restart(pair.second.config, info.exit_code, info.exited_normally, delay_ms, reason)) {
                        pair.second.stats.state = ServiceState::BACKOFF_WAIT;
                        pair.second.stats.current_backoff_ms = delay_ms;
                        LOG_INFO("Scheduled self-healing restart for '" + name + "' in " + std::to_string(delay_ms) + " ms");
                    } else {
                        pair.second.stats.state = ServiceState::FAILED;
                        pair.second.stats.last_error = reason;
                    }
                }
                break;
            }
        }
    }
}

void Supervisor::handle_health_probes() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto now = std::chrono::steady_clock::now();

    for (auto& pair : services_) {
        if (pair.second.stats.state != ServiceState::HEALTHY &&
            pair.second.stats.state != ServiceState::DEGRADED) {
            continue;
        }

        if (pair.second.stats.pid <= 0) continue;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - pair.second.last_health_check).count();
        if (elapsed >= pair.second.config.health_check.interval_ms) {
            pair.second.last_health_check = now;

            std::string reason;
            bool ok = health_checker_.execute_probe(pair.second.config.health_check, pair.second.stats.pid, reason);
            if (ok) {
                pair.second.consecutive_probe_failures = 0;
                pair.second.stats.state = ServiceState::HEALTHY;
                pair.second.stats.total_heartbeats_sent++;
            } else {
                pair.second.consecutive_probe_failures++;
                LOG_WARN("Health probe failed for '" + pair.first + "' (" +
                         std::to_string(pair.second.consecutive_probe_failures) + "/" +
                         std::to_string(pair.second.config.health_check.max_retries) + "): " + reason);

                if (pair.second.consecutive_probe_failures >= pair.second.config.health_check.max_retries) {
                    pair.second.stats.state = ServiceState::DEGRADED;
                    pair.second.stats.last_error = reason;
                    LOG_ERROR("Service '" + pair.first + "' health marked DEGRADED: " + reason);
                }
            }
        }
    }
}

void Supervisor::handle_backoff_restarts() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    for (auto& pair : services_) {
        if (pair.second.stats.state == ServiceState::BACKOFF_WAIT) {
            uint32_t remaining_ms = 0;
            if (!self_healer_.is_in_backoff(pair.first, remaining_ms)) {
                LOG_INFO("Self-healing backoff period completed for '" + pair.first + "'. Respawning service...");
                start_service_recursive(pair.first);
            }
        }
    }
}

void Supervisor::sample_system_metrics() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto now = std::chrono::system_clock::now();

    for (auto& pair : services_) {
        if (pair.second.stats.pid > 0 && pair.second.stats.state == ServiceState::HEALTHY) {
            process_mgr_.sample_resource_usage(pair.second.stats.pid,
                                               pair.second.stats.memory_usage_bytes,
                                               pair.second.stats.cpu_usage_pct);

            pair.second.stats.uptime_seconds =
                std::chrono::duration_cast<std::chrono::seconds>(now - pair.second.stats.start_time).count();
        }
    }
}

std::map<std::string, ManagedService> Supervisor::get_all_services() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return services_;
}

std::string Supervisor::get_status_formatted() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    std::ostringstream ss;

    ss << "\n================================ ProcessPilot Pro Service Status ================================\n";
    ss << std::left
       << std::setw(18) << "SERVICE"
       << std::setw(15) << "STATE"
       << std::setw(8)  << "PID"
       << std::setw(10) << "UPTIME"
       << std::setw(10) << "RESTARTS"
       << std::setw(12) << "MEMORY"
       << std::setw(10) << "CPU %"
       << "DETAILS\n";
    ss << "-------------------------------------------------------------------------------------------------\n";

    for (const auto& pair : services_) {
        const auto& s = pair.second;
        std::string uptime_str = (s.stats.pid > 0) ? (std::to_string(s.stats.uptime_seconds) + "s") : "-";
        std::string pid_str = (s.stats.pid > 0) ? std::to_string(s.stats.pid) : "-";
        std::string mem_str = (s.stats.memory_usage_bytes > 0) ? (std::to_string(s.stats.memory_usage_bytes / 1024) + " KB") : "-";

        std::ostringstream cpu_ss;
        if (s.stats.pid > 0) cpu_ss << std::fixed << std::setprecision(1) << s.stats.cpu_usage_pct << "%";
        else cpu_ss << "-";

        ss << std::left
           << std::setw(18) << s.config.name
           << std::setw(15) << service_state_to_string(s.stats.state)
           << std::setw(8)  << pid_str
           << std::setw(10) << uptime_str
           << std::setw(10) << s.stats.restart_count
           << std::setw(12) << mem_str
           << std::setw(10) << cpu_ss.str()
           << s.stats.last_error << "\n";
    }

    ss << "=================================================================================================\n";
    return ss.str();
}

std::string Supervisor::get_tree_formatted() {
    return dag_resolver_.generate_ascii_tree();
}

IpcResponse Supervisor::handle_ipc_request(const IpcRequest& req) {
    IpcResponse resp;
    resp.success = true;

    switch (req.cmd) {
    case IpcCommandType::CMD_STATUS:
        resp.payload = get_status_formatted();
        resp.message = "Status retrieved successfully";
        break;

    case IpcCommandType::CMD_TREE:
        resp.payload = get_tree_formatted();
        resp.message = "DAG tree retrieved successfully";
        break;

    case IpcCommandType::CMD_START: {
        PilotStatus st = start_service(req.target_service);
        resp.success = (st == PilotStatus::SUCCESS);
        resp.message = resp.success ? ("Started service " + req.target_service)
                                    : ("Failed to start service " + req.target_service + ": " + status_to_string(st));
        break;
    }

    case IpcCommandType::CMD_STOP: {
        PilotStatus st = stop_service(req.target_service);
        resp.success = (st == PilotStatus::SUCCESS);
        resp.message = resp.success ? ("Stopped service " + req.target_service)
                                    : ("Failed to stop service " + req.target_service + ": " + status_to_string(st));
        break;
    }

    case IpcCommandType::CMD_RESTART: {
        PilotStatus st = restart_service(req.target_service);
        resp.success = (st == PilotStatus::SUCCESS);
        resp.message = resp.success ? ("Restarted service " + req.target_service)
                                    : ("Failed to restart service " + req.target_service + ": " + status_to_string(st));
        break;
    }

    case IpcCommandType::CMD_RELOAD: {
        PilotStatus st = reload_configs();
        resp.success = (st == PilotStatus::SUCCESS);
        resp.message = resp.success ? "Reloaded all configurations"
                                    : ("Failed to reload configurations: " + status_to_string(st));
        break;
    }

    case IpcCommandType::CMD_RESET_CIRCUIT: {
        self_healer_.reset_circuit_breaker(req.target_service);
        resp.message = "Circuit breaker reset for " + req.target_service;
        break;
    }

    case IpcCommandType::CMD_DRIVER_STATS: {
        struct pilot_driver_stats stats;
        PilotStatus st = health_checker_.get_driver_stats(stats);
        if (st == PilotStatus::SUCCESS) {
            std::ostringstream ss;
            ss << "\n--- Linux Kernel Driver Telemetry (/dev/process_pilot) ---\n"
               << "Driver Version:          v" << stats.driver_version_major << "." << stats.driver_version_minor << "\n"
               << "Active Watchdogs:        " << stats.active_watchdogs_count << "\n"
               << "Total Heartbeats:        " << stats.total_heartbeats_received << "\n"
               << "Registered Watchdogs:    " << stats.total_watchdogs_registered << "\n"
               << "Expired Watchdogs:       " << stats.total_watchdogs_expired << "\n"
               << "Missed Deadlines:        " << stats.total_missed_deadlines << "\n"
               << "Queued Events:           " << stats.total_events_queued << "\n"
               << "Dropped Events:          " << stats.total_events_dropped << "\n"
               << "---------------------------------------------------------\n";
            resp.payload = ss.str();
            resp.message = "Driver stats retrieved";
        } else {
            resp.success = false;
            resp.message = "Failed to query Linux Kernel Device Driver";
        }
        break;
    }

    case IpcCommandType::CMD_INJECT_FAULT: {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto it = services_.find(req.target_service);
        if (it != services_.end() && it->second.stats.pid > 0) {
            health_checker_.inject_driver_fault(it->second.stats.pid, 1);
            resp.message = "Fault injected into " + req.target_service + " (PID " + std::to_string(it->second.stats.pid) + ")";
        } else {
            resp.success = false;
            resp.message = "Service not running or not found: " + req.target_service;
        }
        break;
    }

    case IpcCommandType::CMD_GET_LOGS: {
        std::string log_file = process_mgr_.get_service_log_path(req.target_service);
        std::ifstream lf(log_file);
        if (lf.is_open()) {
            std::string content((std::istreambuf_iterator<char>(lf)), std::istreambuf_iterator<char>());
            resp.payload = content;
            resp.message = "Logs retrieved for " + req.target_service;
        } else {
            resp.success = false;
            resp.message = "Log file not found: " + log_file;
        }
        break;
    }

    default:
        resp.success = false;
        resp.message = "Unknown IPC command";
        break;
    }

    return resp;
}

} // namespace process_pilot
