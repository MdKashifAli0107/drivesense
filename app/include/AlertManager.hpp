#ifndef ALERTMANAGER_HPP
#define ALERTMANAGER_HPP

#include <deque>
#include <vector>
#include <string>
#include "Alert.hpp"
#include "SensorData.hpp"

class AlertManager {
public:
    static constexpr int32_t SPEED_LIMIT_KMH = 120;
    static constexpr int32_t FUEL_WARN_PCT = 10;
    static constexpr int32_t TEMP_LIMIT_C = 105;
    static constexpr int32_t TYRE_WARN_PSI = 26;

    explicit AlertManager(size_t max_history = 20);

    std::vector<Alert> evaluate(const SensorData& data);

    const std::deque<Alert>& getHistory() const { return history_; }
    bool isSpeedAlertActive() const { return active_speed_; }
    bool isFuelAlertActive() const { return active_fuel_; }
    bool isTempAlertActive() const { return active_temp_; }
    bool isTyreAlertActive() const { return active_tyre_; }
    void clear();

private:
    size_t max_history_;
    std::deque<Alert> history_;

    bool active_speed_{false};
    bool active_fuel_{false};
    bool active_temp_{false};
    bool active_tyre_{false};

    void pushAlert(const Alert& alert);
};

#endif // ALERTMANAGER_HPP
