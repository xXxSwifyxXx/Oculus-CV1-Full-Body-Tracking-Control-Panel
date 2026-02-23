/*
 * File: src/core/logger.h
 *
 * Purpose:
 *   Declares logger API and log level semantics.
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
#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace odtkra {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

class Logger {
public:
    explicit Logger(std::string path);

    void write(LogLevel level, const std::string& message);
    std::string path() const;

private:
    std::string level_to_string(LogLevel level) const;
    std::string timestamp_now() const;

    std::string path_;
    mutable std::mutex mutex_;
    std::ofstream stream_;
};

}
