#include "AlertManager.hpp"

AlertManager::AlertManager(size_t max_history)
    : max_history_(max_history)
{
}

std::vector<Alert> AlertManager::evaluate(const SensorData& data)
{
    std::vector<Alert> new_events;
    auto now = std::chrono::system_clock::now();

    // 1. Speed Threshold Check (> 120 km/h)
    if (data.speed_kmh > SPEED_LIMIT_KMH) {
        if (!active_speed_) {
            active_speed_ = true;
            Alert a("SPEED", "WARNING",
                    "Speed limit exceeded: " + std::to_string(data.speed_kmh) + " km/h (Limit: " +
                        std::to_string(SPEED_LIMIT_KMH) + " km/h)",
                    now, true);
            new_events.push_back(a);
            pushAlert(a);
        }
    } else {
        if (active_speed_) {
            active_speed_ = false;
            Alert a("SPEED", "INFO",
                    "Speed normalized: " + std::to_string(data.speed_kmh) + " km/h",
                    now, false);
            new_events.push_back(a);
            pushAlert(a);
        }
    }

    // 2. Fuel Threshold Check (< 10%)
    if (data.fuel_pct < FUEL_WARN_PCT) {
        if (!active_fuel_) {
            active_fuel_ = true;
            Alert a("FUEL", "CRITICAL",
                    "Low fuel warning: " + std::to_string(data.fuel_pct) + "% remaining (Limit: " +
                        std::to_string(FUEL_WARN_PCT) + "%)",
                    now, true);
            new_events.push_back(a);
            pushAlert(a);
        }
    } else {
        if (active_fuel_) {
            active_fuel_ = false;
            Alert a("FUEL", "INFO",
                    "Fuel level normalized: " + std::to_string(data.fuel_pct) + "%",
                    now, false);
            new_events.push_back(a);
            pushAlert(a);
        }
    }

    // 3. Engine Temperature Check (> 105 deg C)
    if (data.engine_temp_c > TEMP_LIMIT_C) {
        if (!active_temp_) {
            active_temp_ = true;
            Alert a("ENGINE_TEMP", "CRITICAL",
                    "Engine overheating: " + std::to_string(data.engine_temp_c) + " C (Limit: " +
                        std::to_string(TEMP_LIMIT_C) + " C)",
                    now, true);
            new_events.push_back(a);
            pushAlert(a);
        }
    } else {
        if (active_temp_) {
            active_temp_ = false;
            Alert a("ENGINE_TEMP", "INFO",
                    "Engine temperature normalized: " + std::to_string(data.engine_temp_c) + " C",
                    now, false);
            new_events.push_back(a);
            pushAlert(a);
        }
    }

    // 4. Tyre Pressure Check (< 26 PSI)
    if (data.tyre_psi < TYRE_WARN_PSI) {
        if (!active_tyre_) {
            active_tyre_ = true;
            Alert a("TYRE_PRESSURE", "WARNING",
                    "Tyre pressure low: " + std::to_string(data.tyre_psi) + " PSI (Limit: " +
                        std::to_string(TYRE_WARN_PSI) + " PSI)",
                    now, true);
            new_events.push_back(a);
            pushAlert(a);
        }
    } else {
        if (active_tyre_) {
            active_tyre_ = false;
            Alert a("TYRE_PRESSURE", "INFO",
                    "Tyre pressure restored: " + std::to_string(data.tyre_psi) + " PSI",
                    now, false);
            new_events.push_back(a);
            pushAlert(a);
        }
    }

    return new_events;
}

void AlertManager::pushAlert(const Alert& alert)
{
    history_.push_back(alert);
    while (history_.size() > max_history_) {
        history_.pop_front();
    }
}

void AlertManager::clear()
{
    history_.clear();
    active_speed_ = false;
    active_fuel_ = false;
    active_temp_ = false;
    active_tyre_ = false;
}
