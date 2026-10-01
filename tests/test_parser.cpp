/*
 * ProcessPilot Pro - Parser Unit Tests
 * File: tests/test_parser.cpp
 */

#include "parser/config_parser.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdio>

using namespace process_pilot;

void test_parse_valid_unit() {
    std::cout << "[RUN] Testing Unit Config Parser..." << std::endl;
    std::string test_file = "./test_temp.pilot";

    std::ofstream out(test_file);
    out << "[Unit]\n"
        << "Description=Test Unit Service\n"
        << "Requires=dep1.pilot, dep2\n"
        << "AutoStart=true\n\n"
        << "[Service]\n"
        << "ExecStart=/usr/bin/test --arg\n"
        << "Restart=always\n"
        << "Environment=FOO=bar;BAZ=qux\n\n"
        << "[HealthCheck]\n"
        << "Type=tcp\n"
        << "Endpoint=127.0.0.1:9000\n"
        << "IntervalMs=1500\n\n"
        << "[SelfHealing]\n"
        << "InitialBackoffMs=250\n"
        << "MaxCrashCount=3\n\n"
        << "[ResourceLimits]\n"
        << "MemoryMaxBytes=104857600\n";
    out.close();

    ServiceConfig cfg;
    PilotStatus st = ConfigParser::parse_service_file(test_file, cfg);
    assert(st == PilotStatus::SUCCESS);
    assert(cfg.name == "test_temp");
    assert(cfg.description == "Test Unit Service");
    assert(cfg.dependencies.size() == 2);
    assert(cfg.dependencies[0] == "dep1.pilot");
    assert(cfg.dependencies[1] == "dep2");
    assert(cfg.auto_start == true);
    assert(cfg.exec_start == "/usr/bin/test --arg");
    assert(cfg.restart_policy == RestartPolicy::ALWAYS);
    assert(cfg.environment["FOO"] == "bar");
    assert(cfg.environment["BAZ"] == "qux");
    assert(cfg.health_check.type == HealthCheckType::TCP_PORT);
    assert(cfg.health_check.endpoint == "127.0.0.1:9000");
    assert(cfg.health_check.interval_ms == 1500);
    assert(cfg.healing.initial_backoff_ms == 250);
    assert(cfg.healing.max_crash_count == 3);
    assert(cfg.resources.memory_max_bytes == 104857600);

    std::remove(test_file.c_str());
    std::cout << "[PASS] Unit Config Parser Validated" << std::endl;
}

int main() {
    std::cout << "=================== Running Parser Unit Tests ===================" << std::endl;
    test_parse_valid_unit();
    std::cout << "All Parser unit tests passed successfully!\n" << std::endl;
    return 0;
}
