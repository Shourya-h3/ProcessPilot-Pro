/*
 * ProcessPilot Pro - Linux Kernel Module Implementation
 * File: src/driver/pilot_driver.c
 *
 * Description:
 *   High-reliability Character Device Driver (/dev/process_pilot) providing:
 *   - Kernel-level Watchdog timers with customizable millisecond deadlines
 *   - Lockless circular event ring buffer for out-of-band crash telemetry
 *   - Sysfs attribute tree under /sys/class/process_pilot/
 *   - Fault injection simulator for chaos testing userspace self-healing
 */

#include "pilot_driver.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION("1.0.0");

static struct pilot_device_ctx g_pilot_ctx;

/* ========================================================================= */
/*                      Ring Buffer Implementation                           */
/* ========================================================================= */

int pilot_ring_push(struct pilot_device_ctx *ctx, const struct pilot_event *ev)
{
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&ctx->ring_lock, flags);

    if (ctx->ring_count >= PILOT_RING_BUFFER_CAP) {
        /* Drop oldest event or mark dropped */
        ctx->ring_head = (ctx->ring_head + 1) % PILOT_RING_BUFFER_CAP;
        ctx->ring_count--;
        ctx->stats.total_events_dropped++;
    }

    ctx->ring_buffer[ctx->ring_tail] = *ev;
    ctx->ring_tail = (ctx->ring_tail + 1) % PILOT_RING_BUFFER_CAP;
    ctx->ring_count++;
    ctx->stats.total_events_queued++;

    spin_unlock_irqrestore(&ctx->ring_lock, flags);

    wake_up_interruptible(&ctx->read_wait);
    return ret;
}

int pilot_ring_pop(struct pilot_device_ctx *ctx, struct pilot_event *ev)
{
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&ctx->ring_lock, flags);

    if (ctx->ring_count == 0) {
        ret = -EAGAIN;
    } else {
        *ev = ctx->ring_buffer[ctx->ring_head];
        ctx->ring_head = (ctx->ring_head + 1) % PILOT_RING_BUFFER_CAP;
        ctx->ring_count--;
    }

    spin_unlock_irqrestore(&ctx->ring_lock, flags);
    return ret;
}

/* ========================================================================= */
/*                   Watchdog Timer Expiration Callback                      */
/* ========================================================================= */

void pilot_watchdog_timer_callback(struct timer_list *t)
{
    struct pilot_monitored_proc *proc = from_timer(proc, t, watchdog_timer);
    struct pilot_event ev;
    u64 now_ns;

    if (!proc || !proc->active)
        return;

    now_ns = ktime_get_real_ns();
    proc->current_misses++;
    g_pilot_ctx.stats.total_missed_deadlines++;

    memset(&ev, 0, sizeof(ev));
    ev.timestamp_ns = now_ns;
    ev.pid = proc->pid;
    ev.missed_count = proc->current_misses;
    strncpy(ev.service_name, proc->service_name, sizeof(ev.service_name) - 1);

    if (proc->current_misses >= proc->max_misses) {
        ev.event_type = PILOT_EVENT_WATCHDOG_EXPIRED;
        snprintf(ev.message, sizeof(ev.message),
                 "CRITICAL: Service '%s' (PID %d) expired! Misses: %u/%u",
                 proc->service_name, proc->pid, proc->current_misses, proc->max_misses);
        g_pilot_ctx.stats.total_watchdogs_expired++;
        pr_warn("process_pilot: [WATCHDOG EXPIRED] %s\n", ev.message);
    } else {
        ev.event_type = PILOT_EVENT_HEARTBEAT_MISSED;
        snprintf(ev.message, sizeof(ev.message),
                 "WARNING: Service '%s' (PID %d) missed heartbeat (%u/%u)",
                 proc->service_name, proc->pid, proc->current_misses, proc->max_misses);
        pr_info("process_pilot: [HEARTBEAT MISSED] %s\n", ev.message);

        /* Reschedule next timeout if not completely expired */
        mod_timer(&proc->watchdog_timer, jiffies + msecs_to_jiffies(proc->timeout_ms));
    }

    pilot_ring_push(&g_pilot_ctx, &ev);
}

/* ========================================================================= */
/*                          Sysfs Attributes                                 */
/* ========================================================================= */

static ssize_t stats_show(struct class *class, struct class_attribute *attr, char *buf)
{
    ssize_t count;
    mutex_lock(&g_pilot_ctx.lock);
    count = sprintf(buf,
                    "ProcessPilot Driver Status:\n"
                    "  Version: %u.%u\n"
                    "  Active Watchdogs: %u\n"
                    "  Heartbeats Received: %llu\n"
                    "  Watchdogs Registered: %llu\n"
                    "  Watchdogs Expired: %llu\n"
                    "  Missed Deadlines: %llu\n"
                    "  Events Queued: %llu\n"
                    "  Events Dropped: %llu\n",
                    g_pilot_ctx.stats.driver_version_major,
                    g_pilot_ctx.stats.driver_version_minor,
                    g_pilot_ctx.active_watchdog_count,
                    g_pilot_ctx.stats.total_heartbeats_received,
                    g_pilot_ctx.stats.total_watchdogs_registered,
                    g_pilot_ctx.stats.total_watchdogs_expired,
                    g_pilot_ctx.stats.total_missed_deadlines,
                    g_pilot_ctx.stats.total_events_queued,
                    g_pilot_ctx.stats.total_events_dropped);
    mutex_unlock(&g_pilot_ctx.lock);
    return count;
}

static CLASS_ATTR_RO(stats);

/* ========================================================================= */
/*                          File Operations                                  */
/* ========================================================================= */

static int pilot_open(struct inode *inode, struct file *file)
{
    pr_debug("process_pilot: Device opened by PID %d\n", current->pid);
    return 0;
}

static int pilot_release(struct inode *inode, struct file *file)
{
    pr_debug("process_pilot: Device released by PID %d\n", current->pid);
    return 0;
}

static ssize_t pilot_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct pilot_event ev;
    int ret;

    if (count < sizeof(struct pilot_event))
        return -EINVAL;

    while (1) {
        ret = pilot_ring_pop(&g_pilot_ctx, &ev);
        if (ret == 0) {
            /* Event successfully retrieved */
            if (copy_to_user(buf, &ev, sizeof(struct pilot_event)))
                return -EFAULT;
            return sizeof(struct pilot_event);
        }

        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        /* Wait for events to arrive */
        if (wait_event_interruptible(g_pilot_ctx.read_wait, g_pilot_ctx.ring_count > 0))
            return -ERESTARTSYS;
    }
}

static __poll_t pilot_poll(struct file *file, poll_table *wait)
{
    __poll_t mask = 0;

    poll_wait(file, &g_pilot_ctx.read_wait, wait);

    spin_lock_irq(&g_pilot_ctx.ring_lock);
    if (g_pilot_ctx.ring_count > 0)
        mask |= EPOLLIN | EPOLLRDNORM;
    spin_unlock_irq(&g_pilot_ctx.ring_lock);

    return mask;
}

static long pilot_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct pilot_monitored_proc *proc = NULL, *tmp;

    mutex_lock(&g_pilot_ctx.lock);

    switch (cmd) {
    case PILOT_IOCTL_REGISTER_WATCHDOG: {
        struct pilot_watchdog_reg reg;
        if (copy_from_user(&reg, (void __user *)arg, sizeof(reg))) {
            ret = -EFAULT;
            break;
        }

        if (reg.timeout_ms < 50 || reg.timeout_ms > 60000) {
            ret = -EINVAL;
            break;
        }

        /* Check if PID already registered */
        list_for_each_entry(tmp, &g_pilot_ctx.watchdog_list, list) {
            if (tmp->pid == reg.pid) {
                proc = tmp;
                break;
            }
        }

        if (!proc) {
            proc = kzalloc(sizeof(*proc), GFP_KERNEL);
            if (!proc) {
                ret = -ENOMEM;
                break;
            }
            proc->pid = reg.pid;
            strncpy(proc->service_name, reg.service_name, sizeof(proc->service_name) - 1);
            timer_setup(&proc->watchdog_timer, pilot_watchdog_timer_callback, 0);
            list_add_tail(&proc->list, &g_pilot_ctx.watchdog_list);
            g_pilot_ctx.active_watchdog_count++;
            g_pilot_ctx.stats.total_watchdogs_registered++;
        }

        proc->timeout_ms = reg.timeout_ms;
        proc->max_misses = (reg.max_misses > 0) ? reg.max_misses : 3;
        proc->current_misses = 0;
        proc->fault_injected = false;
        proc->active = true;
        proc->last_heartbeat_ktime = ktime_get_real_ns();

        mod_timer(&proc->watchdog_timer, jiffies + msecs_to_jiffies(proc->timeout_ms));
        pr_info("process_pilot: Registered watchdog for '%s' (PID %d), timeout: %u ms\n",
                proc->service_name, proc->pid, proc->timeout_ms);
        break;
    }

    case PILOT_IOCTL_UNREGISTER_WATCHDOG: {
        pid_t target_pid;
        bool found = false;

        if (copy_from_user(&target_pid, (void __user *)arg, sizeof(target_pid))) {
            ret = -EFAULT;
            break;
        }

        list_for_each_entry_safe(proc, tmp, &g_pilot_ctx.watchdog_list, list) {
            if (proc->pid == target_pid) {
                proc->active = false;
                del_timer_sync(&proc->watchdog_timer);
                list_del(&proc->list);
                kfree(proc);
                if (g_pilot_ctx.active_watchdog_count > 0)
                    g_pilot_ctx.active_watchdog_count--;
                found = true;
                pr_info("process_pilot: Unregistered watchdog for PID %d\n", target_pid);
                break;
            }
        }
        if (!found)
            ret = -ESRCH;
        break;
    }

    case PILOT_IOCTL_PING_HEARTBEAT: {
        struct pilot_heartbeat_ping ping;
        bool found = false;

        if (copy_from_user(&ping, (void __user *)arg, sizeof(ping))) {
            ret = -EFAULT;
            break;
        }

        list_for_each_entry(proc, &g_pilot_ctx.watchdog_list, list) {
            if (proc->pid == ping.pid && proc->active) {
                found = true;
                if (proc->fault_injected) {
                    /* Fault simulator: discard heartbeat */
                    pr_debug("process_pilot: Fault active: Discarded heartbeat for PID %d\n", ping.pid);
                    break;
                }

                proc->current_misses = 0;
                proc->last_sequence = ping.sequence_num;
                proc->last_heartbeat_ktime = ktime_get_real_ns();
                g_pilot_ctx.stats.total_heartbeats_received++;

                mod_timer(&proc->watchdog_timer, jiffies + msecs_to_jiffies(proc->timeout_ms));
                break;
            }
        }

        if (!found)
            ret = -ESRCH;
        break;
    }

    case PILOT_IOCTL_GET_STATS: {
        g_pilot_ctx.stats.active_watchdogs_count = g_pilot_ctx.active_watchdog_count;
        if (copy_to_user((void __user *)arg, &g_pilot_ctx.stats, sizeof(g_pilot_ctx.stats)))
            ret = -EFAULT;
        break;
    }

    case PILOT_IOCTL_INJECT_FAULT: {
        struct pilot_fault_inject fault;
        bool found = false;

        if (copy_from_user(&fault, (void __user *)arg, sizeof(fault))) {
            ret = -EFAULT;
            break;
        }

        list_for_each_entry(proc, &g_pilot_ctx.watchdog_list, list) {
            if (proc->pid == fault.target_pid) {
                struct pilot_event ev;
                proc->fault_injected = (fault.fault_type != 0);
                found = true;

                memset(&ev, 0, sizeof(ev));
                ev.timestamp_ns = ktime_get_real_ns();
                ev.event_type = PILOT_EVENT_FAULT_INJECTED;
                ev.pid = proc->pid;
                strncpy(ev.service_name, proc->service_name, sizeof(ev.service_name) - 1);
                snprintf(ev.message, sizeof(ev.message),
                         "Fault injection (type: %u) set on PID %d", fault.fault_type, proc->pid);
                pilot_ring_push(&g_pilot_ctx, &ev);
                pr_info("process_pilot: %s\n", ev.message);
                break;
            }
        }
        if (!found)
            ret = -ESRCH;
        break;
    }

    case PILOT_IOCTL_RESET_STATS: {
        g_pilot_ctx.stats.total_heartbeats_received = 0;
        g_pilot_ctx.stats.total_watchdogs_registered = 0;
        g_pilot_ctx.stats.total_watchdogs_expired = 0;
        g_pilot_ctx.stats.total_missed_deadlines = 0;
        g_pilot_ctx.stats.total_events_queued = 0;
        g_pilot_ctx.stats.total_events_dropped = 0;
        break;
    }

    default:
        ret = -ENOTTY;
        break;
    }

    mutex_unlock(&g_pilot_ctx.lock);
    return ret;
}

static const struct file_operations pilot_fops = {
    .owner          = THIS_MODULE,
    .open           = pilot_open,
    .release        = pilot_release,
    .read           = pilot_read,
    .poll           = pilot_poll,
    .unlocked_ioctl = pilot_ioctl,
};

/* ========================================================================= */
/*                      Module Init & Exit                                   */
/* ========================================================================= */

static int __init pilot_driver_init(void)
{
    int ret;
    struct pilot_event ev;

    pr_info("process_pilot: Initializing ProcessPilot Pro Kernel Driver...\n");

    memset(&g_pilot_ctx, 0, sizeof(g_pilot_ctx));
    mutex_init(&g_pilot_ctx.lock);
    spin_lock_init(&g_pilot_ctx.ring_lock);
    INIT_LIST_HEAD(&g_pilot_ctx.watchdog_list);
    init_waitqueue_head(&g_pilot_ctx.read_wait);

    g_pilot_ctx.stats.driver_version_major = DRIVER_VERSION_MAJ;
    g_pilot_ctx.stats.driver_version_minor = DRIVER_VERSION_MIN;

    /* 1. Allocate dynamic char device number */
    ret = alloc_chrdev_region(&g_pilot_ctx.dev_num, 0, 1, PILOT_DEVICE_NAME);
    if (ret < 0) {
        pr_err("process_pilot: Failed to allocate chrdev region (err: %d)\n", ret);
        return ret;
    }

    /* 2. Initialize and add cdev */
    cdev_init(&g_pilot_ctx.cdev, &pilot_fops);
    g_pilot_ctx.cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_pilot_ctx.cdev, g_pilot_ctx.dev_num, 1);
    if (ret < 0) {
        pr_err("process_pilot: Failed to add cdev (err: %d)\n", ret);
        goto unregister_chrdev;
    }

    /* 3. Create device class */
    g_pilot_ctx.dev_class = class_create(PILOT_CLASS_NAME);
    if (IS_ERR(g_pilot_ctx.dev_class)) {
        ret = PTR_ERR(g_pilot_ctx.dev_class);
        pr_err("process_pilot: Failed to create device class (err: %d)\n", ret);
        goto delete_cdev;
    }

    /* 4. Create sysfs class attribute */
    ret = class_create_file(g_pilot_ctx.dev_class, &class_attr_stats);
    if (ret < 0) {
        pr_warn("process_pilot: Warning: Failed to create sysfs stats attribute\n");
    }

    /* 5. Create device node /dev/process_pilot */
    g_pilot_ctx.device = device_create(g_pilot_ctx.dev_class, NULL,
                                       g_pilot_ctx.dev_num, NULL,
                                       PILOT_DEVICE_NAME);
    if (IS_ERR(g_pilot_ctx.device)) {
        ret = PTR_ERR(g_pilot_ctx.device);
        pr_err("process_pilot: Failed to create device (err: %d)\n", ret);
        goto remove_class_file;
    }

    /* Emit driver loaded event */
    memset(&ev, 0, sizeof(ev));
    ev.timestamp_ns = ktime_get_real_ns();
    ev.event_type = PILOT_EVENT_DRIVER_LOADED;
    snprintf(ev.message, sizeof(ev.message), "ProcessPilot kernel module v%u.%u loaded successfully",
             DRIVER_VERSION_MAJ, DRIVER_VERSION_MIN);
    pilot_ring_push(&g_pilot_ctx, &ev);

    pr_info("process_pilot: Driver loaded successfully. Major: %d, Minor: %d\n",
            MAJOR(g_pilot_ctx.dev_num), MINOR(g_pilot_ctx.dev_num));
    return 0;

remove_class_file:
    class_remove_file(g_pilot_ctx.dev_class, &class_attr_stats);
    class_destroy(g_pilot_ctx.dev_class);
delete_cdev:
    cdev_del(&g_pilot_ctx.cdev);
unregister_chrdev:
    unregister_chrdev_region(g_pilot_ctx.dev_num, 1);
    return ret;
}

static void __exit pilot_driver_exit(void)
{
    struct pilot_monitored_proc *proc, *tmp;

    pr_info("process_pilot: Unloading ProcessPilot Pro Driver...\n");

    mutex_lock(&g_pilot_ctx.lock);
    list_for_each_entry_safe(proc, tmp, &g_pilot_ctx.watchdog_list, list) {
        proc->active = false;
        del_timer_sync(&proc->watchdog_timer);
        list_del(&proc->list);
        kfree(proc);
    }
    mutex_unlock(&g_pilot_ctx.lock);

    device_destroy(g_pilot_ctx.dev_class, g_pilot_ctx.dev_num);
    class_remove_file(g_pilot_ctx.dev_class, &class_attr_stats);
    class_destroy(g_pilot_ctx.dev_class);
    cdev_del(&g_pilot_ctx.cdev);
    unregister_chrdev_region(g_pilot_ctx.dev_num, 1);

    pr_info("process_pilot: Driver unloaded.\n");
}

module_init(pilot_driver_init);
module_exit(pilot_driver_exit);
