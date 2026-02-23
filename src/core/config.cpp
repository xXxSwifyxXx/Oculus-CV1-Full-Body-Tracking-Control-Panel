/*
 * File: src/core/config.cpp
 *
 * Purpose:
 *   Implements JSON-like config parsing/persistence and command-line override handling with compatibility fallbacks.
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
#include "core/config.h"

#include "core/oculus_discovery.h"
#include "core/path_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

// Whitespace trimming helper used by CLI and config parsing. Keeping this
// local avoids exporting utility behavior that is only meaningful in this file.
std::string trim(const std::string& value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    auto begin = std::find_if_not(value.begin(), value.end(), is_space);
    auto end = std::find_if_not(value.rbegin(), value.rend(), is_space).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

// Minimal string extraction for flat JSON fields.
// This parser is intentionally simple because config schema is tiny and stable.
std::string extract_json_string(const std::string& text, const std::string& key, const std::string& fallback) {
    const std::string pattern = "\"" + key + "\"";
    const auto key_pos = text.find(pattern);
    if (key_pos == std::string::npos) {
        return fallback;
    }
    const auto colon = text.find(':', key_pos + pattern.size());
    if (colon == std::string::npos) {
        return fallback;
    }
    const auto open_quote = text.find('"', colon + 1);
    if (open_quote == std::string::npos) {
        return fallback;
    }
    const auto close_quote = text.find('"', open_quote + 1);
    if (close_quote == std::string::npos) {
        return fallback;
    }
    return text.substr(open_quote + 1, close_quote - open_quote - 1);
}

// Parses integer fields with defensive fallback behavior.
int extract_json_int(const std::string& text, const std::string& key, int fallback) {
    const std::string pattern = "\"" + key + "\"";
    const auto key_pos = text.find(pattern);
    if (key_pos == std::string::npos) {
        return fallback;
    }
    const auto colon = text.find(':', key_pos + pattern.size());
    if (colon == std::string::npos) {
        return fallback;
    }

    auto cursor = colon + 1;
    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
        ++cursor;
    }
    std::string number;
    while (cursor < text.size() && (std::isdigit(static_cast<unsigned char>(text[cursor])) != 0 || text[cursor] == '-')) {
        number.push_back(text[cursor]);
        ++cursor;
    }
    if (number.empty()) {
        return fallback;
    }
    return std::stoi(number);
}

// Parses boolean fields while preserving defaults on malformed content.
bool extract_json_bool(const std::string& text, const std::string& key, bool fallback) {
    const std::string pattern = "\"" + key + "\"";
    const auto key_pos = text.find(pattern);
    if (key_pos == std::string::npos) {
        return fallback;
    }
    const auto colon = text.find(':', key_pos + pattern.size());
    if (colon == std::string::npos) {
        return fallback;
    }

    auto cursor = colon + 1;
    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
        ++cursor;
    }

    if (text.compare(cursor, 4, "true") == 0) {
        return true;
    }
    if (text.compare(cursor, 5, "false") == 0) {
        return false;
    }
    return fallback;
}

} // namespace

namespace odtkra {

ConfigStore::ConfigStore(std::string path) : path_(std::move(path)) {}

const std::string& ConfigStore::path() const {
    return path_;
}

AppConfig ConfigStore::load() const {
    // Missing config file is not an error: defaults represent first-run state.
    std::ifstream input(path_, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    // Parse known keys. Unknown keys are ignored by design to tolerate forward
    // additions and keep older binaries resilient.
    AppConfig cfg;
    cfg.oculusDiagnosticsPath = extract_json_string(text, "oculusDiagnosticsPath", cfg.oculusDiagnosticsPath);
    cfg.oculusClientPath = extract_json_string(text, "oculusClientPath", cfg.oculusClientPath);
    cfg.steamVrDriversPath = extract_json_string(text, "steamVrDriversPath", cfg.steamVrDriversPath);
    cfg.ovrpsTrackingUrl = extract_json_string(text, "ovrpsTrackingUrl", cfg.ovrpsTrackingUrl);
    cfg.spaceCalibratorUrl = extract_json_string(text, "spaceCalibratorUrl", cfg.spaceCalibratorUrl);
    cfg.refreshMinutes = extract_json_int(text, "refreshMinutes", cfg.refreshMinutes);
    cfg.enableCliTuning = extract_json_bool(text, "enableCliTuning", cfg.enableCliTuning);
    cfg.dryRun = extract_json_bool(text, "dryRun", cfg.dryRun);
    // Compatibility path migration: if the historical Oculus path is configured
    // but missing, transparently switch to the Meta Horizon default.
    const std::string legacy_path = "C:\\Program Files\\Oculus\\Support\\oculus-diagnostics";
    const std::string meta_path = "C:\\Program Files\\Meta Horizon\\Support\\oculus-diagnostics";
    if (cfg.oculusDiagnosticsPath == legacy_path && !path_exists(cfg.oculusDiagnosticsPath) && path_exists(meta_path)) {
        cfg.oculusDiagnosticsPath = meta_path;
    }
    // If critical paths are invalid, try auto-discovery so users do not have
    // to manually edit config for common install layouts.
    if (!path_exists(cfg.oculusDiagnosticsPath) || !path_exists(join_path(cfg.oculusDiagnosticsPath, "OculusDebugToolCLI.exe")) || !path_exists(cfg.oculusClientPath)) {
        const auto detected = detect_oculus_paths();
        if (!detected.diagnosticsDir.empty()) {
            cfg.oculusDiagnosticsPath = detected.diagnosticsDir;
        }
        if (!detected.oculusClientExe.empty()) {
            cfg.oculusClientPath = detected.oculusClientExe;
        }
    }
    // Normalize old placeholder URLs to the expected Space Calibrator endpoint.
    if (cfg.ovrpsTrackingUrl == "https://store.steampowered.com/") {
        cfg.ovrpsTrackingUrl = "https://store.steampowered.com/app/3368750/Space_Calibrator/";
    }

    return cfg;
}

bool ConfigStore::save(const AppConfig& config) const {
    // Ensure directory exists before opening stream for truncation.
    const auto slash = path_.find_last_of("\\/");
    if (slash != std::string::npos) {
        ensure_directory(path_.substr(0, slash));
    }

    std::ofstream output(path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    // Write a deterministic and human-readable config file so support/debug
    // sessions can inspect and edit values without tooling.
    output << "{\n";
    output << "  \"oculusDiagnosticsPath\": \"" << config.oculusDiagnosticsPath << "\",\n";
    output << "  \"oculusClientPath\": \"" << config.oculusClientPath << "\",\n";
    output << "  \"steamVrDriversPath\": \"" << config.steamVrDriversPath << "\",\n";
    output << "  \"ovrpsTrackingUrl\": \"" << config.ovrpsTrackingUrl << "\",\n";
    output << "  \"spaceCalibratorUrl\": \"" << config.spaceCalibratorUrl << "\",\n";
    output << "  \"refreshMinutes\": " << config.refreshMinutes << ",\n";
    output << "  \"enableCliTuning\": " << (config.enableCliTuning ? "true" : "false") << ",\n";
    output << "  \"dryRun\": " << (config.dryRun ? "true" : "false") << "\n";
    output << "}\n";

    return output.good();
}

AppConfig apply_cli_args(int argc, char* argv[], const AppConfig& base) {
    AppConfig cfg = base;

    // CLI overrides intentionally apply in-order. Last value wins, which allows
    // wrappers/scripts to append overrides without editing earlier arguments.
    for (int i = 1; i < argc; ++i) {
        const std::string arg = trim(argv[i]);
        if (arg == "--path" && i + 1 < argc) {
            cfg.oculusDiagnosticsPath = trim(argv[++i]);
            if (!cfg.oculusDiagnosticsPath.empty() && cfg.oculusDiagnosticsPath.back() == '"') {
                cfg.oculusDiagnosticsPath.pop_back();
            }
            if (!cfg.oculusDiagnosticsPath.empty() && cfg.oculusDiagnosticsPath.front() == '"') {
                cfg.oculusDiagnosticsPath.erase(cfg.oculusDiagnosticsPath.begin());
            }
        } else if (arg == "--steamvr-drivers" && i + 1 < argc) {
            cfg.steamVrDriversPath = trim(argv[++i]);
        } else if (arg == "--oculus-client" && i + 1 < argc) {
            cfg.oculusClientPath = trim(argv[++i]);
        } else if (arg == "--dry-run") {
            cfg.dryRun = true;
        } else if (arg == "--refresh-min" && i + 1 < argc) {
            cfg.refreshMinutes = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--ovrps-url" && i + 1 < argc) {
            cfg.ovrpsTrackingUrl = trim(argv[++i]);
        } else if (arg == "--spacecal-url" && i + 1 < argc) {
            cfg.spaceCalibratorUrl = trim(argv[++i]);
        }
    }

    return cfg;
}

} // namespace odtkra
