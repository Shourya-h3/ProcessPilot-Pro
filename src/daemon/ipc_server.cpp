/*
 * ProcessPilot Pro - Unix Domain Socket IPC Server Implementation
 * File: src/daemon/ipc_server.cpp
 */

#include "ipc/ipc_server.hpp"
#include "common/logger.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <vector>

namespace process_pilot {

IpcServer::IpcServer(const std::string& socket_path)
    : socket_path_(socket_path) {
}

IpcServer::~IpcServer() {
    stop();
}

PilotStatus IpcServer::start(IpcHandlerCallback handler) {
    handler_ = handler;
#ifndef _WIN32
    unlink(socket_path_.c_str());

    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        LOG_ERROR("Failed to create IPC domain socket: " + std::string(strerror(errno)));
        return PilotStatus::ERR_IPC_SOCKET_ERROR;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind IPC domain socket to " + socket_path_ + ": " + std::string(strerror(errno)));
        close(server_fd_);
        server_fd_ = -1;
        return PilotStatus::ERR_IPC_SOCKET_ERROR;
    }

    if (listen(server_fd_, 16) < 0) {
        LOG_ERROR("Failed to listen on IPC socket: " + std::string(strerror(errno)));
        close(server_fd_);
        server_fd_ = -1;
        return PilotStatus::ERR_IPC_SOCKET_ERROR;
    }

    running_ = true;
    worker_thread_ = std::thread(&IpcServer::run_event_loop, this);
    LOG_INFO("IPC Server listening on " + socket_path_);
#endif
    return PilotStatus::SUCCESS;
}

void IpcServer::stop() {
    running_ = false;
#ifndef _WIN32
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    unlink(socket_path_.c_str());
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
#endif
}

void IpcServer::run_event_loop() {
#ifndef _WIN32
    while (running_) {
        struct pollfd pfd;
        pfd.fd = server_fd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 200); // 200ms timeout
        if (ret > 0 && (pfd.revents & POLLIN)) {
            int client_fd = accept(server_fd_, nullptr, nullptr);
            if (client_fd >= 0) {
                handle_client(client_fd);
                close(client_fd);
            }
        }
    }
#endif
}

void IpcServer::handle_client(int client_fd) {
#ifndef _WIN32
    std::vector<char> buffer(4096, 0);
    ssize_t bytes_read = read(client_fd, buffer.data(), buffer.size() - 1);
    if (bytes_read <= 0) return;

    std::string req_str(buffer.data(), bytes_read);
    IpcRequest req = IpcRequest::deserialize(req_str);

    IpcResponse resp;
    if (handler_) {
        resp = handler_(req);
    } else {
        resp.success = false;
        resp.message = "No handler registered";
    }

    std::string resp_str = resp.serialize();
    write(client_fd, resp_str.data(), resp_str.size());
#endif
}

} // namespace process_pilot
