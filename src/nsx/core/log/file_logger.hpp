// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
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

/// @brief Thread-safe structured disk and console logger.
/// @details Formats output as `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [ThreadID] Message`,
///          printing to stdout and appending to the target path.
///          Supports rotation/size capping and gracefully handles missing/unmounted storage.
class FileLogger
{
public:
    static constexpr const char* kDefaultPath = "sdmc:/switch/nsx-manager/nsx.log";
    static constexpr std::size_t kDefaultMaxSize = 1024 * 1024;  // 1 MB

    /// @brief Construct a logger pointing to target path.
    /// @param path The log file destination path.
    /// @param maxSizeBytes Maximum log file size before rotation/capping.
    explicit FileLogger(std::string path = kDefaultPath,
                        std::size_t maxSizeBytes = kDefaultMaxSize);

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

    /// @brief Retrieve configured maximum file size in bytes.
    [[nodiscard]] std::size_t maxSizeBytes() const noexcept;

private:
    std::string m_path;
    std::size_t m_maxSizeBytes;
    std::mutex m_mutex;

    void rotateIfNeededLocked();
};

}  // namespace nsx::core::log
