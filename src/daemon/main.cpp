/*
 * ProcessPilot Pro - Supervisor Daemon Main Entry Point
 * File: src/daemon/main.cpp
 */

#include "daemon/supervisor.hpp"
#include "common/logger.hpp"

#include <iostream>
#include <csignal>
#include <condition_variable>
#include <mutex>
#include <unistd.h>

static std::condition_variable g_shutdown_cv;
static std::mutex g_shutdown_mutex;
static bool g_shutdown_requested = false;

static void signal_handler(int sig) {
    LOG_INFO("Received signal " + std::to_string(sig) + ", initiating shutdown...");
    {
        std::lock_guard<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_requested = true;
    }
    g_shutdown_cv.notify_all();
}

void print_banner() {
    std::cout << R"(
  ____                               ____  _ _       _     ____            
 |  _ \ _ __ ___   ___ ___  ___ ___ |  _ \(_) | ___ | |_  |  _ \ _ __ ___  
 | |_) | '__/ _ \ / __/ _ \/ __/ __|| |_) | | |/ _ \| __| | |_) | '__/ _ \ 
 |  __/| | | (_) | (_|  __/\__ \__ \|  __/| | | (_) | |_  |  __/| | | (_) |
 |_|   |_|  \___/ \___\___||___/___/|_|   |_|_|\___/ \__| |_|   |_|  \___/ 
      Dependency-Aware Linux Service Supervisor with Self-Healing Runtime
    )" << std::endl;
}

int main(int argc, char* argv[]) {
    print_banner();

    std::string config_dir = "./services.d";
    std::string socket_path = PILOT_DEFAULT_SOCKET_PATH;
    process_pilot::LogLevel log_level = process_pilot::LogLevel::LOG_INFO;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            config_dir = argv[++i];
        } else if (arg == "-s" && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (arg == "-v" || arg == "--verbose") {
            log_level = process_pilot::LogLevel::LOG_DEBUG;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: processpilotd [options]\n"
                      << "Options:\n"
                      << "  -c <dir>     Directory containing .pilot service configs (default: ./services.d)\n"
                      << "  -s <path>    Unix domain socket path (default: " << PILOT_DEFAULT_SOCKET_PATH << ")\n"
                      << "  -v, --verbose Enable debug logging\n"
                      << "  -h, --help    Show this help message\n";
            return 0;
        }
    }

    process_pilot::Logger::instance().init(log_level, "./logs/processpilotd.log");
    LOG_INFO("Starting processpilotd (ProcessPilot Pro Supervisor Daemon)...");

    // Register POSIX signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif

    process_pilot::Supervisor supervisor(config_dir, socket_path);
    process_pilot::PilotStatus status = supervisor.init();
    if (status != process_pilot::PilotStatus::SUCCESS) {
        LOG_FATAL("Failed to initialize supervisor: " + process_pilot::status_to_string(status));
        return 1;
    }

    status = supervisor.start();
    if (status != process_pilot::PilotStatus::SUCCESS) {
        LOG_FATAL("Failed to start supervisor: " + process_pilot::status_to_string(status));
        return 1;
    }

    LOG_INFO("ProcessPilot Pro Daemon initialized. Press Ctrl+C to terminate.");

    // Wait for shutdown signal
    {
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait(lock, [] { return g_shutdown_requested; });
    }

    LOG_INFO("Shutting down supervisor daemon...");
    supervisor.stop();
    LOG_INFO("ProcessPilot Pro Daemon exited successfully.");
    return 0;
}
