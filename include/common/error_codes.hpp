/*
 * ProcessPilot Pro - Common Error Codes
 * File: include/common/error_codes.hpp
 */

#ifndef PROCESS_PILOT_ERROR_CODES_HPP
#define PROCESS_PILOT_ERROR_CODES_HPP

#include <string>

namespace process_pilot {

enum class PilotStatus : int {
    SUCCESS                 = 0,
    ERR_GENERIC             = -1,
    ERR_FILE_NOT_FOUND      = -2,
    ERR_CONFIG_INVALID      = -3,
    ERR_DAG_CYCLE_DETECTED  = -4,
    ERR_DAG_UNKNOWN_DEP     = -5,
    ERR_PROCESS_FORK_FAILED = -6,
    ERR_PROCESS_NOT_FOUND   = -7,
    ERR_IPC_SOCKET_ERROR    = -8,
    ERR_IPC_CONNECT_FAILED  = -9,
    ERR_DRIVER_NOT_FOUND    = -10,
    ERR_DRIVER_IOCTL_FAILED = -11,
    ERR_CIRCUIT_BROKEN      = -12,
    ERR_CGROUP_SETUP_FAILED = -13,
    ERR_PERMISSION_DENIED   = -14
};

inline std::string status_to_string(PilotStatus status) {
    switch (status) {
        case PilotStatus::SUCCESS: return "SUCCESS";
        case PilotStatus::ERR_GENERIC: return "ERR_GENERIC";
        case PilotStatus::ERR_FILE_NOT_FOUND: return "ERR_FILE_NOT_FOUND";
        case PilotStatus::ERR_CONFIG_INVALID: return "ERR_CONFIG_INVALID";
        case PilotStatus::ERR_DAG_CYCLE_DETECTED: return "ERR_DAG_CYCLE_DETECTED";
        case PilotStatus::ERR_DAG_UNKNOWN_DEP: return "ERR_DAG_UNKNOWN_DEP";
        case PilotStatus::ERR_PROCESS_FORK_FAILED: return "ERR_PROCESS_FORK_FAILED";
        case PilotStatus::ERR_PROCESS_NOT_FOUND: return "ERR_PROCESS_NOT_FOUND";
        case PilotStatus::ERR_IPC_SOCKET_ERROR: return "ERR_IPC_SOCKET_ERROR";
        case PilotStatus::ERR_IPC_CONNECT_FAILED: return "ERR_IPC_CONNECT_FAILED";
        case PilotStatus::ERR_DRIVER_NOT_FOUND: return "ERR_DRIVER_NOT_FOUND";
        case PilotStatus::ERR_DRIVER_IOCTL_FAILED: return "ERR_DRIVER_IOCTL_FAILED";
        case PilotStatus::ERR_CIRCUIT_BROKEN: return "ERR_CIRCUIT_BROKEN";
        case PilotStatus::ERR_CGROUP_SETUP_FAILED: return "ERR_CGROUP_SETUP_FAILED";
        case PilotStatus::ERR_PERMISSION_DENIED: return "ERR_PERMISSION_DENIED";
        default: return "UNKNOWN_ERROR";
    }
}

} // namespace process_pilot

#endif // PROCESS_PILOT_ERROR_CODES_HPP
