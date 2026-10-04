#ifndef ALERT_HPP
#define ALERT_HPP

#include <string>
#include <chrono>

struct Alert {
    std::string sensor_name;
    std::string severity;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    bool active{true};

    Alert(std::string sensor, std::string sev, std::string msg,
          std::chrono::system_clock::time_point ts = std::chrono::system_clock::now(),
          bool is_active = true)
        : sensor_name(std::move(sensor))
        , severity(std::move(sev))
        , message(std::move(msg))
        , timestamp(ts)
        , active(is_active)
    {
    }
};

#endif // ALERT_HPP
