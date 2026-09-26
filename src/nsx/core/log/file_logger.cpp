// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/log/file_logger.hpp"

#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>

namespace nsx::core::log {

namespace {

const char* levelToString(LogLevel level) noexcept
{
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "INFO";
}

}  // namespace

FileLogger::FileLogger(std::string path, std::size_t maxSizeBytes)
    : m_path(std::move(path)), m_maxSizeBytes(maxSizeBytes)
{
}

void FileLogger::rotateIfNeededLocked()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (m_maxSizeBytes > 0 && fs::exists(m_path, ec)) {
        const auto size = fs::file_size(m_path, ec);
        if (!ec && size >= m_maxSizeBytes) {
            const fs::path backup = m_path + ".1";
            fs::remove(backup, ec);
            fs::rename(m_path, backup, ec);
        }
    }
}

void FileLogger::log(LogLevel level, std::string_view message)
{
    const std::lock_guard<std::mutex> lock(m_mutex);

    const auto now = std::chrono::system_clock::now();
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &timeT);
#else
    localtime_r(&timeT, &tmBuf);
#endif

    std::stringstream tidStream;
    tidStream << std::this_thread::get_id();
    const std::string tidStr = tidStream.str();

    std::array<char, 128> lineHeader{};
    const int written = std::snprintf(
        lineHeader.data(), lineHeader.size(), "[%04d-%02d-%02d %02d:%02d:%02d.%03lld] [%s] [%s] ",
        tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday, tmBuf.tm_hour, tmBuf.tm_min,
        tmBuf.tm_sec, static_cast<long long>(ms.count()), levelToString(level), tidStr.c_str());
    (void)written;

    // 1. Output to console stdout
    std::cout << lineHeader.data() << message << "\n";
    std::cout.flush();

    // 2. Check rotation and append to log file
    rotateIfNeededLocked();

    std::ofstream out(m_path, std::ios::app);
    if (out.is_open()) {
        out << lineHeader.data() << message << "\n";
        out.flush();
    }
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

std::size_t FileLogger::maxSizeBytes() const noexcept
{
    return m_maxSizeBytes;
}

}  // namespace nsx::core::log
