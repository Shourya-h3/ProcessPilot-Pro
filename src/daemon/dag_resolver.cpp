/*
 * ProcessPilot Pro - DAG Dependency Resolver Implementation
 * File: src/daemon/dag_resolver.cpp
 */

#include "engine/dag_resolver.hpp"
#include "common/logger.hpp"
#include <queue>
#include <algorithm>
#include <sstream>

namespace process_pilot {

PilotStatus DAGResolver::build_graph(const std::vector<ServiceConfig>& configs) {
    services_.clear();
    adj_list_.clear();
    reverse_adj_.clear();

    for (const auto& cfg : configs) {
        services_[cfg.name] = cfg;
        adj_list_[cfg.name] = std::set<std::string>();
        reverse_adj_[cfg.name] = std::set<std::string>();
    }

    // Populate edges
    for (const auto& cfg : configs) {
        for (const auto& dep_raw : cfg.dependencies) {
            // strip potential ".pilot" extension if user wrote "database.pilot"
            std::string dep = dep_raw;
            if (dep.size() > 6 && dep.substr(dep.size() - 6) == ".pilot") {
                dep = dep.substr(0, dep.size() - 6);
            }

            if (services_.find(dep) == services_.end()) {
                LOG_ERROR("Service '" + cfg.name + "' specifies non-existent dependency: '" + dep + "'");
                return PilotStatus::ERR_DAG_UNKNOWN_DEP;
            }

            adj_list_[cfg.name].insert(dep);
            reverse_adj_[dep].insert(cfg.name);
        }
    }

    // Check for cycles
    std::vector<std::string> order;
    PilotStatus topo_status = get_topological_order(order);
    if (topo_status != PilotStatus::SUCCESS) {
        return topo_status;
    }

    return PilotStatus::SUCCESS;
}

bool DAGResolver::detect_cycle_util(const std::string& node,
                                    std::map<std::string, int>& visited_state,
                                    std::vector<std::string>& cycle_path) const {
    // visited_state: 0 = unvisited, 1 = visiting (in recursion stack), 2 = visited
    visited_state[node] = 1;
    cycle_path.push_back(node);

    auto it = adj_list_.find(node);
    if (it != adj_list_.end()) {
        for (const auto& neighbor : it->second) {
            if (visited_state[neighbor] == 1) {
                cycle_path.push_back(neighbor);
                return true; // Cycle found
            }
            if (visited_state[neighbor] == 0) {
                if (detect_cycle_util(neighbor, visited_state, cycle_path)) {
                    return true;
                }
            }
        }
    }

    visited_state[node] = 2;
    cycle_path.pop_back();
    return false;
}

PilotStatus DAGResolver::get_topological_order(std::vector<std::string>& out_order) const {
    out_order.clear();
    std::map<std::string, int> in_degree;

    for (const auto& pair : services_) {
        in_degree[pair.first] = 0;
    }

    for (const auto& pair : adj_list_) {
        // pair.first depends on elements in pair.second
        in_degree[pair.first] = pair.second.size();
    }

    std::queue<std::string> q;
    for (const auto& pair : in_degree) {
        if (pair.second == 0) {
            q.push(pair.first);
        }
    }

    while (!q.empty()) {
        std::string current = q.front();
        q.pop();
        out_order.push_back(current);

        // Nodes that depend on current node can have their in-degree decremented
        auto rev_it = reverse_adj_.find(current);
        if (rev_it != reverse_adj_.end()) {
            for (const auto& dependent : rev_it->second) {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    q.push(dependent);
                }
            }
        }
    }

    if (out_order.size() != services_.size()) {
        // Cycle detected, perform DFS to trace the cycle path
        std::map<std::string, int> visited;
        std::vector<std::string> cycle_path;
        for (const auto& pair : services_) {
            visited[pair.first] = 0;
        }

        for (const auto& pair : services_) {
            if (visited[pair.first] == 0) {
                if (detect_cycle_util(pair.first, visited, cycle_path)) {
                    std::ostringstream ss;
                    ss << "Dependency Cycle Detected: ";
                    for (size_t i = 0; i < cycle_path.size(); ++i) {
                        ss << cycle_path[i] << (i + 1 < cycle_path.size() ? " -> " : "");
                    }
                    LOG_ERROR(ss.str());
                    break;
                }
            }
        }
        return PilotStatus::ERR_DAG_CYCLE_DETECTED;
    }

    return PilotStatus::SUCCESS;
}

PilotStatus DAGResolver::get_execution_tiers(std::vector<std::vector<std::string>>& out_tiers) const {
    out_tiers.clear();
    std::map<std::string, int> in_degree;
    std::map<std::string, int> node_level;

    for (const auto& pair : services_) {
        in_degree[pair.first] = 0;
        node_level[pair.first] = 0;
    }

    for (const auto& pair : adj_list_) {
        in_degree[pair.first] = pair.second.size();
    }

    std::queue<std::string> q;
    for (const auto& pair : in_degree) {
        if (pair.second == 0) {
            q.push(pair.first);
            node_level[pair.first] = 0;
        }
    }

    size_t processed_count = 0;
    int max_level = 0;

    while (!q.empty()) {
        std::string current = q.front();
        q.pop();
        processed_count++;

        int curr_lvl = node_level[current];
        max_level = std::max(max_level, curr_lvl);

        auto rev_it = reverse_adj_.find(current);
        if (rev_it != reverse_adj_.end()) {
            for (const auto& dependent : rev_it->second) {
                node_level[dependent] = std::max(node_level[dependent], curr_lvl + 1);
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    q.push(dependent);
                }
            }
        }
    }

    if (processed_count != services_.size()) {
        return PilotStatus::ERR_DAG_CYCLE_DETECTED;
    }

    out_tiers.resize(max_level + 1);
    for (const auto& pair : node_level) {
        out_tiers[pair.second].push_back(pair.first);
    }

    // Sort nodes in each tier for deterministic output
    for (auto& tier : out_tiers) {
        std::sort(tier.begin(), tier.end());
    }

    return PilotStatus::SUCCESS;
}

std::vector<std::string> DAGResolver::get_dependencies(const std::string& service_name) const {
    auto it = adj_list_.find(service_name);
    if (it != adj_list_.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    return {};
}

std::vector<std::string> DAGResolver::get_dependents(const std::string& service_name) const {
    auto it = reverse_adj_.find(service_name);
    if (it != reverse_adj_.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    return {};
}

bool DAGResolver::has_service(const std::string& service_name) const {
    return services_.find(service_name) != services_.end();
}

std::string DAGResolver::generate_ascii_tree() const {
    std::vector<std::vector<std::string>> tiers;
    if (get_execution_tiers(tiers) != PilotStatus::SUCCESS) {
        return "Error: Cannot generate tree (Cycle detected in graph)";
    }

    std::ostringstream ss;
    ss << "=== ProcessPilot Dependency Execution Graph ===\n";

    for (size_t lvl = 0; lvl < tiers.size(); ++lvl) {
        ss << "\n[Tier " << lvl << "] ";
        if (lvl == 0) ss << "(Root / Independent Services)\n";
        else ss << "(Dependent on Tier " << (lvl - 1) << ")\n";

        for (const auto& svc : tiers[lvl]) {
            ss << "  ├── " << svc;
            auto deps = get_dependencies(svc);
            if (!deps.empty()) {
                ss << " [requires: ";
                for (size_t i = 0; i < deps.size(); ++i) {
                    ss << deps[i] << (i + 1 < deps.size() ? ", " : "");
                }
                ss << "]";
            }
            ss << "\n";
        }
    }
    ss << "\n===============================================\n";
    return ss.str();
}

} // namespace process_pilot
