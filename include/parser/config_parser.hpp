/*
 * ProcessPilot Pro - Unit File Configuration Parser
 * File: include/parser/config_parser.hpp
 */

#ifndef PROCESS_PILOT_CONFIG_PARSER_HPP
#define PROCESS_PILOT_CONFIG_PARSER_HPP

#include "common/pilot_types.hpp"
#include "common/error_codes.hpp"
#include <string>
#include <vector>
#include <memory>

namespace process_pilot {

class ConfigParser {
public:
    static PilotStatus parse_service_file(const std::string& file_path, ServiceConfig& out_config);
    static PilotStatus parse_directory(const std::string& dir_path, std::vector<ServiceConfig>& out_configs);

private:
    static std::string trim(const std::string& str);
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static void parse_unit_section(const std::string& key, const std::string& val, ServiceConfig& cfg);
    static void parse_service_section(const std::string& key, const std::string& val, ServiceConfig& cfg);
    static void parse_health_section(const std::string& key, const std::string& val, ServiceConfig& cfg);
    static void parse_healing_section(const std::string& key, const std::string& val, ServiceConfig& cfg);
    static void parse_resource_section(const std::string& key, const std::string& val, ServiceConfig& cfg);
};

} // namespace process_pilot

#endif // PROCESS_PILOT_CONFIG_PARSER_HPP
