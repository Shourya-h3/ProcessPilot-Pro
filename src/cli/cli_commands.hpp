/*
 * ProcessPilot Pro - CLI Commands Handler
 * File: src/cli/cli_commands.hpp
 */

#ifndef PROCESS_PILOT_CLI_COMMANDS_HPP
#define PROCESS_PILOT_CLI_COMMANDS_HPP

#include "ipc/ipc_client.hpp"
#include <string>
#include <vector>

namespace process_pilot {

class CliCommands {
public:
    static int handle_status(const std::string& service, const std::string& sock_path);
    static int handle_tree(const std::string& sock_path);
    static int handle_start(const std::string& service, const std::string& sock_path);
    static int handle_stop(const std::string& service, const std::string& sock_path);
    static int handle_restart(const std::string& service, const std::string& sock_path);
    static int handle_reload(const std::string& sock_path);
    static int handle_logs(const std::string& service, const std::string& sock_path);
    static int handle_driver_stats(const std::string& sock_path);
    static int handle_inject_fault(const std::string& service, const std::string& sock_path);
    static int handle_reset_circuit(const std::string& service, const std::string& sock_path);
    static void print_help();
};

} // namespace process_pilot

#endif // PROCESS_PILOT_CLI_COMMANDS_HPP
