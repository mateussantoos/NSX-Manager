// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <mutex>
#include <string>
#include <string_view>

namespace nsx::core::log {

/// @brief Severity level for file logging.
enum class LogLevel
{
    Debug,
    Info,
    Warn,
    Error
};

/// @brief Minimal thread-safe file logger.
/// @details Appends log records with timestamps and severity to a target file.
///          Default path: "sdmc:/switch/nsx-manager/nsx.log".
class FileLogger
{
public:
    static constexpr const char* kDefaultPath = "sdmc:/switch/nsx-manager/nsx.log";

    /// @brief Construct a logger pointing to target path.
    /// @param path The log file destination path.
    explicit FileLogger(std::string path = kDefaultPath);

    ~FileLogger() = default;

    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;
    FileLogger(FileLogger&&) = delete;
    FileLogger& operator=(FileLogger&&) = delete;

    /// @brief Log a formatted message thread-safely.
    /// @param level Severity level.
    /// @param message Text to record.
    void log(LogLevel level, std::string_view message);

    /// @brief Convenience helper for info messages.
    /// @param message Text to record.
    void info(std::string_view message);

    /// @brief Convenience helper for warning messages.
    /// @param message Text to record.
    void warn(std::string_view message);

    /// @brief Convenience helper for error messages.
    /// @param message Text to record.
    void error(std::string_view message);

    /// @brief Convenience helper for debug messages.
    /// @param message Text to record.
    void debug(std::string_view message);

    /// @brief Retrieve the configured log file path.
    /// @return Configured path string reference.
    [[nodiscard]] const std::string& path() const noexcept;

private:
    std::string m_path;
    std::mutex m_mutex;
};

}  // namespace nsx::core::log
