#ifndef SENSORDEVICE_HPP
#define SENSORDEVICE_HPP

#include <string>
#include <stdexcept>
#include "SensorData.hpp"
#include "drivesense_ioctl.h"

class SensorDevice {
public:
    explicit SensorDevice(const std::string& device_path = "/dev/drivesense");
    ~SensorDevice();

    SensorDevice(const SensorDevice&) = delete;
    SensorDevice& operator=(const SensorDevice&) = delete;

    SensorDevice(SensorDevice&& other) noexcept;
    SensorDevice& operator=(SensorDevice&& other) noexcept;

    SensorData read();
    void start();
    void stop();
    void injectFault(uint32_t fault_id);
    void reset();
    struct ds_stats getStats();
    SensorData getDataViaIoctl();

    int getFd() const { return fd_; }
    const std::string& getPath() const { return device_path_; }

private:
    std::string device_path_;
    int fd_{-1};
};

#endif // SENSORDEVICE_HPP
