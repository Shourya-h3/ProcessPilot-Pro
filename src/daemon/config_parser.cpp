/*
 * ProcessPilot Pro - Unit File Configuration Parser Implementation
 * File: src/daemon/config_parser.cpp
 */

#include "parser/config_parser.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace process_pilot {

std::string ConfigParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> ConfigParser::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        std::string trimmed = trim(token);
        if (!trimmed.empty()) {
            tokens.push_back(trimmed);
        }
    }
    return tokens;
}

void ConfigParser::parse_unit_section(const std::string& key, const std::string& val, ServiceConfig& cfg) {
    if (key == "Description") {
        cfg.description = val;
    } else if (key == "Requires") {
        cfg.dependencies = split(val, ',');
    } else if (key == "Wants") {
        cfg.wants = split(val, ',');
    } else if (key == "AutoStart") {
        std::string lower_val = val;
        std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
        cfg.auto_start = (lower_val == "true" || lower_val == "1" || lower_val == "yes");
    }
}

void ConfigParser::parse_service_section(const std::string& key, const std::string& val, ServiceConfig& cfg) {
    if (key == "ExecStart") {
        cfg.exec_start = val;
    } else if (key == "ExecStop") {
        cfg.exec_stop = val;
    } else if (key == "WorkingDirectory") {
        cfg.working_dir = val;
    } else if (key == "Restart") {
        cfg.restart_policy = string_to_restart_policy(val);
    } else if (key == "Environment") {
        std::vector<std::string> env_pairs = split(val, ';');
        for (const auto& pair : env_pairs) {
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                std::string k = trim(pair.substr(0, eq_pos));
                std::string v = trim(pair.substr(eq_pos + 1));
                if (!k.empty()) {
                    cfg.environment[k] = v;
                }
            }
        }
    }
}

void ConfigParser::parse_health_section(const std::string& key, const std::string& val, ServiceConfig& cfg) {
    if (key == "Type") {
        std::string lower = val;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "kernel" || lower == "kernel_watchdog" || lower == "driver") {
            cfg.health_check.type = HealthCheckType::KERNEL_WATCHDOG;
        } else if (lower == "tcp") {
            cfg.health_check.type = HealthCheckType::TCP_PORT;
        } else if (lower == "http") {
            cfg.health_check.type = HealthCheckType::HTTP_ENDPOINT;
        } else if (lower == "exec") {
            cfg.health_check.type = HealthCheckType::EXEC_COMMAND;
        } else {
            cfg.health_check.type = HealthCheckType::NONE;
        }
    } else if (key == "Endpoint") {
        cfg.health_check.endpoint = val;
    } else if (key == "IntervalMs") {
        cfg.health_check.interval_ms = std::stoul(val);
    } else if (key == "TimeoutMs") {
        cfg.health_check.timeout_ms = std::stoul(val);
    } else if (key == "MaxRetries") {
        cfg.health_check.max_retries = std::stoul(val);
    } else if (key == "KernelWatchdogMs") {
        cfg.health_check.kernel_watchdog_timeout_ms = std::stoul(val);
    }
}

void ConfigParser::parse_healing_section(const std::string& key, const std::string& val, ServiceConfig& cfg) {
    if (key == "InitialBackoffMs") {
        cfg.healing.initial_backoff_ms = std::stoul(val);
    } else if (key == "MaxBackoffMs") {
        cfg.healing.max_backoff_ms = std::stoul(val);
    } else if (key == "BackoffMultiplier") {
        cfg.healing.backoff_multiplier = std::stod(val);
    } else if (key == "MaxCrashCount") {
        cfg.healing.max_crash_count = std::stoul(val);
    } else if (key == "CrashWindowSec") {
        cfg.healing.crash_window_sec = std::stoul(val);
    }
}

void ConfigParser::parse_resource_section(const std::string& key, const std::string& val, ServiceConfig& cfg) {
    if (key == "MemoryMaxBytes") {
        cfg.resources.memory_max_bytes = std::stoull(val);
    } else if (key == "CpuQuotaPct") {
        cfg.resources.cpu_quota_pct = std::stoul(val);
    } else if (key == "MaxPids") {
        cfg.resources.max_pids = std::stoul(val);
    }
}

PilotStatus ConfigParser::parse_service_file(const std::string& file_path, ServiceConfig& out_config) {
    std::ifstream infile(file_path);
    if (!infile.is_open()) {
        LOG_ERROR("Failed to open service config file: " + file_path);
        return PilotStatus::ERR_FILE_NOT_FOUND;
    }

    // Extract base name
    size_t last_slash = file_path.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? file_path.substr(last_slash + 1) : file_path;
    size_t dot_pos = filename.find_last_of('.');
    std::string base_name = (dot_pos != std::string::npos) ? filename.substr(0, dot_pos) : filename;

    out_config = ServiceConfig{};
    out_config.name = base_name;

    std::string line;
    std::string current_section = "";

    while (std::getline(infile, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string val = trim(trimmed.substr(eq_pos + 1));

        if (current_section == "Unit") {
            parse_unit_section(key, val, out_config);
        } else if (current_section == "Service") {
            parse_service_section(key, val, out_config);
        } else if (current_section == "HealthCheck") {
            parse_health_section(key, val, out_config);
        } else if (current_section == "SelfHealing") {
            parse_healing_section(key, val, out_config);
        } else if (current_section == "ResourceLimits") {
            parse_resource_section(key, val, out_config);
        }
    }

    if (out_config.exec_start.empty()) {
        LOG_ERROR("Service unit file '" + file_path + "' is missing required ExecStart parameter");
        return PilotStatus::ERR_CONFIG_INVALID;
    }

    return PilotStatus::SUCCESS;
}

PilotStatus ConfigParser::parse_directory(const std::string& dir_path, std::vector<ServiceConfig>& out_configs) {
    out_configs.clear();

#if defined(_WIN32)
    std::string search_path = dir_path + "/*.pilot";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_WARN("No .pilot files found in directory: " + dir_path);
        return PilotStatus::ERR_FILE_NOT_FOUND;
    }

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string full_path = dir_path + "/" + fd.cFileName;
            ServiceConfig cfg;
            PilotStatus status = parse_service_file(full_path, cfg);
            if (status == PilotStatus::SUCCESS) {
                out_configs.push_back(cfg);
                LOG_INFO("Loaded service config: " + cfg.name + " (" + fd.cFileName + ")");
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        LOG_WARN("Service directory does not exist: " + dir_path);
        return PilotStatus::ERR_FILE_NOT_FOUND;
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string fname(ent->d_name);
        if (fname.size() > 6 && fname.substr(fname.size() - 6) == ".pilot") {
            std::string full_path = dir_path + "/" + fname;
            ServiceConfig cfg;
            PilotStatus status = parse_service_file(full_path, cfg);
            if (status == PilotStatus::SUCCESS) {
                out_configs.push_back(cfg);
                LOG_INFO("Loaded service config: " + cfg.name + " (" + fname + ")");
            }
        }
    }
    closedir(dir);
#endif

    return PilotStatus::SUCCESS;
}

} // namespace process_pilot
