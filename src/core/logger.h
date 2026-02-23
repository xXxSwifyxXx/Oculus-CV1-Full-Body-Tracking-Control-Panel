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
