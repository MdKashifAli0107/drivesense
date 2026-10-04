#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#include "drivesense_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DriveSense Team");
MODULE_DESCRIPTION("Virtual Car Sensor Character Driver with Softirq Simulation");
MODULE_VERSION("1.0");

/* Device representation */
static dev_t ds_dev_num;
static struct cdev ds_cdev;
static struct class *ds_class = NULL;
static struct device *ds_device = NULL;
static struct proc_dir_entry *ds_proc_entry = NULL;

/* Simulation timer and lock */
static struct timer_list ds_timer;
static spinlock_t ds_lock;

/* Shared state protected by ds_lock */
static struct ds_sensor_data ds_data;
static struct ds_stats ds_stats_data;
static u32 fuel_step_counter = 0;

/* Helper to clamp signed integers */
static inline s32 clamp_s32(s32 val, s32 min_val, s32 max_val)
{
    if (val < min_val)
        return min_val;
    if (val > max_val)
        return max_val;
    return val;
}

/* Timer simulation callback (Softirq context) */
static void ds_timer_callback(struct timer_list *t)
{
    (void)t;
    spin_lock(&ds_lock);

    /* 1. Update based on state */
    switch (ds_data.state) {
    case DS_STATE_IDLE:
        /* Speed decays smoothly toward 0 */
        if (ds_data.speed_kmh > 0)
            ds_data.speed_kmh -= 3;
        if (ds_data.speed_kmh < 0)
            ds_data.speed_kmh = 0;

        /* Engine temperature drifts toward ambient/idle 70 deg C */
        if (ds_data.engine_temp_c > 70)
            ds_data.engine_temp_c -= 1;
        else if (ds_data.engine_temp_c < 70)
            ds_data.engine_temp_c += 1;

        /* Tyre pressure returns to normal baseline 32 PSI */
        if (ds_data.tyre_psi < 32)
            ds_data.tyre_psi += 1;
        else if (ds_data.tyre_psi > 32)
            ds_data.tyre_psi -= 1;
        break;

    case DS_STATE_DRIVING: {
        /* Speed drifts randomly in range 40..110 km/h */
        s32 delta_spd = (s32)(get_random_u32() % 7) - 3; /* -3 to +3 */
        ds_data.speed_kmh += delta_spd;
        if (ds_data.speed_kmh < 40)
            ds_data.speed_kmh = 42;
        else if (ds_data.speed_kmh > 110)
            ds_data.speed_kmh = 108;

        /* Fuel slowly drops (~1% every 10 updates) */
        fuel_step_counter++;
        if (fuel_step_counter >= 10) {
            fuel_step_counter = 0;
            if (ds_data.fuel_pct > 0)
                ds_data.fuel_pct -= 1;
        }

        /* Temp tracks speed: moves toward 85..98 deg C */
        {
            s32 target_temp = 85 + (ds_data.speed_kmh - 40) * 13 / 70;
            if (ds_data.engine_temp_c < target_temp)
                ds_data.engine_temp_c += 1;
            else if (ds_data.engine_temp_c > target_temp)
                ds_data.engine_temp_c -= 1;
        }

        /* Tyre pressure fluctuates around 31..33 PSI */
        ds_data.tyre_psi = 32 + ((s32)(get_random_u32() % 3) - 1);
        break;
    }

    case DS_STATE_FAULT: {
        /* Driving dynamics continue */
        s32 delta_spd = (s32)(get_random_u32() % 7) - 3;
        ds_data.speed_kmh += delta_spd;
        if (ds_data.speed_kmh < 30)
            ds_data.speed_kmh = 35;

        fuel_step_counter++;
        if (fuel_step_counter >= 10) {
            fuel_step_counter = 0;
            if (ds_data.fuel_pct > 0)
                ds_data.fuel_pct -= 1;
        }

        /* Specific fault symptoms enforced */
        switch (ds_data.active_fault) {
        case DS_FAULT_OVERHEAT:
            /* Temp climbs toward 118 deg C */
            if (ds_data.engine_temp_c < 118)
                ds_data.engine_temp_c += 2;
            break;

        case DS_FAULT_LOW_FUEL:
            /* Fuel level forced to critical low <= 8% */
            if (ds_data.fuel_pct > 8)
                ds_data.fuel_pct = 8;
            break;

        case DS_FAULT_FLAT_TYRE:
            /* Pressure deflates toward 18 PSI */
            if (ds_data.tyre_psi > 18)
                ds_data.tyre_psi -= 2;
            break;

        case DS_FAULT_OVERSPEED:
            /* Speed accelerates toward 140 km/h */
            if (ds_data.speed_kmh < 135)
                ds_data.speed_kmh += 4;
            else if (ds_data.speed_kmh > 145)
                ds_data.speed_kmh -= 2;
            break;

        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    /* 2. Clamp all channels strictly within valid physical bounds */
    ds_data.speed_kmh = clamp_s32(ds_data.speed_kmh, 0, 200);
    ds_data.fuel_pct = clamp_s32(ds_data.fuel_pct, 0, 100);
    ds_data.engine_temp_c = clamp_s32(ds_data.engine_temp_c, 20, 130);
    ds_data.tyre_psi = clamp_s32(ds_data.tyre_psi, 0, 40);

    /* 3. Update sequence and telemetry timestamp */
    ds_data.sequence++;
    ds_data.timestamp_ns = ktime_get_ns();
    ds_stats_data.updates++;

    spin_unlock(&ds_lock);

    /* Reschedule timer for next tick (500 ms period) */
    mod_timer(&ds_timer, jiffies + msecs_to_jiffies(500));
}

/* File operations: open */
static int ds_open(struct inode *inodep, struct file *filep)
{
    (void)inodep;
    (void)filep;
    spin_lock_bh(&ds_lock);
    ds_stats_data.open_count++;
    spin_unlock_bh(&ds_lock);

    pr_info("drivesense: device opened (open_count=%u)\n", ds_stats_data.open_count);
    return 0;
}

/* File operations: release */
static int ds_release(struct inode *inodep, struct file *filep)
{
    (void)inodep;
    (void)filep;
    spin_lock_bh(&ds_lock);
    if (ds_stats_data.open_count > 0)
        ds_stats_data.open_count--;
    spin_unlock_bh(&ds_lock);

    pr_info("drivesense: device closed (open_count=%u)\n", ds_stats_data.open_count);
    return 0;
}

/* File operations: read */
static ssize_t ds_read(struct file *filep, char __user *buf, size_t count, loff_t *offset)
{
    struct ds_sensor_data local_data;
    (void)filep;
    (void)offset;

    if (count < sizeof(struct ds_sensor_data))
        return -EINVAL;

    spin_lock_bh(&ds_lock);
    local_data = ds_data;
    ds_stats_data.reads++;
    spin_unlock_bh(&ds_lock);

    if (copy_to_user(buf, &local_data, sizeof(local_data)))
        return -EFAULT;

    return sizeof(struct ds_sensor_data);
}

/* File operations: unlocked_ioctl */
static long ds_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    (void)filep;

    switch (cmd) {
    case DS_IOC_START:
        spin_lock_bh(&ds_lock);
        if (ds_data.state != DS_STATE_FAULT)
            ds_data.state = DS_STATE_DRIVING;
        ds_stats_data.ioctls++;
        spin_unlock_bh(&ds_lock);
        pr_info("drivesense: simulation started (DRIVING)\n");
        return 0;

    case DS_IOC_STOP:
        spin_lock_bh(&ds_lock);
        if (ds_data.state != DS_STATE_FAULT)
            ds_data.state = DS_STATE_IDLE;
        ds_stats_data.ioctls++;
        spin_unlock_bh(&ds_lock);
        pr_info("drivesense: simulation stopped (IDLE)\n");
        return 0;

    case DS_IOC_INJECT_FAULT: {
        __u32 fault_id = 0;
        if (copy_from_user(&fault_id, (void __user *)arg, sizeof(fault_id)))
            return -EFAULT;

        if (fault_id < DS_FAULT_OVERHEAT || fault_id > DS_FAULT_OVERSPEED)
            return -EINVAL;

        spin_lock_bh(&ds_lock);
        ds_data.state = DS_STATE_FAULT;
        ds_data.active_fault = fault_id;
        ds_stats_data.faults_injected++;
        ds_stats_data.ioctls++;
        spin_unlock_bh(&ds_lock);

        pr_info("drivesense: fault %u injected\n", fault_id);
        return 0;
    }

    case DS_IOC_RESET:
        spin_lock_bh(&ds_lock);
        ds_data.speed_kmh = 0;
        ds_data.fuel_pct = 80;
        ds_data.engine_temp_c = 70;
        ds_data.tyre_psi = 32;
        ds_data.state = DS_STATE_IDLE;
        ds_data.active_fault = DS_FAULT_NONE;
        ds_stats_data.ioctls++;
        spin_unlock_bh(&ds_lock);
        pr_info("drivesense: state reset to default baseline\n");
        return 0;

    case DS_IOC_GET_STATS: {
        struct ds_stats local_stats;
        spin_lock_bh(&ds_lock);
        ds_stats_data.ioctls++;
        local_stats = ds_stats_data;
        spin_unlock_bh(&ds_lock);

        if (copy_to_user((void __user *)arg, &local_stats, sizeof(local_stats)))
            return -EFAULT;
        return 0;
    }

    case DS_IOC_GET_DATA: {
        struct ds_sensor_data local_data;
        spin_lock_bh(&ds_lock);
        ds_stats_data.ioctls++;
        local_data = ds_data;
        spin_unlock_bh(&ds_lock);

        if (copy_to_user((void __user *)arg, &local_data, sizeof(local_data)))
            return -EFAULT;
        return 0;
    }

    default:
        return -ENOTTY;
    }
}

static const struct file_operations ds_fops = {
    .owner          = THIS_MODULE,
    .open           = ds_open,
    .release        = ds_release,
    .read           = ds_read,
    .unlocked_ioctl = ds_ioctl,
};

/* Procfs seq_file implementation */
static int ds_proc_show(struct seq_file *m, void *v)
{
    struct ds_sensor_data data;
    struct ds_stats stats;
    const char *state_str = "UNKNOWN";
    const char *fault_str = "NONE";
    (void)v;

    spin_lock_bh(&ds_lock);
    data = ds_data;
    stats = ds_stats_data;
    spin_unlock_bh(&ds_lock);

    switch (data.state) {
    case DS_STATE_IDLE:
        state_str = "IDLE";
        break;
    case DS_STATE_DRIVING:
        state_str = "DRIVING";
        break;
    case DS_STATE_FAULT:
        state_str = "FAULT";
        break;
    default:
        break;
    }

    switch (data.active_fault) {
    case DS_FAULT_NONE:
        fault_str = "NONE";
        break;
    case DS_FAULT_OVERHEAT:
        fault_str = "OVERHEAT";
        break;
    case DS_FAULT_LOW_FUEL:
        fault_str = "LOW_FUEL";
        break;
    case DS_FAULT_FLAT_TYRE:
        fault_str = "FLAT_TYRE";
        break;
    case DS_FAULT_OVERSPEED:
        fault_str = "OVERSPEED";
        break;
    default:
        break;
    }

    seq_printf(m, "=== DriveSense Virtual Vehicle Telemetry ===\n");
    seq_printf(m, "State:            %s\n", state_str);
    seq_printf(m, "Active Fault:     %s\n", fault_str);
    seq_printf(m, "Speed:            %d km/h\n", data.speed_kmh);
    seq_printf(m, "Fuel:             %d %%\n", data.fuel_pct);
    seq_printf(m, "Engine Temp:      %d deg C\n", data.engine_temp_c);
    seq_printf(m, "Tyre Pressure:    %d PSI\n", data.tyre_psi);
    seq_printf(m, "Sequence:         %llu\n", data.sequence);
    seq_printf(m, "Timestamp:        %llu ns\n", data.timestamp_ns);
    seq_printf(m, "=== Operational Statistics ===\n");
    seq_printf(m, "Reads:            %llu\n", stats.reads);
    seq_printf(m, "IOCTLs:           %llu\n", stats.ioctls);
    seq_printf(m, "Updates:          %llu\n", stats.updates);
    seq_printf(m, "Faults Injected:  %llu\n", stats.faults_injected);
    seq_printf(m, "Open Clients:     %u\n", stats.open_count);

    return 0;
}

static int ds_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, ds_proc_show, NULL);
}

static const struct proc_ops ds_proc_ops = {
    .proc_open    = ds_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* Module Initialization */
static int __init ds_init(void)
{
    int ret = 0;

    pr_info("drivesense: initializing virtual car sensor driver\n");

    /* Initialize synchronization and baseline telemetry */
    spin_lock_init(&ds_lock);

    ds_data.speed_kmh = 0;
    ds_data.fuel_pct = 80;
    ds_data.engine_temp_c = 70;
    ds_data.tyre_psi = 32;
    ds_data.state = DS_STATE_IDLE;
    ds_data.active_fault = DS_FAULT_NONE;
    ds_data.sequence = 0;
    ds_data.timestamp_ns = ktime_get_ns();

    memset(&ds_stats_data, 0, sizeof(ds_stats_data));

    /* 1. Allocate character device major/minor numbers */
    ret = alloc_chrdev_region(&ds_dev_num, 0, 1, DS_DEVICE_NAME);
    if (ret < 0) {
        pr_err("drivesense: failed to allocate chrdev region: %d\n", ret);
        return ret;
    }

    /* 2. Initialize and register cdev */
    cdev_init(&ds_cdev, &ds_fops);
    ds_cdev.owner = THIS_MODULE;
    ret = cdev_add(&ds_cdev, ds_dev_num, 1);
    if (ret < 0) {
        pr_err("drivesense: failed to add cdev: %d\n", ret);
        goto err_unregister_chrdev;
    }

    /* 3. Create sysfs device class */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    ds_class = class_create(DS_DEVICE_NAME);
#else
    ds_class = class_create(THIS_MODULE, DS_DEVICE_NAME);
#endif
    if (IS_ERR(ds_class)) {
        ret = PTR_ERR(ds_class);
        pr_err("drivesense: failed to create device class: %d\n", ret);
        goto err_cdev_del;
    }

    /* 4. Create device node /dev/drivesense */
    ds_device = device_create(ds_class, NULL, ds_dev_num, NULL, DS_DEVICE_NAME);
    if (IS_ERR(ds_device)) {
        ret = PTR_ERR(ds_device);
        pr_err("drivesense: failed to create device /dev/%s: %d\n", DS_DEVICE_NAME, ret);
        goto err_class_destroy;
    }

    /* 5. Create procfs diagnostic entry */
    ds_proc_entry = proc_create(DS_DEVICE_NAME, 0444, NULL, &ds_proc_ops);
    if (!ds_proc_entry) {
        pr_warn("drivesense: warning, failed to create /proc/%s entry\n", DS_DEVICE_NAME);
    }

    /* 6. Setup and arm softirq periodic simulation timer (500 ms) */
    timer_setup(&ds_timer, ds_timer_callback, 0);
    ret = mod_timer(&ds_timer, jiffies + msecs_to_jiffies(500));
    if (ret < 0) {
        pr_err("drivesense: failed to arm simulation timer\n");
        goto err_remove_proc;
    }

    pr_info("drivesense: loaded successfully (Major: %d, Minor: %d)\n",
            MAJOR(ds_dev_num), MINOR(ds_dev_num));
    return 0;

err_remove_proc:
    if (ds_proc_entry)
        remove_proc_entry(DS_DEVICE_NAME, NULL);
    device_destroy(ds_class, ds_dev_num);
err_class_destroy:
    class_destroy(ds_class);
err_cdev_del:
    cdev_del(&ds_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(ds_dev_num, 1);
    return ret;
}

/* Module Cleanup */
static void __exit ds_exit(void)
{
    pr_info("drivesense: unloading virtual car sensor driver\n");

    /* Stop periodic simulation timer synchronously */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
    timer_delete_sync(&ds_timer);
#else
    del_timer_sync(&ds_timer);
#endif

    /* Remove procfs entry */
    if (ds_proc_entry) {
        remove_proc_entry(DS_DEVICE_NAME, NULL);
        ds_proc_entry = NULL;
    }

    /* Destroy device and class */
    if (ds_device) {
        device_destroy(ds_class, ds_dev_num);
        ds_device = NULL;
    }
    if (ds_class) {
        class_destroy(ds_class);
        ds_class = NULL;
    }

    /* Delete cdev and free device numbers */
    cdev_del(&ds_cdev);
    unregister_chrdev_region(ds_dev_num, 1);

    pr_info("drivesense: driver unloaded cleanly\n");
}

module_init(ds_init);
module_exit(ds_exit);
