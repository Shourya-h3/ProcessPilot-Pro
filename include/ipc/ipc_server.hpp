/*
 * ProcessPilot Pro - Unix Domain Socket IPC Server
 * File: include/ipc/ipc_server.hpp
 */

#ifndef PROCESS_PILOT_IPC_SERVER_HPP
#define PROCESS_PILOT_IPC_SERVER_HPP

#include "ipc/ipc_protocol.hpp"
#include "common/error_codes.hpp"
#include <string>
#include <functional>
#include <thread>
#include <atomic>

namespace process_pilot {

using IpcHandlerCallback = std::function<IpcResponse(const IpcRequest&)>;

class IpcServer {
public:
    IpcServer(const std::string& socket_path = PILOT_DEFAULT_SOCKET_PATH);
    ~IpcServer();

    PilotStatus start(IpcHandlerCallback handler);
    void stop();
    bool is_running() const { return running_; }

private:
    std::string socket_path_;
    int server_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    IpcHandlerCallback handler_;

    void run_event_loop();
    void handle_client(int client_fd);
};

} // namespace process_pilot

#endif // PROCESS_PILOT_IPC_SERVER_HPP
