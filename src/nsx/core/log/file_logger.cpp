// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/log/file_logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <utility>

namespace nsx::core::log {

namespace {

const char* levelToString(LogLevel level) noexcept
{
    switch (level) {
        case LogLevel::Debug:
            return "[DEBUG]";
        case LogLevel::Info:
            return "[INFO] ";
        case LogLevel::Warn:
            return "[WARN] ";
        case LogLevel::Error:
            return "[ERROR]";
    }
    return "[INFO] ";
}

}  // namespace

FileLogger::FileLogger(std::string path) : m_path(std::move(path)) {}

void FileLogger::log(LogLevel level, std::string_view message)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream out(m_path, std::ios::app);
    if (!out.is_open()) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &timeT);
#else
    localtime_r(&timeT, &tmBuf);
#endif

    out << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << " " << levelToString(level) << " "
        << message << "\n";
}

void FileLogger::info(std::string_view message)
{
    log(LogLevel::Info, message);
}

void FileLogger::warn(std::string_view message)
{
    log(LogLevel::Warn, message);
}

void FileLogger::error(std::string_view message)
{
    log(LogLevel::Error, message);
}

void FileLogger::debug(std::string_view message)
{
    log(LogLevel::Debug, message);
}

const std::string& FileLogger::path() const noexcept
{
    return m_path;
}

}  // namespace nsx::core::log
