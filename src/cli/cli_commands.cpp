/*
 * ProcessPilot Pro - CLI Commands Handler Implementation
 * File: src/cli/cli_commands.cpp
 */

#include "cli/cli_commands.hpp"
#include <iostream>

namespace process_pilot {

static int execute_and_print(const IpcRequest& req, const std::string& sock_path) {
    IpcResponse resp;
    PilotStatus status = IpcClient::send_command(req, resp, sock_path);
    if (status != PilotStatus::SUCCESS) {
        std::cerr << "\033[31m[ERROR] Failed to communicate with processpilotd daemon at "
                  << sock_path << " (" << status_to_string(status) << "). Is the daemon running?\033[0m\n";
        return 1;
    }

    if (!resp.payload.empty()) {
        std::cout << resp.payload << std::endl;
    }

    if (!resp.success) {
        std::cerr << "\033[31m[-] " << resp.message << "\033[0m\n";
        return 1;
    } else if (!resp.message.empty() && resp.payload.empty()) {
        std::cout << "\033[32m[+] " << resp.message << "\033[0m\n";
    }

    return 0;
}

int CliCommands::handle_status(const std::string& service, const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_STATUS;
    req.target_service = service;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_tree(const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_TREE;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_start(const std::string& service, const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_START;
    req.target_service = service;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_stop(const std::string& service, const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_STOP;
    req.target_service = service;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_restart(const std::string& service, const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_RESTART;
    req.target_service = service;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_reload(const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_RELOAD;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_logs(const std::string& service, const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_GET_LOGS;
    req.target_service = service;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_driver_stats(const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_DRIVER_STATS;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_inject_fault(const std::string& service, const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_INJECT_FAULT;
    req.target_service = service;
    return execute_and_print(req, sock_path);
}

int CliCommands::handle_reset_circuit(const std::string& service, const std::string& sock_path) {
    IpcRequest req;
    req.cmd = IpcCommandType::CMD_RESET_CIRCUIT;
    req.target_service = service;
    return execute_and_print(req, sock_path);
}

void CliCommands::print_help() {
    std::cout << R"(
ProcessPilot Pro CLI Control Tool (pilotctl)

Usage:
  pilotctl [options] <command> [arguments]

Commands:
  status [service]        Display live status table or specific service info
  tree                    Print DAG dependency execution graph
  start <service>         Start a service and its prerequisite dependencies
  stop <service>          Gracefully stop a running service
  restart <service>       Restart a service and clear backoffs
  reload                  Hot-reload .pilot unit configurations from disk
  logs <service>          View captured stdout/stderr log output
  driver-stats            Inspect Linux Kernel Device Driver (/dev/process_pilot) metrics
  inject-fault <service>  Simulate heartbeat loss / failure on target service
  reset-circuit <service> Clear tripped circuit breaker for a service
  help                    Show this help manual

Options:
  -s, --socket <path>     Path to daemon Unix Domain Socket (default: /tmp/process_pilot.sock)
)" << std::endl;
}

} // namespace process_pilot
