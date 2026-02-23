/*
 * File: src/core/logger.cpp
 *
 * Purpose:
 *   Implements thread-safe file logging with timestamps and stable formatting used across the project.
 *
 * Design Notes:
 *   - This file is part of the production-grade refactor where responsibilities are intentionally split.
 *   - The intent is to keep logic predictable, observable, and recoverable under partial-runtime scenarios.
 *   - Error paths are expected in real user environments (missing Oculus runtime, missing SteamVR, permissions).
 *
 * Maintenance Guidance:
 *   - Keep behavior deterministic and avoid hidden side effects.
 *   - Prefer explicit logging and explicit return values over implicit assumptions.
 *   - If a change alters runtime behavior, update tests and diagnostics messaging in the same change.
 */
#include "core/logger.h"

#include "core/path_utils.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace odtkra {

Logger::Logger(std::string path) : path_(std::move(path)) {
    const auto slash = path_.find_last_of("\\/");
    if (slash != std::string::npos) {
        ensure_directory(path_.substr(0, slash));
    }
    stream_.open(path_, std::ios::out | std::ios::app);
}

void Logger::write(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open()) {
        return;
    }

    stream_ << timestamp_now() << " [" << level_to_string(level) << "] " << message << "\n";
    stream_.flush();
}

std::string Logger::path() const {
    return path_;
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "INFO";
}

std::string Logger::timestamp_now() const {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local{};
    localtime_s(&tm_local, &t);

    std::ostringstream out;
    out << std::put_time(&tm_local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

}
