#include "Logger.hpp"
#include <iomanip>
#include <sstream>
#include <ctime>

Logger::Logger(const std::string& log_file_path)
    : file_path_(log_file_path)
{
    out_file_.open(file_path_, std::ios::out | std::ios::app);
}

Logger::~Logger()
{
    flush();
    if (out_file_.is_open()) {
        out_file_.close();
    }
}

std::string Logger::formatTimestamp(const std::chrono::system_clock::time_point& tp)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf;
    localtime_r(&t, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::log(const Alert& alert)
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (!out_file_.is_open()) {
        out_file_.open(file_path_, std::ios::out | std::ios::app);
    }
    if (out_file_.is_open()) {
        out_file_ << "[" << formatTimestamp(alert.timestamp) << "] "
                  << "[" << std::setw(8) << std::left << alert.severity << "] "
                  << "[" << std::setw(14) << std::left << alert.sensor_name << "] "
                  << alert.message << std::endl;
        out_file_.flush();
    }
}

void Logger::logMessage(const std::string& severity, const std::string& tag, const std::string& msg)
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (!out_file_.is_open()) {
        out_file_.open(file_path_, std::ios::out | std::ios::app);
    }
    if (out_file_.is_open()) {
        out_file_ << "[" << formatTimestamp(std::chrono::system_clock::now()) << "] "
                  << "[" << std::setw(8) << std::left << severity << "] "
                  << "[" << std::setw(14) << std::left << tag << "] "
                  << msg << std::endl;
        out_file_.flush();
    }
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (out_file_.is_open()) {
        out_file_.flush();
    }
}
