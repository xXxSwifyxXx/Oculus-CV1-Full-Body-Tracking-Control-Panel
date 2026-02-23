/*
 * File: src/core/touchlink_installer.h
 *
 * Purpose:
 *   Declares TouchLink installer contract and result model for CLI/UI usage.
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

#include "core/config.h"
#include "core/logger.h"

#include <string>
#include <vector>

namespace odtkra {

struct TouchLinkInstallResult {
    bool success = false;
    std::string message;
    std::string installed_path;
};

class TouchLinkInstaller {
public:
    explicit TouchLinkInstaller(Logger* logger);

    TouchLinkInstallResult install_or_update(const AppConfig& config, const std::string& steamvr_drivers_path = "");

private:
    std::string resolve_target_path(const AppConfig& config, const std::string& steamvr_drivers_path) const;
    bool backup_existing(const std::string& target) const;
    bool verify_driver_files(const std::string& driver_root, std::string* missing_file) const;
    std::vector<std::string> list_top_level_entries(const std::string& path) const;

    Logger* logger_;
};

}
