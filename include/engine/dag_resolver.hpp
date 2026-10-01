/*
 * ProcessPilot Pro - DAG Dependency Resolver
 * File: include/engine/dag_resolver.hpp
 */

#ifndef PROCESS_PILOT_DAG_RESOLVER_HPP
#define PROCESS_PILOT_DAG_RESOLVER_HPP

#include "common/pilot_types.hpp"
#include "common/error_codes.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace process_pilot {

class DAGResolver {
public:
    DAGResolver() = default;

    /* Build DAG from service configurations */
    PilotStatus build_graph(const std::vector<ServiceConfig>& configs);

    /* Validate acyclicity and compute sequential topological execution order */
    PilotStatus get_topological_order(std::vector<std::string>& out_order) const;

    /* Compute parallel execution tiers (Level 0: no deps, Level 1: depends on L0, etc.) */
    PilotStatus get_execution_tiers(std::vector<std::vector<std::string>>& out_tiers) const;

    /* Get direct dependencies of a service (services this service depends on) */
    std::vector<std::string> get_dependencies(const std::string& service_name) const;

    /* Get direct dependents (services that depend on this service) */
    std::vector<std::string> get_dependents(const std::string& service_name) const;

    /* Check if graph contains a service */
    bool has_service(const std::string& service_name) const;

    /* Generate ASCII tree visualization of the DAG */
    std::string generate_ascii_tree() const;

private:
    std::map<std::string, ServiceConfig> services_;
    std::map<std::string, std::set<std::string>> adj_list_;      /* service -> direct dependencies */
    std::map<std::string, std::set<std::string>> reverse_adj_;   /* service -> dependents */

    bool detect_cycle_util(const std::string& node,
                           std::map<std::string, int>& visited_state,
                           std::vector<std::string>& cycle_path) const;
};

} // namespace process_pilot

#endif // PROCESS_PILOT_DAG_RESOLVER_HPP
