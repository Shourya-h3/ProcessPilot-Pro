/*
 * ProcessPilot Pro - Sample Microservice Binary
 * File: services.d/mock_service.c
 *
 * Description:
 *   Lightweight worker process that simulates application activity,
 *   responds to signals, and optionally interacts with /dev/process_pilot.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t g_running = 1;

static void handle_sigterm(int sig) {
    (void)sig;
    printf("[MOCK-SVC] Received termination signal. Cleaning up...\n");
    g_running = 0;
}

int main(int argc, char* argv[]) {
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);

    const char* svc_name = getenv("PILOT_SERVICE_NAME");
    if (!svc_name) {
        svc_name = (argc > 1) ? argv[1] : "generic_service";
    }

    printf("[MOCK-SVC] Starting microservice: '%s' (PID: %d)\n", svc_name, getpid());
    fflush(stdout);

    int tick = 0;
    while (g_running) {
        sleep(1);
        tick++;
        printf("[MOCK-SVC] [%s] Heartbeat tick #%d (PID: %d)\n", svc_name, tick, getpid());
        fflush(stdout);
    }

    printf("[MOCK-SVC] Microservice '%s' (PID: %d) shut down cleanly.\n", svc_name, getpid());
    return 0;
}
