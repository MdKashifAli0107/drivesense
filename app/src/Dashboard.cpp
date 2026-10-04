#include "Dashboard.hpp"
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

Dashboard::Dashboard()
{
}

Dashboard::~Dashboard()
{
    cleanup();
}

void Dashboard::init()
{
    if (initialized_) return;

    ::initscr();
    ::cbreak();
    ::noecho();
    ::keypad(stdscr, TRUE);
    ::nodelay(stdscr, TRUE);
    ::curs_set(0);

    has_colors_ = (::has_colors() == TRUE);
    if (has_colors_) {
        ::start_color();
        ::use_default_colors();
        ::init_pair(1, COLOR_GREEN, -1);   // OK / Normal
        ::init_pair(2, COLOR_RED, -1);     // WARN / CRITICAL
        ::init_pair(3, COLOR_YELLOW, -1);  // WARNING / IDLE
        ::init_pair(4, COLOR_CYAN, -1);    // Header / Info
        ::init_pair(5, COLOR_MAGENTA, -1); // Fault / Special
    }

    initialized_ = true;
}

void Dashboard::cleanup()
{
    if (initialized_) {
        ::curs_set(1);
        ::endwin();
        initialized_ = false;
    }
}

int Dashboard::getKey()
{
    if (!initialized_) return ERR;
    return ::getch();
}

void Dashboard::drawGaugeBar(int y, const std::string& label, int32_t val, int32_t min_v,
                             int32_t max_v, const std::string& unit, bool is_warn,
                             const std::string& status_text)
{
    const int bar_width = 24;
    int range = max_v - min_v;
    if (range <= 0) range = 1;
    int filled = ((val - min_v) * bar_width) / range;
    if (filled < 0) filled = 0;
    if (filled > bar_width) filled = bar_width;

    std::string bar_str = "[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) bar_str += "=";
        else bar_str += " ";
    }
    bar_str += "]";

    // Print label
    ::mvprintw(y, 2, "%-16s", label.c_str());

    // Print bar in color
    int pair = is_warn ? 2 : 1;
    if (has_colors_) ::attron(COLOR_PAIR(pair) | A_BOLD);
    ::mvprintw(y, 19, "%s", bar_str.c_str());
    if (has_colors_) ::attroff(COLOR_PAIR(pair) | A_BOLD);

    // Print numerical reading and units
    char val_buf[32];
    std::snprintf(val_buf, sizeof(val_buf), "%4d %-4s", val, unit.c_str());
    ::mvprintw(y, 47, "%s", val_buf);

    // Print status badge [ OK ] or [ WARN ]
    if (has_colors_) ::attron(COLOR_PAIR(pair) | A_BOLD);
    ::mvprintw(y, 57, "[ %-4s ]", status_text.c_str());
    if (has_colors_) ::attroff(COLOR_PAIR(pair) | A_BOLD);
}

void Dashboard::draw(const SensorData& data, const std::deque<Alert>& alerts,
                     const struct ds_stats& stats)
{
    if (!initialized_) return;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    ::erase();

    // Check window dimensions
    if (rows < MIN_ROWS || cols < MIN_COLS) {
        if (has_colors_) ::attron(COLOR_PAIR(2) | A_BOLD);
        ::mvprintw(rows / 2, (cols - 46) > 0 ? (cols - 46) / 2 : 0,
                   "Terminal too small! Resize to at least %dx%d.", MIN_COLS, MIN_ROWS);
        ::mvprintw(rows / 2 + 1, (cols - 30) > 0 ? (cols - 30) / 2 : 0,
                   "Current size: %d rows x %d cols", rows, cols);
        if (has_colors_) ::attroff(COLOR_PAIR(2) | A_BOLD);
        ::refresh();
        return;
    }

    // Top Title Box
    if (has_colors_) ::attron(COLOR_PAIR(4) | A_BOLD);
    ::mvprintw(1, 2, "==================================================================");
    ::mvprintw(2, 2, "       DRIVESENSE ? VIRTUAL VEHICLE TELEMETRY DASHBOARD           ");
    ::mvprintw(3, 2, "==================================================================");
    if (has_colors_) ::attroff(COLOR_PAIR(4) | A_BOLD);

    // Status Line
    std::string state_str = data.getStateString();
    std::string fault_str = data.getFaultString();

    ::mvprintw(5, 2, "State: ");
    int state_pair = (data.state == DS_STATE_FAULT) ? 5 : ((data.state == DS_STATE_DRIVING) ? 1 : 3);
    if (has_colors_) ::attron(COLOR_PAIR(state_pair) | A_BOLD);
    ::printw("[%-8s]", state_str.c_str());
    if (has_colors_) ::attroff(COLOR_PAIR(state_pair) | A_BOLD);

    ::printw("   Fault: ");
    int fault_pair = (data.active_fault != DS_FAULT_NONE) ? 2 : 1;
    if (has_colors_) ::attron(COLOR_PAIR(fault_pair) | A_BOLD);
    ::printw("[%-10s]", fault_str.c_str());
    if (has_colors_) ::attroff(COLOR_PAIR(fault_pair) | A_BOLD);

    ::printw("   Seq: %-6lu", data.sequence);

    // Sensor Gauges
    bool speed_warn = (data.speed_kmh > 120);
    bool fuel_warn  = (data.fuel_pct < 10);
    bool temp_warn  = (data.engine_temp_c > 105);
    bool tyre_warn  = (data.tyre_psi < 26);

    drawGaugeBar(7,  "Vehicle Speed", data.speed_kmh,     0, 200, "km/h", speed_warn, speed_warn ? "WARN" : "OK");
    drawGaugeBar(8,  "Fuel Level",    data.fuel_pct,      0, 100, "%",    fuel_warn,  fuel_warn ? "LOW" : "OK");
    drawGaugeBar(9,  "Engine Temp",   data.engine_temp_c, 20, 130, "C",   temp_warn,  temp_warn ? "HOT" : "OK");
    drawGaugeBar(10, "Tyre Pressure", data.tyre_psi,      0,  40, "PSI",  tyre_warn,  tyre_warn ? "WARN" : "OK");

    // Divider
    ::mvprintw(12, 2, "------------------------------------------------------------------");
    ::mvprintw(12, 4, "[ ACTIVE & RECENT ALERTS ]");

    // Recent Alerts (display up to 4 latest)
    int alert_row = 13;
    if (alerts.empty()) {
        if (has_colors_) ::attron(COLOR_PAIR(1));
        ::mvprintw(alert_row++, 4, "* No active alerts. All vehicular parameters within normal limits.");
        if (has_colors_) ::attroff(COLOR_PAIR(1));
    } else {
        int count = 0;
        for (auto it = alerts.rbegin(); it != alerts.rend() && count < 4; ++it, ++count) {
            int a_pair = (it->severity == "CRITICAL" || it->severity == "WARNING") ? 2 : 1;
            if (has_colors_) ::attron(COLOR_PAIR(a_pair));
            ::mvprintw(alert_row++, 4, "* [%-8s] %-54s", it->severity.c_str(), it->message.substr(0, 54).c_str());
            if (has_colors_) ::attroff(COLOR_PAIR(a_pair));
        }
    }

    // Driver statistics
    ::mvprintw(17, 2, "------------------------------------------------------------------");
    ::mvprintw(18, 2, "Stats: Reads: %llu | IOCTLs: %llu | Updates: %llu | Faults: %llu",
               stats.reads, stats.ioctls, stats.updates, stats.faults_injected);

    // Keyboard help footer
    if (has_colors_) ::attron(COLOR_PAIR(4));
    ::mvprintw(20, 2, "Controls:");
    if (has_colors_) ::attroff(COLOR_PAIR(4));
    ::mvprintw(21, 2, "[S] Start  [P] Stop  [R] Reset  [Q] Quit");
    ::mvprintw(22, 2, "Faults: [1] Overheat  [2] Low Fuel  [3] Flat Tyre  [4] Overspeed");

    ::refresh();
}
