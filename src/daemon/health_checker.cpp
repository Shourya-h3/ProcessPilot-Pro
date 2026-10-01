/*
 * ProcessPilot Pro - Multi-Modal Health Checker & Driver Watchdog Implementation
 * File: src/daemon/health_checker.cpp
 */

#include "engine/health_checker.hpp"
#include "common/logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <cstring>
#include <chrono>
#include <cstdlib>

namespace process_pilot {

HealthChecker::HealthChecker() {
    init_driver();
}

HealthChecker::~HealthChecker() {
    close_driver();
}

PilotStatus HealthChecker::init_driver() {
    std::lock_guard<std::mutex> lock(driver_mutex_);
#ifndef _WIN32
    if (driver_fd_ >= 0) {
        return PilotStatus::SUCCESS;
    }

    driver_fd_ = open(PILOT_DEVICE_PATH, O_RDWR);
    if (driver_fd_ < 0) {
        LOG_WARN("Could not open Linux Device Driver " PILOT_DEVICE_PATH " (" + std::string(strerror(errno)) + "). Running with userspace software fallback.");
        return PilotStatus::ERR_DRIVER_NOT_FOUND;
    }

    LOG_INFO("Successfully initialized ProcessPilot Kernel Driver interface (/dev/process_pilot)");
    return PilotStatus::SUCCESS;
#else
    LOG_DEBUG("[SIM] Driver interface initialized in mock mode");
    return PilotStatus::SUCCESS;
#endif
}

void HealthChecker::close_driver() {
    std::lock_guard<std::mutex> lock(driver_mutex_);
#ifndef _WIN32
    if (driver_fd_ >= 0) {
        close(driver_fd_);
        driver_fd_ = -1;
    }
#endif
}

PilotStatus HealthChecker::register_kernel_watchdog(pid_t pid, const std::string& service_name, uint32_t timeout_ms, uint32_t max_misses) {
    std::lock_guard<std::mutex> lock(driver_mutex_);
#ifndef _WIN32
    if (driver_fd_ < 0) {
        return PilotStatus::ERR_DRIVER_NOT_FOUND;
    }

    struct pilot_watchdog_reg reg;
    memset(&reg, 0, sizeof(reg));
    reg.pid = pid;
    reg.timeout_ms = timeout_ms;
    reg.max_misses = max_misses;
    strncpy(reg.service_name, service_name.c_str(), sizeof(reg.service_name) - 1);

    if (ioctl(driver_fd_, PILOT_IOCTL_REGISTER_WATCHDOG, &reg) < 0) {
        LOG_ERROR("ioctl PILOT_IOCTL_REGISTER_WATCHDOG failed for PID " + std::to_string(pid) + ": " + std::string(strerror(errno)));
        return PilotStatus::ERR_DRIVER_IOCTL_FAILED;
    }

    LOG_INFO("Registered Kernel Watchdog for '" + service_name + "' (PID " + std::to_string(pid) + "), timeout: " + std::to_string(timeout_ms) + " ms");
#endif
    return PilotStatus::SUCCESS;
}

PilotStatus HealthChecker::ping_kernel_watchdog(pid_t pid) {
    std::lock_guard<std::mutex> lock(driver_mutex_);
#ifndef _WIN32
    if (driver_fd_ < 0) {
        return PilotStatus::ERR_DRIVER_NOT_FOUND;
    }

    struct pilot_heartbeat_ping ping;
    memset(&ping, 0, sizeof(ping));
    ping.pid = pid;
    ping.sequence_num = ++heartbeat_seq_;

    auto now = std::chrono::steady_clock::now();
    ping.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    if (ioctl(driver_fd_, PILOT_IOCTL_PING_HEARTBEAT, &ping) < 0) {
        return PilotStatus::ERR_DRIVER_IOCTL_FAILED;
    }
#endif
    return PilotStatus::SUCCESS;
}

PilotStatus HealthChecker::unregister_kernel_watchdog(pid_t pid) {
    std::lock_guard<std::mutex> lock(driver_mutex_);
#ifndef _WIN32
    if (driver_fd_ < 0) {
        return PilotStatus::ERR_DRIVER_NOT_FOUND;
    }

    if (ioctl(driver_fd_, PILOT_IOCTL_UNREGISTER_WATCHDOG, &pid) < 0) {
        return PilotStatus::ERR_DRIVER_IOCTL_FAILED;
    }

    LOG_INFO("Unregistered Kernel Watchdog for PID " + std::to_string(pid));
#endif
    return PilotStatus::SUCCESS;
}

PilotStatus HealthChecker::get_driver_stats(struct pilot_driver_stats& out_stats) {
    std::lock_guard<std::mutex> lock(driver_mutex_);
    memset(&out_stats, 0, sizeof(out_stats));
#ifndef _WIN32
    if (driver_fd_ < 0) {
        return PilotStatus::ERR_DRIVER_NOT_FOUND;
    }

    if (ioctl(driver_fd_, PILOT_IOCTL_GET_STATS, &out_stats) < 0) {
        return PilotStatus::ERR_DRIVER_IOCTL_FAILED;
    }
#else
    out_stats.driver_version_major = 1;
    out_stats.driver_version_minor = 0;
#endif
    return PilotStatus::SUCCESS;
}

PilotStatus HealthChecker::inject_driver_fault(pid_t pid, uint32_t fault_type, uint32_t delay_ms) {
    std::lock_guard<std::mutex> lock(driver_mutex_);
#ifndef _WIN32
    if (driver_fd_ < 0) {
        return PilotStatus::ERR_DRIVER_NOT_FOUND;
    }

    struct pilot_fault_inject fault;
    fault.target_pid = pid;
    fault.fault_type = fault_type;
    fault.delay_ms = delay_ms;

    if (ioctl(driver_fd_, PILOT_IOCTL_INJECT_FAULT, &fault) < 0) {
        return PilotStatus::ERR_DRIVER_IOCTL_FAILED;
    }
#endif
    return PilotStatus::SUCCESS;
}

bool HealthChecker::check_tcp_port(const std::string& endpoint, uint32_t timeout_ms) {
#ifndef _WIN32
    size_t colon_pos = endpoint.find(':');
    if (colon_pos == std::string::npos) return false;

    std::string host = endpoint.substr(0, colon_pos);
    int port = std::stoi(endpoint.substr(colon_pos + 1));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (res < 0 && errno != EINPROGRESS) {
        close(sock);
        return false;
    }

    if (res == 0) {
        close(sock);
        return true;
    }

    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLOUT;

    int poll_res = poll(&pfd, 1, timeout_ms);
    if (poll_res > 0 && (pfd.revents & POLLOUT)) {
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
        close(sock);
        return (err == 0);
    }

    close(sock);
    return false;
#else
    return true;
#endif
}

bool HealthChecker::check_http_endpoint(const std::string& endpoint, uint32_t timeout_ms) {
    // Basic HTTP check over TCP socket
    return check_tcp_port(endpoint, timeout_ms);
}

bool HealthChecker::check_exec_command(const std::string& command, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (command.empty()) return true;
#ifndef _WIN32
    int ret = std::system(command.c_str());
    return (ret == 0);
#else
    return true;
#endif
}

bool HealthChecker::execute_probe(const HealthCheckConfig& config, pid_t pid, std::string& out_reason) {
    if (config.type == HealthCheckType::NONE) {
        return true;
    }

    if (config.type == HealthCheckType::KERNEL_WATCHDOG) {
        PilotStatus status = ping_kernel_watchdog(pid);
        if (status == PilotStatus::SUCCESS) {
            return true;
        } else {
            out_reason = "Kernel Watchdog heartbeat ping rejected or driver absent";
            return false;
        }
    }

    if (config.type == HealthCheckType::TCP_PORT) {
        bool ok = check_tcp_port(config.endpoint, config.timeout_ms);
        if (!ok) out_reason = "TCP connection failed to " + config.endpoint;
        return ok;
    }

    if (config.type == HealthCheckType::HTTP_ENDPOINT) {
        bool ok = check_http_endpoint(config.endpoint, config.timeout_ms);
        if (!ok) out_reason = "HTTP probe failed on " + config.endpoint;
        return ok;
    }

    if (config.type == HealthCheckType::EXEC_COMMAND) {
        bool ok = check_exec_command(config.endpoint, config.timeout_ms);
        if (!ok) out_reason = "Health command probe exited with non-zero status";
        return ok;
    }

    return true;
}

} // namespace process_pilot
