/*
 * ProcessPilot Pro - Central Supervisor Daemon Engine
 * File: src/daemon/supervisor.hpp
 */

#ifndef PROCESS_PILOT_SUPERVISOR_HPP
#define PROCESS_PILOT_SUPERVISOR_HPP

#include "common/pilot_types.hpp"
#include "common/error_codes.hpp"
#include "parser/config_parser.hpp"
#include "engine/dag_resolver.hpp"
#include "engine/process_manager.hpp"
#include "engine/health_checker.hpp"
#include "engine/self_healer.hpp"
#include "ipc/ipc_server.hpp"

#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>

namespace process_pilot {

struct ManagedService {
    ServiceConfig config;
    ServiceRuntimeStats stats;
    std::chrono::steady_clock::time_point last_health_check;
    uint32_t consecutive_probe_failures = 0;
};

class Supervisor {
public:
    Supervisor(const std::string& config_dir = "./services.d",
               const std::string& socket_path = PILOT_DEFAULT_SOCKET_PATH);
    ~Supervisor();

    /* Initialize and load configs */
    PilotStatus init();

    /* Start supervisor daemon and launch auto-start services */
    PilotStatus start();

    /* Stop supervisor daemon and terminate all child processes */
    void stop();

    /* Check if supervisor is currently running */
    bool is_running() const { return running_; }

    /* Service Operations */
    PilotStatus start_service(const std::string& name);
    PilotStatus stop_service(const std::string& name);
    PilotStatus restart_service(const std::string& name);
    PilotStatus reload_configs();

    /* Getters for telemetry */
    std::map<std::string, ManagedService> get_all_services();
    std::string get_status_formatted();
    std::string get_tree_formatted();

private:
    std::string config_dir_;
    std::string socket_path_;
    std::atomic<bool> running_{false};

    std::map<std::string, ManagedService> services_;
    std::mutex state_mutex_;

    DAGResolver dag_resolver_;
    ProcessManager process_mgr_;
    HealthChecker health_checker_;
    SelfHealer self_healer_;
    IpcServer ipc_server_;

    std::thread supervisor_thread_;

    /* Background supervision loop */
    void supervision_loop();

    /* Core monitoring handlers */
    void handle_reaped_processes();
    void handle_health_probes();
    void handle_backoff_restarts();
    void sample_system_metrics();

    /* IPC Command Dispatcher */
    IpcResponse handle_ipc_request(const IpcRequest& req);

    /* Helper: Start service dependencies first */
    PilotStatus start_service_recursive(const std::string& name);
};

} // namespace process_pilot

#endif // PROCESS_PILOT_SUPERVISOR_HPP
