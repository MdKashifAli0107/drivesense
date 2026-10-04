#ifndef DRIVESENSE_IOCTL_H
#define DRIVESENSE_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <linux/types.h>
#include <sys/ioctl.h>
#endif

#define DS_DEVICE_NAME "drivesense"
#define DS_MAGIC 'D'

enum ds_state {
    DS_STATE_IDLE = 0,
    DS_STATE_DRIVING = 1,
    DS_STATE_FAULT = 2
};

enum ds_fault {
    DS_FAULT_NONE = 0,
    DS_FAULT_OVERHEAT = 1,
    DS_FAULT_LOW_FUEL = 2,
    DS_FAULT_FLAT_TYRE = 3,
    DS_FAULT_OVERSPEED = 4
};

struct ds_sensor_data {
    __s32 speed_kmh;       /* 0..200 */
    __s32 fuel_pct;        /* 0..100 */
    __s32 engine_temp_c;   /* 20..130 */
    __s32 tyre_psi;        /* 0..40 */
    __u32 state;           /* enum ds_state */
    __u32 active_fault;    /* enum ds_fault */
    __u64 sequence;        /* increases on every timer update */
    __u64 timestamp_ns;    /* ktime_get_ns() at last update */
};

struct ds_stats {
    __u64 reads;
    __u64 ioctls;
    __u64 updates;
    __u64 faults_injected;
    __u32 open_count;
};

#define DS_IOC_START        _IO(DS_MAGIC, 1)
#define DS_IOC_STOP         _IO(DS_MAGIC, 2)
#define DS_IOC_INJECT_FAULT _IOW(DS_MAGIC, 3, __u32)
#define DS_IOC_RESET        _IO(DS_MAGIC, 4)
#define DS_IOC_GET_STATS    _IOR(DS_MAGIC, 5, struct ds_stats)
#define DS_IOC_GET_DATA     _IOR(DS_MAGIC, 6, struct ds_sensor_data)

#endif /* DRIVESENSE_IOCTL_H */
