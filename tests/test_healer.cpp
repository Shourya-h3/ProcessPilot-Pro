/*
 * ProcessPilot Pro - Self-Healer Unit Tests
 * File: tests/test_healer.cpp
 */

#include "engine/self_healer.hpp"
#include <iostream>
#include <cassert>

using namespace process_pilot;

void test_backoff_calculation() {
    std::cout << "[RUN] Testing Exponential Backoff Calculation..." << std::endl;
    SelfHealer healer;
    SelfHealingConfig cfg;
    cfg.initial_backoff_ms = 500;
    cfg.backoff_multiplier = 2.0;
    cfg.max_backoff_ms = 4000;

    // Restart 0 -> 0 ms
    assert(healer.calculate_backoff_ms(cfg, 0) == 0);

    // Restart 1 -> ~500 ms (+/- 10%)
    uint32_t b1 = healer.calculate_backoff_ms(cfg, 1);
    assert(b1 >= 450 && b1 <= 550);

    // Restart 2 -> ~1000 ms (+/- 10%)
    uint32_t b2 = healer.calculate_backoff_ms(cfg, 2);
    assert(b2 >= 900 && b2 <= 1100);

    // Restart 5 -> capped at ~4000 ms (+/- 10%)
    uint32_t b5 = healer.calculate_backoff_ms(cfg, 5);
    assert(b5 >= 3600 && b5 <= 4400);

    std::cout << "[PASS] Exponential Backoff Calculation" << std::endl;
}

void test_circuit_breaker() {
    std::cout << "[RUN] Testing Circuit Breaker Tripping..." << std::endl;
    SelfHealer healer;
    SelfHealingConfig cfg;
    cfg.initial_backoff_ms = 100;
    cfg.max_crash_count = 3;
    cfg.crash_window_sec = 60;

    assert(!healer.is_circuit_broken("svc_test"));

    healer.record_crash("svc_test", cfg);
    assert(!healer.is_circuit_broken("svc_test"));

    healer.record_crash("svc_test", cfg);
    assert(!healer.is_circuit_broken("svc_test"));

    healer.record_crash("svc_test", cfg); // 3rd crash
    assert(healer.is_circuit_broken("svc_test"));

    // Reset circuit breaker
    healer.reset_circuit_breaker("svc_test");
    assert(!healer.is_circuit_broken("svc_test"));

    std::cout << "[PASS] Circuit Breaker Tripping and Reset" << std::endl;
}

int main() {
    std::cout << "================ Running Self-Healer Unit Tests ================" << std::endl;
    test_backoff_calculation();
    test_circuit_breaker();
    std::cout << "All Self-Healer unit tests passed successfully!\n" << std::endl;
    return 0;
}
