/*
 * ProcessPilot Pro - Unix Domain Socket IPC Protocol
 * File: include/ipc/ipc_protocol.hpp
 */

#ifndef PROCESS_PILOT_IPC_PROTOCOL_HPP
#define PROCESS_PILOT_IPC_PROTOCOL_HPP

#include <string>
#include <vector>
#include <sstream>

namespace process_pilot {

#define PILOT_DEFAULT_SOCKET_PATH "/tmp/process_pilot.sock"

enum class IpcCommandType : uint32_t {
    CMD_UNKNOWN = 0,
    CMD_STATUS,
    CMD_START,
    CMD_STOP,
    CMD_RESTART,
    CMD_RELOAD,
    CMD_TREE,
    CMD_DRIVER_STATS,
    CMD_INJECT_FAULT,
    CMD_RESET_CIRCUIT,
    CMD_GET_LOGS
};

struct IpcRequest {
    IpcCommandType cmd = IpcCommandType::CMD_UNKNOWN;
    std::string target_service;
    std::string extra_arg;

    std::string serialize() const {
        std::ostringstream oss;
        oss << static_cast<uint32_t>(cmd) << "\n"
            << target_service << "\n"
            << extra_arg << "\n";
        return oss.str();
    }

    static IpcRequest deserialize(const std::string& data) {
        IpcRequest req;
        std::istringstream iss(data);
        std::string cmd_str;
        if (std::getline(iss, cmd_str)) {
            req.cmd = static_cast<IpcCommandType>(std::stoul(cmd_str));
        }
        std::getline(iss, req.target_service);
        std::getline(iss, req.extra_arg);
        return req;
    }
};

struct IpcResponse {
    bool success = false;
    std::string message;
    std::string payload;

    std::string serialize() const {
        std::ostringstream oss;
        oss << (success ? "1" : "0") << "\n"
            << message << "\n"
            << payload;
        return oss.str();
    }

    static IpcResponse deserialize(const std::string& data) {
        IpcResponse resp;
        std::istringstream iss(data);
        std::string status_str;
        if (std::getline(iss, status_str)) {
            resp.success = (status_str == "1");
        }
        std::getline(iss, resp.message);
        std::string remaining;
        std::string line;
        while (std::getline(iss, line)) {
            remaining += line + "\n";
        }
        resp.payload = remaining;
        return resp;
    }
};

} // namespace process_pilot

#endif // PROCESS_PILOT_IPC_PROTOCOL_HPP
