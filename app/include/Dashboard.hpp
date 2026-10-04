#ifndef DASHBOARD_HPP
#define DASHBOARD_HPP

#include <deque>
#include <string>
#include <curses.h>
#include "SensorData.hpp"
#include "Alert.hpp"
#include "drivesense_ioctl.h"

class Dashboard {
public:
    static constexpr int MIN_ROWS = 22;
    static constexpr int MIN_COLS = 68;

    Dashboard();
    ~Dashboard();

    void init();
    void cleanup();
    void draw(const SensorData& data, const std::deque<Alert>& alerts, const struct ds_stats& stats);
    int getKey();

private:
    bool initialized_{false};
    bool has_colors_{false};

    void drawGaugeBar(int y, const std::string& label, int32_t val, int32_t min_v,
                      int32_t max_v, const std::string& unit, bool is_warn, const std::string& status_text);
};

#endif // DASHBOARD_HPP
