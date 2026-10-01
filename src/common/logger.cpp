/*
 * ProcessPilot Pro - High Performance Structured Logger Implementation
 * File: src/common/logger.cpp
 */

#include "common/logger.hpp"
#include <chrono>
#include <iomanip>
#include <ctime>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace process_pilot {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void Logger::init(LogLevel level, const std::string& log_file) {
    PlatformLockGuard lock(log_mutex_);
    current_level_ = level;
    if (!log_file.empty()) {
        file_stream_.open(log_file, std::ios::out | std::ios::app);
    }
#ifndef _WIN32
    use_colors_ = isatty(fileno(stdout));
#else
    use_colors_ = true;
#endif
}

void Logger::set_level(LogLevel level) {
    PlatformLockGuard lock(log_mutex_);
    current_level_ = level;
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::LOG_TRACE: return "TRACE";
        case LogLevel::LOG_DEBUG: return "DEBUG";
        case LogLevel::LOG_INFO:  return "INFO ";
        case LogLevel::LOG_WARN:  return "WARN ";
        case LogLevel::LOG_ERROR: return "ERROR";
        case LogLevel::LOG_FATAL: return "FATAL";
        default:                  return "LOG  ";
    }
}

std::string Logger::level_to_color(LogLevel level) {
    if (!use_colors_) return "";
    switch (level) {
        case LogLevel::LOG_TRACE: return "\033[90m";   // Gray
        case LogLevel::LOG_DEBUG: return "\033[36m";   // Cyan
        case LogLevel::LOG_INFO:  return "\033[32m";   // Green
        case LogLevel::LOG_WARN:  return "\033[33m";   // Yellow
        case LogLevel::LOG_ERROR: return "\033[31m";   // Red
        case LogLevel::LOG_FATAL: return "\033[1;31m"; // Bold Red
        default:                  return "\033[0m";
    }
}

std::string Logger::format_string(const std::string& fmt) {
    return fmt;
}

void Logger::log(LogLevel level, const char* file, int line, const std::string& message) {
    if (level < current_level_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#if defined(_MSC_VER)
    localtime_s(&tm_buf, &time_t_now);
#elif defined(_WIN32)
    std::tm* tmp = std::localtime(&time_t_now);
    if (tmp) tm_buf = *tmp;
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    // Extract filename basename
    std::string file_str(file);
    size_t last_slash = file_str.find_last_of("/\\");
    std::string base_file = (last_slash != std::string::npos) ? file_str.substr(last_slash + 1) : file_str;

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count()
       << " [" << level_to_string(level) << "] "
       << "[" << base_file << ":" << line << "] "
       << message;

    std::string log_line = ss.str();

    PlatformLockGuard lock(log_mutex_);
    if (use_colors_) {
        std::cout << level_to_color(level) << log_line << "\033[0m" << std::endl;
    } else {
        std::cout << log_line << std::endl;
    }

    if (file_stream_.is_open()) {
        file_stream_ << log_line << std::endl;
    }
}

} // namespace process_pilot
