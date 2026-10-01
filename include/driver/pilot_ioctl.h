/*
 * ProcessPilot Pro - Kernel Device Driver IOCTL Interface
 * File: include/driver/pilot_ioctl.h
 *
 * Defines the shared user/kernel contracts, ioctl commands, data structures,
 * and event codes for /dev/process_pilot.
 */

#ifndef PROCESS_PILOT_IOCTL_H
#define PROCESS_PILOT_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

#define PILOT_DEVICE_NAME     "process_pilot"
#define PILOT_DEVICE_PATH     "/dev/process_pilot"
#define PILOT_CLASS_NAME      "process_pilot_class"
#define PILOT_MAX_NAME_LEN    64
#define PILOT_RING_BUFFER_CAP 256

/* Magic number for ProcessPilot driver */
#define PILOT_IOC_MAGIC       'P'

/* Event types delivered by kernel ring buffer to userspace */
enum pilot_event_type {
    PILOT_EVENT_HEARTBEAT_OK      = 0x01,
    PILOT_EVENT_HEARTBEAT_MISSED  = 0x02,
    PILOT_EVENT_WATCHDOG_EXPIRED  = 0x03,
    PILOT_EVENT_PROCESS_CRASHED   = 0x04,
    PILOT_EVENT_FAULT_INJECTED    = 0x05,
    PILOT_EVENT_DRIVER_LOADED     = 0x06,
    PILOT_EVENT_DRIVER_UNLOADING  = 0x07
};

/* Watchdog registration request structure */
struct pilot_watchdog_reg {
    pid_t    pid;                          /* Target process PID */
    uint32_t timeout_ms;                   /* Watchdog timeout in milliseconds */
    uint32_t max_misses;                   /* Maximum allowed consecutive heartbeat misses */
    char     service_name[PILOT_MAX_NAME_LEN]; /* Associated service name */
};

/* Heartbeat ping structure */
struct pilot_heartbeat_ping {
    pid_t    pid;                          /* Target PID sending heartbeat */
    uint64_t timestamp_ns;                 /* Monotonic timestamp */
    uint32_t sequence_num;                 /* Heartbeat sequence counter */
};

/* Fault injection request structure */
struct pilot_fault_inject {
    pid_t    target_pid;                   /* Target PID to disrupt */
    uint32_t fault_type;                   /* 1: Drop heartbeats, 2: Kernel panic simulation, 3: Delay */
    uint32_t delay_ms;                     /* Delay in ms if applicable */
};

/* Driver statistics snapshot */
struct pilot_driver_stats {
    uint64_t total_heartbeats_received;
    uint64_t total_watchdogs_registered;
    uint64_t total_watchdogs_expired;
    uint64_t total_missed_deadlines;
    uint64_t total_events_queued;
    uint64_t total_events_dropped;
    uint32_t active_watchdogs_count;
    uint32_t driver_version_major;
    uint32_t driver_version_minor;
};

/* Asynchronous Event structure stored in the driver's ring buffer */
struct pilot_event {
    uint64_t timestamp_ns;
    uint32_t event_type;                   /* enum pilot_event_type */
    pid_t    pid;
    uint32_t missed_count;
    char     service_name[PILOT_MAX_NAME_LEN];
    char     message[128];
};

/* IOCTL Command Definitions */
#define PILOT_IOCTL_REGISTER_WATCHDOG   _IOW(PILOT_IOC_MAGIC, 1, struct pilot_watchdog_reg)
#define PILOT_IOCTL_UNREGISTER_WATCHDOG _IOW(PILOT_IOC_MAGIC, 2, pid_t)
#define PILOT_IOCTL_PING_HEARTBEAT      _IOW(PILOT_IOC_MAGIC, 3, struct pilot_heartbeat_ping)
#define PILOT_IOCTL_GET_STATS           _IOR(PILOT_IOC_MAGIC, 4, struct pilot_driver_stats)
#define PILOT_IOCTL_INJECT_FAULT        _IOW(PILOT_IOC_MAGIC, 5, struct pilot_fault_inject)
#define PILOT_IOCTL_RESET_STATS         _IO(PILOT_IOC_MAGIC, 6)

#endif /* PROCESS_PILOT_IOCTL_H */
