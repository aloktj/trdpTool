#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace trdp
{

inline std::string timeStamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

inline void log(const std::string &level, const std::string &message)
{
    std::cout << "[" << timeStamp() << "] " << level << ": " << message << std::endl;
}

inline void info(const std::string &message) { log("INFO", message); }
inline void warn(const std::string &message) { log("WARN", message); }
inline void error(const std::string &message) { log("ERROR", message); }

} // namespace trdp

