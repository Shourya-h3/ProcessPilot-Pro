/*
 * ProcessPilot Pro - Self-Healing Engine (Exponential Backoff & Circuit Breaker)
 * File: include/engine/self_healer.hpp
 */

#ifndef PROCESS_PILOT_SELF_HEALER_HPP
#define PROCESS_PILOT_SELF_HEALER_HPP

#include "common/pilot_types.hpp"
#include "common/logger.hpp"
#include <string>
#include <map>
#include <deque>
#include <chrono>

namespace process_pilot {

struct ServiceHealingState {
    std::deque<std::chrono::steady_clock::time_point> crash_timestamps;
    uint32_t current_consecutive_restarts = 0;
    bool circuit_broken = false;
    std::chrono::steady_clock::time_point backoff_until;
};

class SelfHealer {
public:
    SelfHealer() = default;

    /* Evaluate if a service is allowed to restart after an unexpected termination */
    bool should_restart(const ServiceConfig& config,
                        int exit_code,
                        bool normal_exit,
                        uint32_t& out_delay_ms,
                        std::string& out_reason);

    /* Record a crash event */
    void record_crash(const std::string& service_name, const SelfHealingConfig& healing_cfg);

    /* Record a successful healthy state (resets consecutive backoff) */
    void record_healthy(const std::string& service_name);

    /* Check if service is currently under active backoff timer */
    bool is_in_backoff(const std::string& service_name, uint32_t& out_remaining_ms);

    /* Check if circuit breaker is tripped */
    bool is_circuit_broken(const std::string& service_name);

    /* Manually reset circuit breaker for a service */
    void reset_circuit_breaker(const std::string& service_name);

    /* Calculate next backoff delay in ms with jitter */
    uint32_t calculate_backoff_ms(const SelfHealingConfig& cfg, uint32_t consecutive_restarts);

    /* Clean old crash timestamps */
    void prune_crash_window(const std::string& service_name, uint32_t window_sec);

    /* Get current crash count in window */
    uint32_t get_crash_count_in_window(const std::string& service_name, uint32_t window_sec);

private:
    std::map<std::string, ServiceHealingState> healing_states_;
    PlatformMutex healer_mutex_;
};

} // namespace process_pilot

#endif // PROCESS_PILOT_SELF_HEALER_HPP
