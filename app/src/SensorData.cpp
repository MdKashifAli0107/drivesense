#include "SensorData.hpp"

SensorData::SensorData(const struct ds_sensor_data& raw)
    : speed_kmh(raw.speed_kmh)
    , fuel_pct(raw.fuel_pct)
    , engine_temp_c(raw.engine_temp_c)
    , tyre_psi(raw.tyre_psi)
    , state(raw.state)
    , active_fault(raw.active_fault)
    , sequence(raw.sequence)
    , timestamp_ns(raw.timestamp_ns)
{
}

std::string SensorData::getStateString() const
{
    switch (state) {
    case DS_STATE_IDLE:
        return "IDLE";
    case DS_STATE_DRIVING:
        return "DRIVING";
    case DS_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

std::string SensorData::getFaultString() const
{
    switch (active_fault) {
    case DS_FAULT_NONE:
        return "NONE";
    case DS_FAULT_OVERHEAT:
        return "OVERHEAT";
    case DS_FAULT_LOW_FUEL:
        return "LOW_FUEL";
    case DS_FAULT_FLAT_TYRE:
        return "FLAT_TYRE";
    case DS_FAULT_OVERSPEED:
        return "OVERSPEED";
    default:
        return "UNKNOWN";
    }
}

struct ds_sensor_data SensorData::toRaw() const
{
    struct ds_sensor_data raw;
    raw.speed_kmh = speed_kmh;
    raw.fuel_pct = fuel_pct;
    raw.engine_temp_c = engine_temp_c;
    raw.tyre_psi = tyre_psi;
    raw.state = state;
    raw.active_fault = active_fault;
    raw.sequence = sequence;
    raw.timestamp_ns = timestamp_ns;
    return raw;
}
