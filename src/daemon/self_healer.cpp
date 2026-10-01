/*
 * ProcessPilot Pro - Self-Healing Engine Implementation
 * File: src/daemon/self_healer.cpp
 */

#include "engine/self_healer.hpp"
#include "common/logger.hpp"
#include <cmath>
#include <random>
#include <algorithm>

namespace process_pilot {

uint32_t SelfHealer::calculate_backoff_ms(const SelfHealingConfig& cfg, uint32_t consecutive_restarts) {
    if (consecutive_restarts == 0) {
        return 0;
    }

    double base = static_cast<double>(cfg.initial_backoff_ms);
    double calculated = base * std::pow(cfg.backoff_multiplier, consecutive_restarts - 1);

    if (calculated > static_cast<double>(cfg.max_backoff_ms)) {
        calculated = static_cast<double>(cfg.max_backoff_ms);
    }

    // Add +/- 10% jitter to prevent thundering herd
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.9, 1.1);
    double jittered = calculated * dist(rng);

    return static_cast<uint32_t>(jittered);
}

void SelfHealer::prune_crash_window(const std::string& service_name, uint32_t window_sec) {
    auto it = healing_states_.find(service_name);
    if (it == healing_states_.end()) return;

    auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(window_sec);
    while (!it->second.crash_timestamps.empty() && it->second.crash_timestamps.front() < cutoff) {
        it->second.crash_timestamps.pop_front();
    }
}

uint32_t SelfHealer::get_crash_count_in_window(const std::string& service_name, uint32_t window_sec) {
    PlatformLockGuard lock(healer_mutex_);
    prune_crash_window(service_name, window_sec);
    auto it = healing_states_.find(service_name);
    if (it == healing_states_.end()) return 0;
    return it->second.crash_timestamps.size();
}

void SelfHealer::record_crash(const std::string& service_name, const SelfHealingConfig& healing_cfg) {
    PlatformLockGuard lock(healer_mutex_);
    auto& state = healing_states_[service_name];
    auto now = std::chrono::steady_clock::now();

    state.crash_timestamps.push_back(now);
    state.current_consecutive_restarts++;

    // Prune older crashes
    auto cutoff = now - std::chrono::seconds(healing_cfg.crash_window_sec);
    while (!state.crash_timestamps.empty() && state.crash_timestamps.front() < cutoff) {
        state.crash_timestamps.pop_front();
    }

    // Check circuit breaker trip condition
    if (state.crash_timestamps.size() >= healing_cfg.max_crash_count) {
        state.circuit_broken = true;
        LOG_FATAL("CIRCUIT BREAKER TRIPPED for service '" + service_name + "'! Crashed " +
                  std::to_string(state.crash_timestamps.size()) + " times in " +
                  std::to_string(healing_cfg.crash_window_sec) + "s window. Halting automatic restarts.");
    } else {
        uint32_t backoff = calculate_backoff_ms(healing_cfg, state.current_consecutive_restarts);
        state.backoff_until = now + std::chrono::milliseconds(backoff);
        LOG_WARN("Self-Healing scheduled for '" + service_name + "': Backoff delay " +
                 std::to_string(backoff) + " ms (Restart attempt #" +
                 std::to_string(state.current_consecutive_restarts) + ")");
    }
}

void SelfHealer::record_healthy(const std::string& service_name) {
    PlatformLockGuard lock(healer_mutex_);
    auto it = healing_states_.find(service_name);
    if (it != healing_states_.end()) {
        it->second.current_consecutive_restarts = 0;
    }
}

bool SelfHealer::is_in_backoff(const std::string& service_name, uint32_t& out_remaining_ms) {
    PlatformLockGuard lock(healer_mutex_);
    auto it = healing_states_.find(service_name);
    if (it == healing_states_.end()) {
        out_remaining_ms = 0;
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    if (now < it->second.backoff_until) {
        out_remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(it->second.backoff_until - now).count();
        return true;
    }

    out_remaining_ms = 0;
    return false;
}

bool SelfHealer::is_circuit_broken(const std::string& service_name) {
    PlatformLockGuard lock(healer_mutex_);
    auto it = healing_states_.find(service_name);
    if (it == healing_states_.end()) return false;
    return it->second.circuit_broken;
}

void SelfHealer::reset_circuit_breaker(const std::string& service_name) {
    PlatformLockGuard lock(healer_mutex_);
    auto it = healing_states_.find(service_name);
    if (it != healing_states_.end()) {
        it->second.circuit_broken = false;
        it->second.crash_timestamps.clear();
        it->second.current_consecutive_restarts = 0;
        it->second.backoff_until = std::chrono::steady_clock::now();
        LOG_INFO("Circuit Breaker reset for service '" + service_name + "'");
    }
}

bool SelfHealer::should_restart(const ServiceConfig& config,
                                int exit_code,
                                bool normal_exit,
                                uint32_t& out_delay_ms,
                                std::string& out_reason) {
    PlatformLockGuard lock(healer_mutex_);
    auto& state = healing_states_[config.name];

    if (state.circuit_broken) {
        out_reason = "Circuit breaker is active (service exceeded max crash rate)";
        out_delay_ms = 0;
        return false;
    }

    if (config.restart_policy == RestartPolicy::NEVER) {
        out_reason = "Restart policy is 'never'";
        out_delay_ms = 0;
        return false;
    }

    if (config.restart_policy == RestartPolicy::ON_FAILURE && normal_exit && exit_code == 0) {
        out_reason = "Service exited cleanly (code 0) and policy is 'on-failure'";
        out_delay_ms = 0;
        return false;
    }

    out_delay_ms = calculate_backoff_ms(config.healing, state.current_consecutive_restarts + 1);
    out_reason = "Restarting according to policy '" + restart_policy_to_string(config.restart_policy) + "'";
    return true;
}

} // namespace process_pilot
