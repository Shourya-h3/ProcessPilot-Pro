/*
 * ProcessPilot Pro - DAG Engine Unit Tests
 * File: tests/test_dag.cpp
 */

#include "engine/dag_resolver.hpp"
#include <iostream>
#include <cassert>

using namespace process_pilot;

void test_linear_dag() {
    std::cout << "[RUN] Testing Linear DAG Resolution..." << std::endl;
    std::vector<ServiceConfig> configs;

    ServiceConfig db;
    db.name = "database";
    configs.push_back(db);

    ServiceConfig backend;
    backend.name = "backend";
    backend.dependencies = {"database"};
    configs.push_back(backend);

    ServiceConfig frontend;
    frontend.name = "frontend";
    frontend.dependencies = {"backend"};
    configs.push_back(frontend);

    DAGResolver dag;
    assert(dag.build_graph(configs) == PilotStatus::SUCCESS);

    std::vector<std::string> order;
    assert(dag.get_topological_order(order) == PilotStatus::SUCCESS);
    assert(order.size() == 3);
    assert(order[0] == "database");
    assert(order[1] == "backend");
    assert(order[2] == "frontend");

    std::vector<std::vector<std::string>> tiers;
    assert(dag.get_execution_tiers(tiers) == PilotStatus::SUCCESS);
    assert(tiers.size() == 3);
    assert(tiers[0][0] == "database");
    assert(tiers[1][0] == "backend");
    assert(tiers[2][0] == "frontend");

    std::cout << "[PASS] Linear DAG Resolution" << std::endl;
}

void test_diamond_dag() {
    std::cout << "[RUN] Testing Diamond DAG (Parallel Tiers)..." << std::endl;
    std::vector<ServiceConfig> configs;

    ServiceConfig root;
    root.name = "root";
    configs.push_back(root);

    ServiceConfig left;
    left.name = "left";
    left.dependencies = {"root"};
    configs.push_back(left);

    ServiceConfig right;
    right.name = "right";
    right.dependencies = {"root"};
    configs.push_back(right);

    ServiceConfig join;
    join.name = "join";
    join.dependencies = {"left", "right"};
    configs.push_back(join);

    DAGResolver dag;
    assert(dag.build_graph(configs) == PilotStatus::SUCCESS);

    std::vector<std::vector<std::string>> tiers;
    assert(dag.get_execution_tiers(tiers) == PilotStatus::SUCCESS);
    assert(tiers.size() == 3);
    assert(tiers[0].size() == 1 && tiers[0][0] == "root");
    assert(tiers[1].size() == 2 && tiers[1][0] == "left" && tiers[1][1] == "right");
    assert(tiers[2].size() == 1 && tiers[2][0] == "join");

    std::cout << "[PASS] Diamond DAG Parallel Tiers" << std::endl;
}

void test_cycle_detection() {
    std::cout << "[RUN] Testing Cycle Detection..." << std::endl;
    std::vector<ServiceConfig> configs;

    ServiceConfig s1;
    s1.name = "svc1";
    s1.dependencies = {"svc3"};
    configs.push_back(s1);

    ServiceConfig s2;
    s2.name = "svc2";
    s2.dependencies = {"svc1"};
    configs.push_back(s2);

    ServiceConfig s3;
    s3.name = "svc3";
    s3.dependencies = {"svc2"};
    configs.push_back(s3);

    DAGResolver dag;
    PilotStatus st = dag.build_graph(configs);
    assert(st == PilotStatus::ERR_DAG_CYCLE_DETECTED);

    std::cout << "[PASS] Cycle Detection Correctly Rejected Graph" << std::endl;
}

int main() {
    std::cout << "=================== Running DAG Unit Tests ===================" << std::endl;
    test_linear_dag();
    test_diamond_dag();
    test_cycle_detection();
    std::cout << "All DAG unit tests passed successfully!\n" << std::endl;
    return 0;
}
