#ifndef APP_HPP
#define APP_HPP

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>
#include "SensorDevice.hpp"
#include "SensorData.hpp"
#include "AlertManager.hpp"
#include "Logger.hpp"
#include "Dashboard.hpp"
#include "drivesense_ioctl.h"

class App {
public:
    explicit App(const std::string& device_path = "/dev/drivesense",
                 const std::string& log_path = "drivesense_alerts.log");
    ~App();

    int runInteractive();
    int runOnce();
    int runStart();
    int runStop();
    int runReset();
    int runInjectFault(const std::string& fault_name);
    int runStats();
    static void printHelp(const char* prog_name);

    static void handleSignal(int sig);

private:
    std::string device_path_;
    SensorDevice device_;
    AlertManager alert_mgr_;
    Logger logger_;
    Dashboard dashboard_;

    std::atomic<bool> running_{false};
    std::thread reader_thread_;
    std::mutex data_mutex_;

    SensorData current_data_;
    struct ds_stats current_stats_{0, 0, 0, 0, 0};
    std::deque<Alert> current_alerts_;

    void readerLoop();
    static std::atomic<bool> s_stop_requested;
};

#endif // APP_HPP
