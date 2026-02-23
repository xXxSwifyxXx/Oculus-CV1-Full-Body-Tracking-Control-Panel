/*
 * File: apps/agent/main.cpp
 *
 * Purpose:
 *   Headless runtime entrypoint that orchestrates config loading, path discovery, diagnostics mode, one-shot mode, and service loop execution.
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
#include "core/anti_sleep_service.h"
#include "core/config.h"
#include "core/diagnostics.h"
#include "core/logger.h"
#include "core/oculus_discovery.h"
#include "core/path_utils.h"
#include "core/steam_discovery.h"
#include "core/touchlink_installer.h"

#include <iostream>

namespace {

// The runtime stores all mutable data under ProgramData so both CLI and UI can
// read/write shared state without relying on install-directory permissions.
std::string data_root() {
    const std::string base = odtkra::join_path(odtkra::get_program_data_dir(), "ODTKRA");
    odtkra::ensure_directory(base);
    return base;
}

// Tiny helper used for mode flags where we do not need value parsing.
bool has_arg(int argc, char* argv[], const std::string& needle) {
    for (int i = 1; i < argc; ++i) {
        if (needle == argv[i]) {
            return true;
        }
    }
    return false;
}

}

int main(int argc, char* argv[]) {
    // Build canonical file locations once so every runtime path is explicit.
    const std::string root = data_root();
    const std::string config_path = odtkra::join_path(root, "config.json");
    const std::string log_path = odtkra::join_path(root, "odtkra.log");
    const std::string state_path = odtkra::join_path(root, "state.json");

    // Load persisted config first, then allow CLI to override for this run.
    odtkra::ConfigStore store(config_path);
    auto config = store.load();
    config = odtkra::apply_cli_args(argc, argv, config);

    // Self-healing discovery pass: if configured paths are stale, attempt to
    // auto-resolve valid Oculus install paths so first-run friction is lower.
    if (!odtkra::path_exists(odtkra::join_path(config.oculusDiagnosticsPath, "OculusDebugToolCLI.exe")) || !odtkra::path_exists(config.oculusClientPath)) {
        const auto oculus = odtkra::detect_oculus_paths();
        if (!oculus.diagnosticsDir.empty()) {
            config.oculusDiagnosticsPath = oculus.diagnosticsDir;
        }
        if (!oculus.oculusClientExe.empty()) {
            config.oculusClientPath = oculus.oculusClientExe;
        }
    }
    if (!odtkra::path_exists(config.steamVrDriversPath)) {
        const auto discovery = odtkra::detect_steamvr();
        config.steamVrDriversPath = discovery.steamVrDrivers;
    }

    // Persist the reconciled config so UI/next runs observe the same values.
    store.save(config);

    odtkra::Logger logger(log_path);
    logger.write(odtkra::LogLevel::Info, "ODTKRA agent booting");

    // Diagnostics mode is always dry-run by design: it validates system state
    // without mutating runtime settings.
    if (has_arg(argc, argv, "--diagnostics")) {
        odtkra::Diagnostics diagnostics;
        const auto report = diagnostics.run(config, true);
        std::cout << report.summary_text();
        logger.write(report.success ? odtkra::LogLevel::Info : odtkra::LogLevel::Warning, "Diagnostics executed in dry-run mode");
        return report.success ? 0 : 2;
    }

    // Installer mode is isolated so the control panel can call one binary with
    // a simple flag and receive an exit code + message.
    if (has_arg(argc, argv, "--install-touchlink")) {
        odtkra::TouchLinkInstaller installer(&logger);
        const auto result = installer.install_or_update(config);
        std::cout << result.message << std::endl;
        return result.success ? 0 : 3;
    }

    // Service execution modes:
    // - --once: single refresh attempt, useful for scripting/diagnostics
    // - dryRun config: do not apply runtime command, only validate flow
    // - default: long-running periodic refresh loop
    odtkra::AntiSleepService service(config, &logger, state_path);
    if (has_arg(argc, argv, "--once") || config.dryRun) {
        return service.run_once(config.dryRun) ? 0 : 1;
    }

    return service.run_forever();
}
