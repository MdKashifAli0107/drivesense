#include "SensorDevice.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>

SensorDevice::SensorDevice(const std::string& device_path)
    : device_path_(device_path)
{
    fd_ = ::open(device_path_.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error("Device " + device_path_ + " not found or inaccessible. "
                                 "Please load the driver first: sudo ./scripts/load.sh");
    }
}

SensorDevice::~SensorDevice()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

SensorDevice::SensorDevice(SensorDevice&& other) noexcept
    : device_path_(std::move(other.device_path_))
    , fd_(other.fd_)
{
    other.fd_ = -1;
}

SensorDevice& SensorDevice::operator=(SensorDevice&& other) noexcept
{
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        device_path_ = std::move(other.device_path_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

SensorData SensorDevice::read()
{
    struct ds_sensor_data raw;
    ssize_t bytes = ::read(fd_, &raw, sizeof(raw));
    if (bytes < 0) {
        throw std::runtime_error("Failed to read from " + device_path_ + ": " + std::strerror(errno));
    }
    if (static_cast<size_t>(bytes) < sizeof(raw)) {
        throw std::runtime_error("Short read from " + device_path_);
    }
    return SensorData(raw);
}

void SensorDevice::start()
{
    if (::ioctl(fd_, DS_IOC_START) < 0) {
        throw std::runtime_error("ioctl DS_IOC_START failed: " + std::string(std::strerror(errno)));
    }
}

void SensorDevice::stop()
{
    if (::ioctl(fd_, DS_IOC_STOP) < 0) {
        throw std::runtime_error("ioctl DS_IOC_STOP failed: " + std::string(std::strerror(errno)));
    }
}

void SensorDevice::injectFault(uint32_t fault_id)
{
    if (::ioctl(fd_, DS_IOC_INJECT_FAULT, &fault_id) < 0) {
        throw std::runtime_error("ioctl DS_IOC_INJECT_FAULT failed: " + std::string(std::strerror(errno)));
    }
}

void SensorDevice::reset()
{
    if (::ioctl(fd_, DS_IOC_RESET) < 0) {
        throw std::runtime_error("ioctl DS_IOC_RESET failed: " + std::string(std::strerror(errno)));
    }
}

struct ds_stats SensorDevice::getStats()
{
    struct ds_stats stats;
    if (::ioctl(fd_, DS_IOC_GET_STATS, &stats) < 0) {
        throw std::runtime_error("ioctl DS_IOC_GET_STATS failed: " + std::string(std::strerror(errno)));
    }
    return stats;
}

SensorData SensorDevice::getDataViaIoctl()
{
    struct ds_sensor_data raw;
    if (::ioctl(fd_, DS_IOC_GET_DATA, &raw) < 0) {
        throw std::runtime_error("ioctl DS_IOC_GET_DATA failed: " + std::string(std::strerror(errno)));
    }
    return SensorData(raw);
}
