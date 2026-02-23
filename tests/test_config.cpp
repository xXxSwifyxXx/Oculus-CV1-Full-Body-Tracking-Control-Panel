/*
 * File: tests/test_config.cpp
 *
 * Purpose:
 *   Covers config persistence and command-line override behavior to prevent regressions in startup configuration logic.
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
#include "core/path_utils.h"
#include "test_utils.h"

#include <cstdio>

TEST_CASE(config_save_and_load_roundtrip) {
    const std::string dir = odtkra::join_path(".", "tmp-tests");
    odtkra::ensure_directory(dir);
    const std::string path = odtkra::join_path(dir, "config.json");

    odtkra::ConfigStore store(path);
    odtkra::AppConfig cfg;
    cfg.oculusDiagnosticsPath = "C:\\Oculus\\Diag";
    cfg.refreshMinutes = 5;
    cfg.dryRun = true;

    REQUIRE(store.save(cfg));

    const auto loaded = store.load();
    REQUIRE(loaded.refreshMinutes == 5);
    REQUIRE(loaded.dryRun == true);
    REQUIRE(!loaded.oculusDiagnosticsPath.empty());

    std::remove(path.c_str());
}

TEST_CASE(cli_override_path_and_dryrun) {
    odtkra::AppConfig base;

    char arg0[] = "odtkra_agent.exe";
    char arg1[] = "--path";
    char arg2[] = "C:\\Custom";
    char arg3[] = "--dry-run";
    char* argv[] = {arg0, arg1, arg2, arg3};

    auto cfg = odtkra::apply_cli_args(4, argv, base);

    REQUIRE(cfg.oculusDiagnosticsPath == "C:\\Custom");
    REQUIRE(cfg.dryRun);
}
