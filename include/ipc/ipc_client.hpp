/*
 * ProcessPilot Pro - Unix Domain Socket IPC Client
 * File: include/ipc/ipc_client.hpp
 */

#ifndef PROCESS_PILOT_IPC_CLIENT_HPP
#define PROCESS_PILOT_IPC_CLIENT_HPP

#include "ipc/ipc_protocol.hpp"
#include "common/error_codes.hpp"
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

namespace process_pilot {

class IpcClient {
public:
    static PilotStatus send_command(const IpcRequest& request,
                                    IpcResponse& out_response,
                                    const std::string& socket_path = PILOT_DEFAULT_SOCKET_PATH) {
#ifndef _WIN32
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            return PilotStatus::ERR_IPC_SOCKET_ERROR;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            return PilotStatus::ERR_IPC_CONNECT_FAILED;
        }

        std::string req_str = request.serialize();
        if (write(sock, req_str.data(), req_str.size()) < 0) {
            close(sock);
            return PilotStatus::ERR_IPC_SOCKET_ERROR;
        }

        std::string full_response;
        std::vector<char> buffer(4096, 0);
        ssize_t n;
        while ((n = read(sock, buffer.data(), buffer.size())) > 0) {
            full_response.append(buffer.data(), n);
        }

        close(sock);
        out_response = IpcResponse::deserialize(full_response);
        return PilotStatus::SUCCESS;
#else
        out_response.success = true;
        out_response.message = "[SIM] Command processed";
        return PilotStatus::SUCCESS;
#endif
    }
};

} // namespace process_pilot

#endif // PROCESS_PILOT_IPC_CLIENT_HPP
