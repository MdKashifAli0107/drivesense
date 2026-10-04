#ifndef SENSORDATA_HPP
#define SENSORDATA_HPP

#include <cstdint>
#include <string>
#include "drivesense_ioctl.h"

class SensorData {
public:
    int32_t speed_kmh{0};
    int32_t fuel_pct{0};
    int32_t engine_temp_c{0};
    int32_t tyre_psi{0};
    uint32_t state{DS_STATE_IDLE};
    uint32_t active_fault{DS_FAULT_NONE};
    uint64_t sequence{0};
    uint64_t timestamp_ns{0};

    SensorData() = default;
    explicit SensorData(const struct ds_sensor_data& raw);

    std::string getStateString() const;
    std::string getFaultString() const;
    struct ds_sensor_data toRaw() const;
};

#endif // SENSORDATA_HPP
