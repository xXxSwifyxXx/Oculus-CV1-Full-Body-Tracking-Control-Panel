/*
 * File: src/core/config.h
 *
 * Purpose:
 *   Declares persistent application configuration fields and the config store interface.
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

#include <string>

namespace odtkra {

struct AppConfig {
    std::string oculusDiagnosticsPath = "C:\\Program Files\\Meta Horizon\\Support\\oculus-diagnostics";
    std::string oculusClientPath = "C:\\Program Files\\Meta\\Support\\oculus-client\\OculusClient.exe";
    std::string steamVrDriversPath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\drivers";
    std::string ovrpsTrackingUrl = "https://store.steampowered.com/app/3368750/Space_Calibrator/";
    std::string spaceCalibratorUrl = "https://store.steampowered.com/app/3368750/Space_Calibrator/";
    int refreshMinutes = 9;
    bool enableCliTuning = true;
    bool dryRun = false;
};

class ConfigStore {
public:
    explicit ConfigStore(std::string path);

    const std::string& path() const;
    AppConfig load() const;
    bool save(const AppConfig& config) const;

private:
    std::string path_;
};

AppConfig apply_cli_args(int argc, char* argv[], const AppConfig& base);

}
