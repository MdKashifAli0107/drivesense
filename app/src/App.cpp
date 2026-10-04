#include "App.hpp"
#include <iostream>
#include <chrono>
#include <csignal>
#include <cctype>

std::atomic<bool> App::s_stop_requested{false};

void App::handleSignal(int sig)
{
    (void)sig;
    s_stop_requested = true;
}

App::App(const std::string& device_path, const std::string& log_path)
    : device_path_(device_path)
    , device_(device_path)
    , alert_mgr_(20)
    , logger_(log_path)
{
}

App::~App()
{
    if (running_.load()) {
        running_.store(false);
        if (reader_thread_.joinable()) {
            reader_thread_.join();
        }
    }
}

void App::readerLoop()
{
    while (running_.load() && !s_stop_requested.load()) {
        try {
            SensorData data = device_.read();
            struct ds_stats stats = device_.getStats();

            std::vector<Alert> new_alerts = alert_mgr_.evaluate(data);
            for (const auto& a : new_alerts) {
                logger_.log(a);
            }

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                current_data_ = data;
                current_stats_ = stats;
                current_alerts_ = alert_mgr_.getHistory();
            }
        } catch (const std::exception& e) {
            logger_.logMessage("ERROR", "READER", e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

int App::runInteractive()
{
    s_stop_requested = false;
    std::signal(SIGINT, App::handleSignal);
    std::signal(SIGTERM, App::handleSignal);

    dashboard_.init();
    running_.store(true);
    reader_thread_ = std::thread(&App::readerLoop, this);

    while (running_.load() && !s_stop_requested.load()) {
        SensorData local_data;
        struct ds_stats local_stats;
        std::deque<Alert> local_alerts;

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            local_data = current_data_;
            local_stats = current_stats_;
            local_alerts = current_alerts_;
        }

        dashboard_.draw(local_data, local_alerts, local_stats);

        int ch = dashboard_.getKey();
        if (ch != ERR) {
            ch = std::toupper(ch);
            try {
                switch (ch) {
                case 'S':
                    device_.start();
                    logger_.logMessage("INFO", "COMMAND", "User issued START command");
                    break;
                case 'P':
                    device_.stop();
                    logger_.logMessage("INFO", "COMMAND", "User issued STOP command");
                    break;
                case 'R':
                    device_.reset();
                    alert_mgr_.clear();
                    logger_.logMessage("INFO", "COMMAND", "User issued RESET command");
                    break;
                case '1':
                    device_.injectFault(DS_FAULT_OVERHEAT);
                    logger_.logMessage("WARNING", "COMMAND", "Injected fault: OVERHEAT");
                    break;
                case '2':
                    device_.injectFault(DS_FAULT_LOW_FUEL);
                    logger_.logMessage("WARNING", "COMMAND", "Injected fault: LOW_FUEL");
                    break;
                case '3':
                    device_.injectFault(DS_FAULT_FLAT_TYRE);
                    logger_.logMessage("WARNING", "COMMAND", "Injected fault: FLAT_TYRE");
                    break;
                case '4':
                    device_.injectFault(DS_FAULT_OVERSPEED);
                    logger_.logMessage("WARNING", "COMMAND", "Injected fault: OVERSPEED");
                    break;
                case 'Q':
                    running_.store(false);
                    break;
                default:
                    break;
                }
            } catch (const std::exception& e) {
                logger_.logMessage("ERROR", "IOCTL", e.what());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    running_.store(false);
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
    dashboard_.cleanup();
    logger_.flush();

    return 0;
}

int App::runOnce()
{
    SensorData data = device_.read();
    struct ds_stats stats = device_.getStats();

    std::cout << "=== DriveSense Telemetry Snapshot ===" << std::endl;
    std::cout << "State:            " << data.getStateString() << std::endl;
    std::cout << "Active Fault:     " << data.getFaultString() << std::endl;
    std::cout << "Speed:            " << data.speed_kmh << " km/h" << std::endl;
    std::cout << "Fuel Level:       " << data.fuel_pct << " %" << std::endl;
    std::cout << "Engine Temp:      " << data.engine_temp_c << " C" << std::endl;
    std::cout << "Tyre Pressure:    " << data.tyre_psi << " PSI" << std::endl;
    std::cout << "Sequence:         " << data.sequence << std::endl;
    std::cout << "Timestamp:        " << data.timestamp_ns << " ns" << std::endl;
    std::cout << "=== Operational Statistics ===" << std::endl;
    std::cout << "Reads:            " << stats.reads << std::endl;
    std::cout << "IOCTLs:           " << stats.ioctls << std::endl;
    std::cout << "Updates:          " << stats.updates << std::endl;
    std::cout << "Faults Injected:  " << stats.faults_injected << std::endl;
    std::cout << "Open Clients:     " << stats.open_count << std::endl;

    return 0;
}

int App::runStart()
{
    device_.start();
    std::cout << "DriveSense simulation started (DRIVING mode)." << std::endl;
    return 0;
}

int App::runStop()
{
    device_.stop();
    std::cout << "DriveSense simulation stopped (IDLE mode)." << std::endl;
    return 0;
}

int App::runReset()
{
    device_.reset();
    std::cout << "DriveSense telemetry reset to default baseline." << std::endl;
    return 0;
}

int App::runInjectFault(const std::string& fault_name)
{
    uint32_t fid = 0;
    if (fault_name == "overheat") fid = DS_FAULT_OVERHEAT;
    else if (fault_name == "lowfuel") fid = DS_FAULT_LOW_FUEL;
    else if (fault_name == "flattyre") fid = DS_FAULT_FLAT_TYRE;
    else if (fault_name == "overspeed") fid = DS_FAULT_OVERSPEED;
    else {
        std::cerr << "Error: Unknown fault '" << fault_name << "'." << std::endl;
        std::cerr << "Supported faults: overheat, lowfuel, flattyre, overspeed" << std::endl;
        return 1;
    }

    device_.injectFault(fid);
    std::cout << "Fault injected successfully: " << fault_name << " (ID: " << fid << ")." << std::endl;
    return 0;
}

int App::runStats()
{
    struct ds_stats stats = device_.getStats();
    std::cout << "=== DriveSense Kernel Driver Stats ===" << std::endl;
    std::cout << "Reads:            " << stats.reads << std::endl;
    std::cout << "IOCTLs:           " << stats.ioctls << std::endl;
    std::cout << "Updates:          " << stats.updates << std::endl;
    std::cout << "Faults Injected:  " << stats.faults_injected << std::endl;
    std::cout << "Open Clients:     " << stats.open_count << std::endl;
    return 0;
}

void App::printHelp(const char* prog_name)
{
    std::cout << "Usage: " << prog_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  (no args)             Launch interactive ncurses dashboard\n"
              << "  --once                Print single telemetry reading as text and exit\n"
              << "  --start               Start driving simulation\n"
              << "  --stop                Stop driving simulation (set to IDLE)\n"
              << "  --reset               Reset telemetry to default baseline\n"
              << "  --inject <fault>      Inject fault (overheat | lowfuel | flattyre | overspeed)\n"
              << "  --stats               Print kernel driver stats and exit\n"
              << "  --help                Display this help menu and exit\n";
}
