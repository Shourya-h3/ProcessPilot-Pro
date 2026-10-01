/*
 * ProcessPilot Pro - High Performance Structured Logger
 * File: include/common/logger.hpp
 */

#ifndef PROCESS_PILOT_LOGGER_HPP
#define PROCESS_PILOT_LOGGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(_WIN32) && !defined(__MINGW64_VERSION_MAJOR)
#include <windows.h>
namespace process_pilot {
class PlatformMutex {
public:
    PlatformMutex() { InitializeCriticalSection(&cs_); }
    ~PlatformMutex() { DeleteCriticalSection(&cs_); }
    void lock() { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }
private:
    CRITICAL_SECTION cs_;
};
class PlatformLockGuard {
public:
    PlatformLockGuard(PlatformMutex& m) : m_(m) { m_.lock(); }
    ~PlatformLockGuard() { m_.unlock(); }
private:
    PlatformMutex& m_;
};
}
#else
#include <mutex>
namespace process_pilot {
using PlatformMutex = std::mutex;
using PlatformLockGuard = std::lock_guard<std::mutex>;
}
#endif

namespace process_pilot {

enum class LogLevel {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

class Logger {
public:
    static Logger& instance();

    void init(LogLevel level, const std::string& log_file = "");
    void log(LogLevel level, const char* file, int line, const std::string& message);
    void set_level(LogLevel level);

    template<typename... Args>
    void log_fmt(LogLevel level, const char* file, int line, const std::string& fmt, Args&&... args) {
        log(level, file, line, format_string(fmt, std::forward<Args>(args)...));
    }

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string level_to_string(LogLevel level);
    std::string level_to_color(LogLevel level);
    std::string format_string(const std::string& fmt);

    template<typename T, typename... Rest>
    std::string format_string(const std::string& fmt, T&& value, Rest&&... rest) {
        std::ostringstream oss;
        size_t pos = fmt.find("{}");
        if (pos != std::string::npos) {
            oss << fmt.substr(0, pos) << value;
            std::string remaining = fmt.substr(pos + 2);
            oss << format_string(remaining, std::forward<Rest>(rest)...);
            return oss.str();
        }
        return fmt;
    }

    LogLevel current_level_ = LogLevel::LOG_INFO;
    std::ofstream file_stream_;
    PlatformMutex log_mutex_;
    bool use_colors_ = true;
};

} // namespace process_pilot

#define LOG_TRACE(msg) process_pilot::Logger::instance().log(process_pilot::LogLevel::LOG_TRACE, __FILE__, __LINE__, msg)
#define LOG_DEBUG(msg) process_pilot::Logger::instance().log(process_pilot::LogLevel::LOG_DEBUG, __FILE__, __LINE__, msg)
#define LOG_INFO(msg)  process_pilot::Logger::instance().log(process_pilot::LogLevel::LOG_INFO,  __FILE__, __LINE__, msg)
#define LOG_WARN(msg)  process_pilot::Logger::instance().log(process_pilot::LogLevel::LOG_WARN,  __FILE__, __LINE__, msg)
#define LOG_ERROR(msg) process_pilot::Logger::instance().log(process_pilot::LogLevel::LOG_ERROR, __FILE__, __LINE__, msg)
#define LOG_FATAL(msg) process_pilot::Logger::instance().log(process_pilot::LogLevel::LOG_FATAL, __FILE__, __LINE__, msg)

#endif // PROCESS_PILOT_LOGGER_HPP
