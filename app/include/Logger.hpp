#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include "Alert.hpp"

class Logger {
public:
    explicit Logger(const std::string& log_file_path = "drivesense_alerts.log");
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(const Alert& alert);
    void logMessage(const std::string& severity, const std::string& tag, const std::string& msg);
    void flush();

    const std::string& getFilePath() const { return file_path_; }

private:
    std::string file_path_;
    std::ofstream out_file_;
    std::mutex log_mutex_;

    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
};

#endif // LOGGER_HPP
