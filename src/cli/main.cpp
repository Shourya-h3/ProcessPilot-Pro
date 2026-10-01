/*
 * ProcessPilot Pro - CLI Main Entry Point
 * File: src/cli/main.cpp
 */

#include "cli/cli_commands.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        process_pilot::CliCommands::print_help();
        return 1;
    }

    std::string socket_path = PILOT_DEFAULT_SOCKET_PATH;
    std::string command = "";
    std::string target_arg = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-s" || arg == "--socket") && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (command.empty()) {
            command = arg;
        } else if (target_arg.empty()) {
            target_arg = arg;
        }
    }

    if (command == "status") {
        return process_pilot::CliCommands::handle_status(target_arg, socket_path);
    } else if (command == "tree") {
        return process_pilot::CliCommands::handle_tree(socket_path);
    } else if (command == "start") {
        if (target_arg.empty()) {
            std::cerr << "Error: 'start' requires a target service name.\n";
            return 1;
        }
        return process_pilot::CliCommands::handle_start(target_arg, socket_path);
    } else if (command == "stop") {
        if (target_arg.empty()) {
            std::cerr << "Error: 'stop' requires a target service name.\n";
            return 1;
        }
        return process_pilot::CliCommands::handle_stop(target_arg, socket_path);
    } else if (command == "restart") {
        if (target_arg.empty()) {
            std::cerr << "Error: 'restart' requires a target service name.\n";
            return 1;
        }
        return process_pilot::CliCommands::handle_restart(target_arg, socket_path);
    } else if (command == "reload") {
        return process_pilot::CliCommands::handle_reload(socket_path);
    } else if (command == "logs") {
        if (target_arg.empty()) {
            std::cerr << "Error: 'logs' requires a target service name.\n";
            return 1;
        }
        return process_pilot::CliCommands::handle_logs(target_arg, socket_path);
    } else if (command == "driver-stats") {
        return process_pilot::CliCommands::handle_driver_stats(socket_path);
    } else if (command == "inject-fault") {
        if (target_arg.empty()) {
            std::cerr << "Error: 'inject-fault' requires a target service name.\n";
            return 1;
        }
        return process_pilot::CliCommands::handle_inject_fault(target_arg, socket_path);
    } else if (command == "reset-circuit") {
        if (target_arg.empty()) {
            std::cerr << "Error: 'reset-circuit' requires a target service name.\n";
            return 1;
        }
        return process_pilot::CliCommands::handle_reset_circuit(target_arg, socket_path);
    } else if (command == "help" || command == "--help" || command == "-h") {
        process_pilot::CliCommands::print_help();
        return 0;
    } else {
        std::cerr << "Unknown command: '" << command << "'. Run 'pilotctl help' for usage.\n";
        return 1;
    }
}
