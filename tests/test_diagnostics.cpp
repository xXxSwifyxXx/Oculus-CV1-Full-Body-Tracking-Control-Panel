/*
 * File: tests/test_diagnostics.cpp
 *
 * Purpose:
 *   Covers diagnostics report generation to ensure expected checks are always present in summaries.
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
#include "test_utils.h"

TEST_CASE(diagnostics_report_contains_core_checks) {
    odtkra::AppConfig cfg;
    cfg.oculusDiagnosticsPath = "C:\\unlikely-odt-path";
    cfg.steamVrDriversPath = "C:\\unlikely-steamvr-drivers";

    odtkra::Diagnostics diagnostics;
    const auto report = diagnostics.run(cfg, true);

    REQUIRE(!report.items.empty());
    REQUIRE(report.items.size() >= 4);

    const auto summary = report.summary_text();
    REQUIRE(summary.find("Diagnostic verdict") != std::string::npos);
}
