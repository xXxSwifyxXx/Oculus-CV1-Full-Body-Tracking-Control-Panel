/*
 * File: src/core/diagnostics.h
 *
 * Purpose:
 *   Declares diagnostics report structures consumed by CLI and UI layers.
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

#include <string>
#include <vector>

namespace odtkra {

struct DiagnosticItem {
    std::string name;
    bool ok;
    std::string detail;
    std::string remediation;
};

struct DiagnosticReport {
    bool success = false;
    std::vector<DiagnosticItem> items;

    std::string summary_text() const;
};

class Diagnostics {
public:
    DiagnosticReport run(const AppConfig& config, bool dry_run) const;
};

}
