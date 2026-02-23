/*
 * File: src/core/diagnostics.cpp
 *
 * Purpose:
 *   Implements dry-run and runtime diagnostics checks for Oculus runtime, SteamVR runtime, permissions, and driver presence.
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
#include "core/diagnostics.h"

#include "core/oculus_discovery.h"
#include "core/path_utils.h"
#include "core/process_utils.h"
#include "core/steam_discovery.h"

#include <sstream>
#include <fstream>
#include <cstdio>
#include <filesystem>

namespace odtkra {
namespace {

// Recursively checks whether a SteamVR driver tree contains a driver manifest.
// We intentionally avoid hard-coding exact file names to stay resilient if
// upstream packaging changes while keeping manifest extension semantics.
bool has_manifest_recursive(const std::string& root, std::string* found_manifest = nullptr) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.size() >= 15 && name.find(".vrdrivermanifest") != std::string::npos) {
            if (found_manifest != nullptr) {
                *found_manifest = entry.path().string();
            }
            return true;
        }
    }
    return false;
}

} // namespace

std::string DiagnosticReport::summary_text() const {
    // Human-readable summary used by both CLI output and UI dialog display.
    std::ostringstream out;
    out << (success ? "Diagnostic verdict: PASS" : "Diagnostic verdict: FAIL") << "\n";
    for (const auto& item : items) {
        out << "- [" << (item.ok ? "OK" : "FAIL") << "] " << item.name << ": " << item.detail;
        if (!item.ok && !item.remediation.empty()) {
            out << " | Action: " << item.remediation;
        }
        out << "\n";
    }
    return out.str();
}

DiagnosticReport Diagnostics::run(const AppConfig& config, bool dry_run) const {
    DiagnosticReport report;

    // Run discovery once and re-use results so diagnostics are consistent.
    const auto oculus = detect_oculus_paths();

    // 1) Discovery-level checks (best-effort environment scan)
    report.items.push_back({
        "Detected Oculus Debug CLI",
        !oculus.oculusDebugToolCliExe.empty(),
        oculus.oculusDebugToolCliExe.empty() ? "Not detected under Meta Horizon/Meta/Oculus folders" : oculus.oculusDebugToolCliExe,
        "Verify Meta Horizon desktop app installation"
    });

    report.items.push_back({
        "Detected Oculus Client",
        !oculus.oculusClientExe.empty(),
        oculus.oculusClientExe.empty() ? "Not detected under Meta Horizon/Meta/Oculus folders" : oculus.oculusClientExe,
        "Install Meta Horizon desktop app"
    });

    // 2) Config-level checks (what this app is currently configured to use)
    const std::string cli = join_path(config.oculusDiagnosticsPath, "OculusDebugToolCLI.exe");
    report.items.push_back({
        "Oculus Debug Tool CLI",
        path_exists(cli),
        path_exists(cli) ? "CLI found" : "Missing OculusDebugToolCLI.exe",
        "Set --path to Oculus diagnostics directory"
    });

    report.items.push_back({
        "Meta/Oculus Client",
        path_exists(config.oculusClientPath),
        path_exists(config.oculusClientPath) ? "Oculus client found" : ("Client not found: " + config.oculusClientPath),
        "Install Meta Horizon/Oculus desktop app or set --oculus-client"
    });

    // 3) Runtime process checks (is runtime currently alive?)
    const int oculus_pid = find_process_id(L"OVRServer_x64.exe");
    report.items.push_back({
        "Oculus Runtime",
        oculus_pid != 0,
        oculus_pid != 0 ? "OVRServer_x64.exe detected" : "Oculus runtime not running",
        "Start Oculus app/runtime before session"
    });

    const int vrserver_pid = find_process_id(L"vrserver.exe");
    report.items.push_back({
        "SteamVR Runtime",
        vrserver_pid != 0,
        vrserver_pid != 0 ? "vrserver.exe detected" : "SteamVR not running",
        "Launch SteamVR to validate driver runtime"
    });

    // 4) SteamVR path and permission checks for installer operations.
    auto discovery = detect_steamvr();
    const std::string drivers_dir = path_exists(config.steamVrDriversPath) ? config.steamVrDriversPath : discovery.steamVrDrivers;
    report.items.push_back({
        "SteamVR drivers path",
        path_exists(drivers_dir),
        path_exists(drivers_dir) ? ("Detected path: " + drivers_dir) : ("Path not found: " + drivers_dir),
        "Install/repair SteamVR or set --steamvr-drivers"
    });

    const std::string write_probe = join_path(drivers_dir, "odtkra_write_probe.tmp");
    bool writable = false;
    if (path_exists(drivers_dir)) {
        std::ofstream probe(write_probe, std::ios::out | std::ios::trunc);
        writable = probe.is_open();
        probe.close();
        if (writable) {
            std::remove(write_probe.c_str());
        }
    }
    report.items.push_back({
        "SteamVR drivers permissions",
        writable,
        writable ? "Write permission OK" : "No write permission",
        "Run elevated for driver install, or choose writable Steam library path"
    });

    // 5) Driver integrity checks.
    const std::string touch_link_root = join_path(drivers_dir, "OculusTouchLink");
    std::string manifest_found;
    const bool has_manifest = has_manifest_recursive(touch_link_root, &manifest_found);
    report.items.push_back({
        "OculusTouchLink Driver",
        has_manifest,
        has_manifest ? ("manifest found: " + manifest_found) : ("No *.vrdrivermanifest under " + touch_link_root),
        "Run install/update of OculusTouchLink"
    });

    // 6) Optional active command check (disabled in dry-run mode by default).
    if (!dry_run && path_exists(cli)) {
        std::string output;
        const int code = run_command("echo service set-pixels-per-display-pixel-override 0.99 | \"" + cli + "\"", &output);
        report.items.push_back({
            "CLI execution",
            code == 0,
            code == 0 ? "Command applied" : "CLI command failed (code " + std::to_string(code) + ")",
            "Verify Oculus service and permissions"
        });
    } else {
        report.items.push_back({
            "CLI execution",
            true,
            "Skipped in dry-run mode",
            ""
        });
    }

    // Final verdict: fail fast if any item failed.
    report.success = true;
    for (const auto& item : report.items) {
        if (!item.ok) {
            report.success = false;
            break;
        }
    }
    return report;
}

}
