/*
 * File: src/core/anti_sleep_service.h
 *
 * Purpose:
 *   Declares the anti-sleep service API and ownership model used by the headless agent.
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

#include <atomic>
#include <string>

namespace odtkra {

class AntiSleepService {
public:
    AntiSleepService(AppConfig config, Logger* logger, std::string state_path);

    int run_forever();
    bool run_once(bool dry_run);

private:
    bool apply_cli_profile();
    bool write_state(bool running, bool active, const std::string& message) const;
    bool cli_exists() const;

    AppConfig config_;
    Logger* logger_;
    std::string state_path_;
    std::atomic<bool> stop_requested_;
};

}
