/*
 * ProcessPilot Pro - Core Types and Models
 * File: include/common/pilot_types.hpp
 */

#ifndef PROCESS_PILOT_TYPES_HPP
#define PROCESS_PILOT_TYPES_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>
#include <sys/types.h>

namespace process_pilot {

/* Service Lifecycle State */
enum class ServiceState {
    INACTIVE,
    STARTING,
    HEALTHY,
    DEGRADED,
    FAILED,
    STOPPING,
    STOPPED,
    CIRCUIT_BROKEN,
    BACKOFF_WAIT
};

inline std::string service_state_to_string(ServiceState state) {
    switch (state) {
        case ServiceState::INACTIVE:       return "INACTIVE";
        case ServiceState::STARTING:       return "STARTING";
        case ServiceState::HEALTHY:        return "HEALTHY";
        case ServiceState::DEGRADED:       return "DEGRADED";
        case ServiceState::FAILED:         return "FAILED";
        case ServiceState::STOPPING:       return "STOPPING";
        case ServiceState::STOPPED:        return "STOPPED";
        case ServiceState::CIRCUIT_BROKEN: return "CIRCUIT_BROKEN";
        case ServiceState::BACKOFF_WAIT:   return "BACKOFF_WAIT";
        default:                           return "UNKNOWN";
    }
}

/* Service Restart Policies */
enum class RestartPolicy {
    ALWAYS,
    ON_FAILURE,
    NEVER,
    EXPONENTIAL_BACKOFF
};

inline RestartPolicy string_to_restart_policy(const std::string& str) {
    if (str == "always") return RestartPolicy::ALWAYS;
    if (str == "on-failure") return RestartPolicy::ON_FAILURE;
    if (str == "never") return RestartPolicy::NEVER;
    return RestartPolicy::EXPONENTIAL_BACKOFF;
}

inline std::string restart_policy_to_string(RestartPolicy policy) {
    switch (policy) {
        case RestartPolicy::ALWAYS: return "always";
        case RestartPolicy::ON_FAILURE: return "on-failure";
        case RestartPolicy::NEVER: return "never";
        case RestartPolicy::EXPONENTIAL_BACKOFF: return "exponential-backoff";
        default: return "exponential-backoff";
    }
}

/* Multi-Modal Health Check Type */
enum class HealthCheckType {
    NONE,
    KERNEL_WATCHDOG,
    TCP_PORT,
    HTTP_ENDPOINT,
    EXEC_COMMAND
};

/* Health Check Configuration */
struct HealthCheckConfig {
    HealthCheckType type = HealthCheckType::NONE;
    std::string endpoint = "";            /* E.g. "127.0.0.1:8080" or "/health" or command */
    uint32_t interval_ms = 2000;          /* Check interval in ms */
    uint32_t timeout_ms = 1000;           /* Probe timeout in ms */
    uint32_t max_retries = 3;             /* Consecutive failures before marking degraded/failed */
    uint32_t kernel_watchdog_timeout_ms = 1000; /* Watchdog timeout if type is KERNEL_WATCHDOG */
};

/* cgroup Resource Limits Configuration */
struct ResourceLimits {
    uint64_t memory_max_bytes = 0;        /* 0 = unlimited */
    uint32_t cpu_quota_pct = 0;           /* 0 = unlimited, e.g. 50 = 50% CPU */
    uint32_t max_pids = 0;                /* 0 = unlimited */
};

/* Exponential Backoff & Circuit Breaker Config */
struct SelfHealingConfig {
    uint32_t initial_backoff_ms = 500;
    uint32_t max_backoff_ms = 30000;
    double backoff_multiplier = 2.0;
    uint32_t max_crash_count = 5;         /* Trip circuit breaker if crashed > 5 times */
    uint32_t crash_window_sec = 60;       /* within 60 seconds */
};

/* Service Unit Configuration */
struct ServiceConfig {
    std::string name;
    std::string description;
    std::string exec_start;
    std::string exec_stop;
    std::string working_dir = "/";
    std::vector<std::string> dependencies; /* Services that must be healthy BEFORE this starts */
    std::vector<std::string> wants;        /* Soft dependencies */
    std::map<std::string, std::string> environment;
    RestartPolicy restart_policy = RestartPolicy::EXPONENTIAL_BACKOFF;
    HealthCheckConfig health_check;
    ResourceLimits resources;
    SelfHealingConfig healing;
    bool auto_start = true;
};

/* Realtime Service Telemetry & Stats */
struct ServiceRuntimeStats {
    pid_t pid = 0;
    ServiceState state = ServiceState::INACTIVE;
    uint32_t restart_count = 0;
    uint32_t crash_count_in_window = 0;
    uint64_t uptime_seconds = 0;
    uint64_t memory_usage_bytes = 0;
    double cpu_usage_pct = 0.0;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_crash_time;
    std::string last_error = "";
    uint32_t current_backoff_ms = 0;
    uint64_t total_heartbeats_sent = 0;
};

} // namespace process_pilot

#endif // PROCESS_PILOT_TYPES_HPP
