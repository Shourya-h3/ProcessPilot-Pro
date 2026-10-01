/*
 * ProcessPilot Pro - Linux Kernel Module Internal Header
 * File: src/driver/pilot_driver.h
 */

#ifndef PILOT_DRIVER_INTERNAL_H
#define PILOT_DRIVER_INTERNAL_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#include "../../include/driver/pilot_ioctl.h"

#define DRIVER_AUTHOR      "ProcessPilot Pro Team"
#define DRIVER_DESC        "ProcessPilot Pro Kernel Watchdog & Event Driver"
#define DRIVER_VERSION_MAJ 1
#define DRIVER_VERSION_MIN 0

/* Maximum number of concurrently monitored processes in kernel space */
#define PILOT_MAX_WATCHDOGS 128

/* Monitored process descriptor inside kernel */
struct pilot_monitored_proc {
    struct list_head    list;
    pid_t               pid;
    char                service_name[PILOT_MAX_NAME_LEN];
    uint32_t            timeout_ms;
    uint32_t            max_misses;
    uint32_t            current_misses;
    uint32_t            last_sequence;
    u64                 last_heartbeat_ktime;
    struct timer_list   watchdog_timer;
    bool                fault_injected;
    bool                active;
};

/* Global Device Driver Context */
struct pilot_device_ctx {
    dev_t                       dev_num;
    struct cdev                 cdev;
    struct class               *dev_class;
    struct device              *device;

    /* Synchronization */
    struct mutex                lock;
    spinlock_t                  ring_lock;

    /* Monitored Watchdogs */
    struct list_head            watchdog_list;
    uint32_t                    active_watchdog_count;

    /* Circular Event Ring Buffer */
    struct pilot_event          ring_buffer[PILOT_RING_BUFFER_CAP];
    uint32_t                    ring_head;
    uint32_t                    ring_tail;
    uint32_t                    ring_count;
    wait_queue_head_t           read_wait;

    /* Statistics */
    struct pilot_driver_stats   stats;
};

/* Function Prototypes */
int  pilot_ring_push(struct pilot_device_ctx *ctx, const struct pilot_event *ev);
int  pilot_ring_pop(struct pilot_device_ctx *ctx, struct pilot_event *ev);
void pilot_watchdog_timer_callback(struct timer_list *t);

#endif /* PILOT_DRIVER_INTERNAL_H */
